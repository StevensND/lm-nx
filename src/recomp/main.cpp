// Entry point of the recompiled build: loads the DOL from the user's disc into
// guest memory, boots it on a guest thread, and runs the Aurora host loop.
#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/event.h>
#include <aurora/main.h>
#ifdef __SWITCH__
#include "pgo_switch.hpp"
#include <lmnx/perfil_vivo.hpp>
#endif
#include <aurora/present_timing.h>
#include <dolphin/dvd.h>
#include <dolphin/gx/GXAurora.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_video.h>

#include "port/crash_handler.h"
#include "port/dol.h"
#include "port/install.h"
#include "port/options.h"
#include "port/recovery.h"
#include "port/settings.h"
#include "port/update.h"
#include "recomp/audio.h"
#include "recomp/cpu_qos.h"
#include "recomp/mem_report.h"
#include "recomp/frame.h"
#include "recomp/gx_bridge.h"
#include "recomp/menu.h"
#include "recomp/pacing.h"
#include "recomp/hle.h"
#include "recomp/interp.h"
#include "recomp/module_loader.h"
#include "recomp/precache.h"
#include "recomp/runtime.h"
#include "recomp/sched.h"
#include "recomp/ui_scale.h"
#include "recomp/widescreen.h"
#ifndef LM_STATIC_MODULE
#include "setup/setup.h"
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>

#if defined(_WIN32)
// PC diagnostic marker; does not change AOT execution or rendering.
extern "C" void lmnx_diagnostico_estado(bool fotogramaDeJuego);
#endif
#ifdef __SWITCH__
bool lmnx_vi_ajustar();  // platforms/switch/vi_ajuste.cpp: false if the program was asked to close meanwhile
bool lmnx_vi_valida();   // the start-up table read is the one the build was made from (true without a table)
void lmnx_vi_aviso();    // tells the player why the build cannot start
bool lmnx_aviso_mostrar();  // platforms/switch/aviso.cpp: the disclaimer; false if asked to close meanwhile
#endif

namespace recomp {
u32 copyDispCount();  // hle_gx.cpp
#ifdef __APPLE__
void hideWindowMacos(SDL_Window* w);  // macos_window.cpp
#endif
#ifdef LM_STATIC_MODULE
extern const GameModule kGameModule;  // generated func_table.cpp, linked in (lmrecomp)
#ifdef LM_SWITCH_PAL_MODULE
namespace pal { extern const GameModule kGameModule; }
#endif
#endif
namespace vi {
void startTimer();
void stopTimer();
}  // namespace vi


namespace {

void loadDol(const u8* dol) {
    auto be = [&](u32 off) { return u32(dol[off]) << 24 | u32(dol[off + 1]) << 16 | u32(dol[off + 2]) << 8 | dol[off + 3]; };
    // .bss overlaps the small-data sections, so clear it before copying them in.
    std::memset(hostp(be(0xD8)), 0, be(0xDC));
    for (int i = 0; i < 18; i++) {
        const u32 off = be(i * 4), addr = be(0x48 + i * 4), size = be(0x90 + i * 4);
        if (size) std::memcpy(hostp(addr), dol + off, size);
    }
}

// Reads the `lis rD, hi; ori rD, rD, lo` pairs of __init_registers (r1, r2, r13).
void initRegisters(Ctx& c) {
    const u32 start = guestSymbol("__init_registers");
    for (u32 pc = start; pc < start + 0x18; pc += 8) {
        const u32 hi = rd32(pc), lo = rd32(pc + 4);
        const u32 reg = (hi >> 21) & 31;
        c.r[reg] = (hi << 16) | (lo & 0xFFFF);
    }
}

void boot() {
    Ctx init;
    initRegisters(init);
    const u32 stackTop = init.r[1];
    init.r[1] = stackTop - 8;
    wr32(init.r[1], 0);

    const u32 defaultThread = hleAlloc(0x310);
    GuestThread* t = sched::adoptBootThread(defaultThread, init);
    wr32(defaultThread + 0x304, stackTop);
    wr32(defaultThread + 0x308, stackTop - 0x10000);
    const u32 irqStack = hleAlloc(0x10000);
    sched::setIrqContext(init, irqStack + 0x10000 - 8);

    Ctx& c = sched::ctxOf(t);
    guestCall(c, guestSymbol("OSInit"));
    vi::startTimer();
    guestCall(c, guestSymbol("__init_user"));
    log("[boot] entering main");
    guestCall(c, guestSymbol("main"), 0, 0);
    log("[boot] main returned %d", s32(c.r[3]));
}

bool sNoGpuAdapter = false;  // Aurora found no usable graphics adapter (no Vulkan/D3D driver)

void logCallback(AuroraLogLevel level, const char* module, const char* message, unsigned int) {
    static const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
#ifdef __SWITCH__
    // Console logs write synchronously to the SD card. Disable DEBUG to avoid excessive FIFO-thread writes; flush
    // only warnings and errors.
    if (level < LOG_INFO) return;
    std::fprintf(level >= LOG_ERROR ? stderr : stdout, "[%s: %s] %s\n", names[level], module, message);
    if (level >= LOG_WARNING) std::fflush(stdout);
#else
    std::fprintf(level >= LOG_ERROR ? stderr : stdout, "[%s: %s] %s\n", names[level], module, message);
    std::fflush(stdout);
#endif
    if (port::recovery::runLog()) {
        const std::string line = std::string("[") + names[level] + ": " + module + "] " + message;
        port::recovery::logLine(line.c_str(), line.size());
    }
    if (level == LOG_FATAL) port::recovery::noteFatal(message);
    if (level == LOG_ERROR && std::strstr(message, "Failed to create adapter")) sNoGpuAdapter = true;
    if (level == LOG_FATAL) {
        std::fflush(stderr);
        // Without any usable GPU Aurora falls back to its null device and stops on the first
        // allocation; tell the player what is missing instead of only crashing.
        std::string text = std::string("The game stopped with an error:\n\n") + message +
                           "\n\nThe log of this run (last_run.log) is in the folder of settings.ini; please "
                           "attach it to a bug report. If the game no longer starts, the next start offers "
                           "to reset the settings or rebuild the game data.";
        if (sNoGpuAdapter) {
#ifdef _WIN32
            text = "No usable graphics adapter was found (Direct3D 12, Direct3D 11 or Vulkan).\n\n"
                   "Update your graphics driver.";
#else
            text = "No usable Vulkan graphics driver was found.\n\n"
                   "Install the Vulkan driver for your GPU (AMD/Intel: Mesa's Vulkan drivers, e.g. the "
                   "mesa-vulkan-drivers package; NVIDIA: the proprietary driver) and check it with "
                   "'vulkaninfo --summary'. See README.txt.";
#endif
        }
        if (!std::getenv("LM_BACKGROUND"))  // automated runs must not block on a dialog
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Luigi's Mansion Recompiled", text.c_str(), nullptr);
        std::abort();
    }
}

#ifdef LM_STATIC_MODULE
std::string findDisc(const char* gameId) {
    if (!port::options().disc.empty()) return port::options().disc;
#ifdef __SWITCH__
    const auto candidate = std::filesystem::path("sdmc:/switch/LM-NX/orig") / (std::string(gameId) + ".iso");
    return std::filesystem::exists(candidate) ? candidate.string() : std::string{};
#else
    for (auto dir = std::filesystem::current_path();; dir = dir.parent_path()) {
        const auto candidate = dir / "orig" / (std::string(gameId) + ".iso");
        if (std::filesystem::exists(candidate)) return candidate.string();
        if (dir == dir.parent_path()) return {};
    }
#endif
}
#else
bool hasArg(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; i++)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

std::string argValue(int argc, char** argv, const char* name) {
    for (int i = 1; i + 1 < argc; i++)
        if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
    return {};
}
#endif

// Hang reports (port/crash_handler.h): whether the game itself kept running and what Aurora's
// threads are doing, so a log tells a stopped game from a stopped display (issue #11: on a Steam
// Deck the game kept running at the file select while nothing was presented any more).
std::atomic<u32> sFramesAtStall{0};
std::atomic<bool> sStallOpen{false};
void hangDetail(char* out, size_t size, bool stallBegins) {
    const u32 frames = frame::guestFrames();
    if (stallBegins) {
        sFramesAtStall.store(frames, std::memory_order_relaxed);
        sStallOpen.store(true, std::memory_order_relaxed);
        return;
    }
    char act[400] = "";
    aurora_describe_activity(act, sizeof(act), 1000);
    char since[64] = "";
    if (sStallOpen.load(std::memory_order_relaxed))
        std::snprintf(since, sizeof(since), ", %u since the main thread stopped", frames - sFramesAtStall.load(std::memory_order_relaxed));
    std::snprintf(out, size, "game frames %u%s (%s)%s%s", frames, since,
                  frame::interpolating() ? "interpolated presentation" : "lockstep", act[0] ? "; aurora: " : "", act);
}

}  // namespace
}  // namespace recomp

int main(int argc, char* argv[]) {
    using namespace recomp;
    // Windows: no console window for players; output to a terminal, a redirect or last_run.log.
    port::recovery::setupConsole(argc, argv);
    port::installCrashHandler();
    port::setHangDetail(&hangDetail);
    port::startHangWatchdog();
    port::parseOptions(argc, argv);
    port::setLaunchArgs(argc, argv);
#if defined(__SWITCH__) || defined(LMNX_PC_TEST)
    std::printf("ProjectLM-NX (base PC 1.1.2)\n");
#else
    std::printf("Luigi's Mansion Recompiled %s\n", port::update::buildVersion());
#endif
    std::fflush(stdout);
    // A game folder too deep for the graphics start-up (docs/SETUP.md "Long paths"): say so
    // instead of stopping without a word.
    if (const std::string p = port::installPathProblem(); !p.empty()) {
        std::fprintf(stderr, "%s\n", p.c_str());
        if (!std::getenv("LM_BACKGROUND"))
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Luigi's Mansion Recompiled", p.c_str(), nullptr);
        return 1;
    }
    // Portable or system folders (docs/SETUP.md "Portable mode"), before any file is opened: a
    // data move the F1 menu asked for runs here.
#if !defined(__SWITCH__) && !defined(LMNX_PC_TEST)
    port::dataLocation();
#endif
    // Windows: last_run.log (recovery.h), in the folder dataLocation just chose (portable or system).
    port::recovery::openRunLog();

    AuroraConfig config{};
#if defined(__SWITCH__) || defined(LMNX_PC_TEST)
    config.appName = "ProjectLM-NX";
#else
    config.appName = "Luigi's Mansion Recompiled";
#endif
    config.logCallback = &logCallback;
    port::loadSettings();
    // Start-up recovery (docs/SETUP.md "When the game does not start"): after a start that failed,
    // offers to reset the settings, rebuild the game data or put the saves aside.
#ifdef LM_STATIC_MODULE
    port::recovery::onStart(false);
#else
    port::recovery::onStart(true);
#endif
    // Updates (docs/SETUP.md "Updates"): remove what a finished update left, then look for a newer
    // release in the background (never blocks; the F1 menu shows the result).
#if !defined(__SWITCH__) && !defined(LMNX_PC_TEST)
    port::update::cleanupAfterUpdate();
    port::update::startupCheck();
    // A data folder too deep for the files Aurora writes with plain paths (docs/SETUP.md "Long paths").
    if (const std::string p = port::dataPathProblem(); !p.empty()) {
        std::fprintf(stderr, "%s\n", p.c_str());
        if (!std::getenv("LM_BACKGROUND"))  // automated runs must not block on a dialog
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Luigi's Mansion Recompiled", p.c_str(), nullptr);
    }
#endif
    config.msaa = u32(port::settings().video.msaa);
    config.vsync = port::settings().video.vsync;
#if !defined(__SWITCH__) && !defined(LMNX_PC_VULKAN)
    // Under Gamescope (Steam Deck game mode) the swap chain always waits for the refresh (FIFO).
    // Gamescope composites every frame, so VSync off (Mailbox there) never tore and only lowered
    // latency, and interpolated presentation with Mailbox stopped showing frames on a Steam Deck
    // while the game ran on (issue #11). LM_GAMESCOPE_VSYNC=0 keeps the setting (test aid).
    if (!config.vsync && pacing::gamescope()) {
        const char* keep = std::getenv("LM_GAMESCOPE_VSYNC");
        if (!(keep && *keep == '0')) {
            config.vsync = true;
            std::printf("[video] Gamescope: VSync on (FIFO presentation; the VSync setting is ignored under Gamescope)\n");
        }
    }
    // LM_BACKEND=d3d12|d3d11|vulkan (testing): graphics backend instead of Aurora's choice.
    if (const char* b = std::getenv("LM_BACKEND")) {
        const std::string s = b;
        config.desiredBackend = s == "d3d12"    ? BACKEND_D3D12
                                : s == "d3d11"  ? BACKEND_D3D11
                                : s == "vulkan" ? BACKEND_VULKAN
                                : s == "opengl" ? BACKEND_OPENGL
                                : s == "gles"   ? BACKEND_OPENGLES
                                                : BACKEND_AUTO;
    }
    // LM_ALLOW_CPU_GPU=1: accept a software renderer (e.g. Mesa llvmpipe for Vulkan in WSL or a
    // VM without GPU passthrough; docs/LINUX.md). Slow, for testing only.
    if (const char* c = std::getenv("LM_ALLOW_CPU_GPU"); c && *c == '1') config.allowCpuAdapter = true;
#else
    // Switch selects the native backend exclusively, with no fallback.
    config.allowCpuAdapter = false;
#endif
    // LM_CACHE_DIR selects a separate shader/pipeline cache. It defaults to the settings.ini directory, so an
    // alternate APPDATA also gets a separate cache; Aurora's SDL_GetPrefPath ignores APPDATA. Linux uses
    // $XDG_CACHE_HOME/Luigi's Mansion Recompiled (port::cacheDir; docs/LINUX.md). Aurora needs UTF-8 paths;
    // path::string() uses the Windows ANSI code page and can fail for non-ASCII user names.
    const auto utf8 = [](const std::filesystem::path& p) {
        const std::u8string s = p.u8string();
        return std::string(reinterpret_cast<const char*>(s.data()), s.size());
    };
    static const std::string sCacheDir = utf8(port::cacheDir());
    config.cachePath = sCacheDir.c_str();
    // Memory cards and imgui.ini (Aurora's userPath) in dataDir, which honours XDG_DATA_HOME /
    // APPDATA like the rest. On Windows this is the same folder SDL_GetPrefPath gives for players,
    // but SDL ignores the APPDATA variable, so test runs with a scratch APPDATA used to write
    // imgui.ini into the real install.
    // LM_CARD_DIR (test runs) is the user path too: Aurora's CARDInit builds the card paths from
    // userPath whenever one is set, so CARDSetBasePath alone no longer moved the cards.
    static const std::string sUserDir =
        std::getenv("LM_CARD_DIR") ? utf8(port::envPath("LM_CARD_DIR")) + "/" : utf8(port::dataDir()) + "/";
    config.userPath = sUserDir.c_str();
    static const std::string sCacheOverride = std::getenv("LM_CACHE_DIR") ? utf8(port::envPath("LM_CACHE_DIR")) : "";
    if (!sCacheOverride.empty()) config.cachePath = sCacheOverride.c_str();
    const port::Options& opts = port::options();
    config.startFullscreen = opts.fullscreen || port::settings().video.fullscreen;
    config.windowWidth = opts.windowWidth;
    config.windowHeight = opts.windowHeight;
    if (!config.windowWidth && opts.aspectRatio > 0.0f) {  // fixed aspect: open a window of that shape
        config.windowHeight = 960;
        config.windowWidth = u32(960.0f * opts.aspectRatio + 0.5f);
    }
    precache::configure();
    port::setPhase("graphics initialization (device, render targets, anti-aliasing)");
    // LM_TOPMOST=1 (testing: screen-capture judder runs, tools/interp/screen_run.py): the window
    // stays on top and a fullscreen window is not minimized when another window takes the focus.
    const bool topmost = std::getenv("LM_TOPMOST") != nullptr;
    if (topmost) SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    cpu::init();  // thread policy + Aurora thread hooks (issue #18), before Aurora starts its threads
    aurora_initialize(argc, argv, &config);
    mem::trackInit();  // LM_MEM_TRACK (test aid, docs/RENDERING.md "Memory use over time")
    if (topmost)
        if (SDL_Window* w = aurora_get_window()) SDL_SetWindowAlwaysOnTop(w, true);
    // LM_OFFSCREEN=1 (testing: automated runs on a machine someone is using): the window is moved far
    // outside every display, so it renders and presents as usual but is never seen (a hidden window
    // pauses the game; Start-Process -WindowStyle Hidden does not hide SDL's window).
    if (std::getenv("LM_OFFSCREEN"))
        if (SDL_Window* w = aurora_get_window()) {
#ifdef __APPLE__
            // macOS keeps windows on a display whatever position is asked for: fully transparent
            // and click-through instead (it still renders and presents; docs/MACOS.md).
            hideWindowMacos(w);
#else
            SDL_SetWindowPosition(w, -32000, -32000);
#endif
        }
    // LM_MAXIMIZE=1 (testing): open the window maximized, as players often run it.
    if (std::getenv("LM_MAXIMIZE"))
        if (SDL_Window* w = aurora_get_window()) SDL_MaximizeWindow(w);
    precache::init();
#ifdef __SWITCH__
    // The menu's fonts and scale (ui_scale.cpp) from the first frame: the start-up notice below uses its font.
    // (menu::init installs it again later; a second call only changes nothing.)
    recomp::ui::install(recomp::ui::Context::Game);
    // PORT (ProjectLM-NX): first display frames (platforms/switch/vi_ajuste.cpp); the shaders load meanwhile.
    if (!lmnx_vi_ajustar()) {
        port::recovery::onExit();
        aurora_shutdown();
        std::_Exit(0);
    }
    // A build whose start-up table was removed or replaced stops here, saying so on screen (vi_ajuste.cpp).
    if (!lmnx_vi_valida()) {
        lmnx_vi_aviso();
        port::recovery::onExit();
        aurora_shutdown();
        std::_Exit(0);
    }
    // The disclaimer (community port, based on rob646464's work, not for sale) before the shader screen.
    if (!lmnx_aviso_mostrar()) {
        port::recovery::onExit();
        aurora_shutdown();
        std::_Exit(0);
    }
#endif
    ws::init();
    port::setPhase("loading the game");
    gx::installFifoErrorReport();

#ifdef LM_STATIC_MODULE
    // lmrecomp (developer build): the game was recompiled at build time and linked in.
    const GameModule* selectedModule = &kGameModule;
#ifdef LM_SWITCH_PAL_MODULE
    port::loadInstall();
    if (port::install().base == port::GameVersion::PAL) selectedModule = &pal::kGameModule;
#endif
    setGameModule(selectedModule);
    const char* wantId = selectedModule->gameId;
    const std::string disc = findDisc(wantId);
    {
        auto& inst = port::install();
        inst.staticBuild = true;
        inst.running = std::strcmp(wantId, "GLMP01") == 0 ? port::GameVersion::PAL : port::GameVersion::USA;
        inst.usReady = inst.running == port::GameVersion::USA;
        inst.palReady = inst.running == port::GameVersion::PAL;
#ifdef LM_SWITCH_PAL_MODULE
        inst.usDisc = findDisc("GLME01");
        inst.palDisc = findDisc("GLMP01");
        inst.usReady = !inst.usDisc.empty();
        inst.palReady = !inst.palDisc.empty();
#endif
#if defined(__SWITCH__) || defined(LMNX_PC_TEST)
        inst.hmOverlayReady = std::filesystem::is_regular_file(port::hiddenMansionOverlayDir() / "manifest.txt");
#else
        inst.hmOverlayReady = true;  // dev: orig/hidden_mansion (tools/hidden_mansion_data.py), both directions
#endif
    }
#else
    // LuigisMansionRecomp: set up from the user's discs (first run, or after changes).
    setup::Options so;
    so.forceUi = hasArg(argc, argv, "--setup");
    so.autoStart = hasArg(argc, argv, "--setup-auto");
    so.usDisc = argValue(argc, argv, "--us-disc");
    so.palDisc = argValue(argc, argv, "--pal-disc");
    setup::LaunchInfo launch;
    if (!setup::run(so, launch)) {
        port::recovery::onExit();
        aurora_shutdown();
        std::_Exit(0);
    }
    const std::string disc = launch.disc;
    const char* wantId = port::gameId(launch.version);
#endif
    port::setPhase("loading the game");
    if (disc.empty() || !aurora_dvd_open(disc.c_str())) {
        std::fprintf(stderr, "Could not open a Luigi's Mansion disc image. Pass --disc <path to .iso/.rvz>.\n");
        aurora_shutdown();
        return 1;
    }
    // Disc header: game ID (bytes 0-5) and revision (byte 7). Then main.dol: code and layout must be retail.
    const DVDDiskID* diskId = DVDGetCurrentDiskID();
    const std::string discId = std::string(diskId->gameName, 4) + std::string(diskId->company, 2);
    if (discId != wantId || diskId->gameVersion != 0) {
        std::fprintf(stderr, "The game was recompiled from Luigi's Mansion %s rev 0; the disc is %s rev %d.\n", wantId,
                     discId.c_str(), diskId->gameVersion);
        aurora_shutdown();
        return 1;
    }
    const port::dol::Version discVersion = port::dol::load();
    const port::dol::Version wantVersion =
        std::strcmp(wantId, "GLMP01") == 0 ? port::dol::Version::GLMP01 : port::dol::Version::GLME01;
    if (discVersion != wantVersion) {
        std::fprintf(stderr, "The disc's main.dol changes code or section layout. Only data sections may change.\n");
        aurora_shutdown();
        return 1;
    }
    s32 dolSize = 0;
    const u8* dol = DVDGetDOLLocation(&dolSize);

    menu::init();
#ifdef __SWITCH__
    g_mem = g_memStorage;  // a fixed array on the console (ctx.h, memBase)
#else
    g_mem = allocGuestMemory();
#endif
    loadDol(dol);
    if (port::install().running == port::GameVersion::PAL) {
        // The PAL release's language (.sdata 0x804BD658: 1 English, 2 German, 3 French,
        // 4 Spanish, 5 Italian; docs/SETUP.md). The game reads it when it loads text.
        const int lang = port::settings().enhancements.palLanguage;
        wr32(0x804BD658, u32(lang >= 1 && lang <= 5 ? lang : 1));
    }
    initTimeBase();
    initTrace();
    initWatch();
#ifndef LM_STATIC_MODULE
    if (std::string err; !loadGameModule(launch.module, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        aurora_shutdown();
        return 1;
    }
#endif
    std::printf("Opened %s (%s)\n", disc.c_str(), discId.c_str());

    frame::start(&boot);
    if (std::getenv("LM_WATCHDOG")) {
        auto trabajador = std::thread([] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
#ifdef __SWITCH__
                if (sched::stopping()) break;
#endif
                sched::dumpThreads();
                if (const char* peek = std::getenv("LM_PEEK")) {  // LM_PEEK=addr,addr,...: guest words
                    std::string line = "[peek]";
                    for (const char* p = peek; *p;) {
                        char* end = nullptr;
                        const u32 a = u32(std::strtoul(p, &end, 16));
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), " %08X=%08X", a, rd32(a));
                        line += buf;
                        p = (*end == ',') ? end + 1 : end;
                        if (end == p && *p) break;
                    }
                    log("%s", line.c_str());
                }
            }
        });
#ifdef __SWITCH__
        sched::ownBackground(std::move(trabajador));
#else
        trabajador.detach();
#endif
    }

    bool exiting = false;
    bool paused = false;
    // Test aid LM_QUIT_AFTER=<seconds>: a normal quit after that long (the shader and pipeline
    // caches are written as on a user's exit; a killed test run loses what it compiled).
    const double quitAfter = [] {
        const char* v = std::getenv("LM_QUIT_AFTER");
        return v ? std::atof(v) : 0.0;
    }();
    const auto runStart = std::chrono::steady_clock::now();
    port::setPhase("game loop");
    bool overlapPolicyReady = false;
    // Stalls the watchdog cannot see because this loop keeps running (port::reportStall): no frame
    // presented (aurora_begin_frame failing), or with interpolation the game thread no longer
    // finishing frames while the display replays its last one.
    using SteadyClock = std::chrono::steady_clock;
    auto lastPresented = SteadyClock::now();
    auto lastGuestProgress = SteadyClock::now();
    auto lastIteration = SteadyClock::now();
    u32 lastGuestFrames = frame::guestFrames();
    bool presentStallReported = false, guestStallReported = false;
    while (!exiting) {
        // dev_tools events may write MEM1. Wait for the released thread to park before processing them, including while
        // paused or recompiling shaders.
#ifdef __SWITCH__
        const auto inicioEsperaSolape = std::chrono::steady_clock::now();
#endif
        frame::finishGuestCpuOverlap();
#ifdef __SWITCH__
        const auto esperaSolape = std::chrono::steady_clock::now() - inicioEsperaSolape;
#endif
        port::heartbeat();
        sStallOpen.store(false, std::memory_order_relaxed);
        if (!paused) port::setPhase("game loop: window events (aurora_update)");  // (paused: nullptr, a wait on purpose)
        if (quitAfter > 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count() > quitAfter)
            break;
        for (const AuroraEvent* ev = aurora_update(); ev && ev->type != AURORA_NONE; ++ev) {
            if (ev->type == AURORA_SDL_EVENT) menu::handleEvent(ev->sdl);
            if (ev->type == AURORA_EXIT) exiting = true;
            else if (ev->type == AURORA_PAUSED) {
                paused = true;
                port::setPhase(nullptr);  // waits for the window to come back: not a hang
                audio::outSetPaused(true);
            } else if (ev->type == AURORA_UNPAUSED) {
                paused = false;
                port::setPhase("game loop");
                audio::outSetPaused(false);
            }
        }
        // Shaders first (precache.h holdGame): at startup the game does not start, and after a
        // settings change that needs other shader variants it waits, until they are compiled.
        const bool hold = !exiting && precache::holdGame();
#if defined(_WIN32)
        lmnx_diagnostico_estado(!exiting && !paused && !hold);
#endif
        static bool sHoldAudio = false;
        if (hold != sHoldAudio) {
            sHoldAudio = hold;
            if (!paused) audio::outSetPaused(hold);
        }
        // Test aid LM_FREEZE_AT=<frame>: from that game frame (copyDispCount) on, the guest is held
        // as for a focus loss while the host keeps presenting its last tape with the post-process
        // chain (A/B screenshots of one game frame, e.g. CRT signals with LM_CRT_CYCLE). Needs
        // interpolated presentation (frame rate above 30).
        static const u32 sFreezeAt = [] {
            const char* v = std::getenv("LM_FREEZE_AT");
            return v ? u32(std::strtoul(v, nullptr, 10)) : 0u;
        }();
        const bool frozen = sFreezeAt && copyDispCount() >= sFreezeAt;
        frame::setHostPaused(paused || hold || frozen);
        if (hold || paused) overlapPolicyReady = false;
        {
            const auto now = SteadyClock::now();
            // (A gap between iterations is a stall of this thread itself, which the watchdog reports.)
            if (paused || now - lastIteration > std::chrono::seconds(1)) {
                lastPresented = now;
                lastGuestProgress = now;
            }
            lastIteration = now;
            if (const u32 g = frame::guestFrames(); g != lastGuestFrames || paused || hold || frozen || !frame::interpolating()) {
                lastGuestFrames = g;
                lastGuestProgress = now;
                if (guestStallReported) log("[hang] the game finishes frames again");
                guestStallReported = false;
            } else if (!guestStallReported && now - lastGuestProgress > std::chrono::seconds(15)) {
                guestStallReported = true;
                port::reportStall("the game has not finished a frame for 15 s while the display loop runs (interpolated "
                                  "presentation replays its last frame)",
                                  true);
                sched::dumpThreads();
            }
            if (!presentStallReported && now - lastPresented > std::chrono::seconds(10)) {
                presentStallReported = true;
                port::reportStall("the main loop runs but no frame could be begun for 10 s (aurora_begin_frame fails)", true);
            }
        }
        if (exiting || paused) continue;
        if (hold) {
            port::setPhase("game loop: shader preparation frame");
            const auto beginAt = SteadyClock::now();
            if (!aurora_begin_frame()) continue;
            const double beginMs = std::chrono::duration<double, std::milli>(SteadyClock::now() - beginAt).count();
            lastPresented = SteadyClock::now();
            presentStallReported = false;
            menu::draw();  // applies settings changes (they may change what is being compiled)
            precache::drawHold();
            precache::frame();
            aurora_end_frame();
            // Hold frames well below the display's refresh rate (issue #33): a swap chain image is
            // then always free, so the render worker never waits for one inside Dawn, where the wait
            // holds the device lock every pipeline creation needs.
            precache::holdFramePause(beginMs);
            if (menu::quitRequested()) break;
            continue;
        }
#ifdef __SWITCH__
        // Main-loop frame timing, logged to the SD card every five seconds.
        using RelojFase = std::chrono::steady_clock;
        static double sFase[6] = {}; static uint64_t sVueltas = 0; static auto sDesde = RelojFase::now();
        // Include the early wait so the log accounts for game CPU time.
        const auto t0Fase = RelojFase::now() - (frame::overlapGuestCpu() ? esperaSolape : RelojFase::duration::zero());
#endif
        const bool overlap = frame::overlapGuestCpu();
        // (Phases name the step in a hang report: issue #11.)
        port::setPhase("game loop: frame pacing wait");
        pacing::beforeFrame();
        interp::hostMark(0);
        port::setPhase("game loop: aurora_begin_frame (frame slot, staging buffer)");
        if (!aurora_begin_frame()) continue;
        lastPresented = SteadyClock::now();
        if (presentStallReported) log("[hang] frames are presented again");
        presentStallReported = false;
        pacing::afterBegin();
        interp::hostMark(1);
        // The first iteration has no policy prepared by the preceding frame.
        if (!overlap || !overlapPolicyReady) ws::beginFrame();
        overlapPolicyReady = false;
        port::setPhase(frame::interpolating() ? "game loop: replaying a game frame" : "game loop: running the game (lockstep)");
#ifdef __SWITCH__
        const auto t1Fase = RelojFase::now();
#endif
        const bool running = frame::hostFrame();
        pacing::afterReplay();
        interp::hostMark(2);
#ifdef __SWITCH__
        const auto t2Fase = RelojFase::now();
#endif
        port::setPhase("game loop: F1 menu / overlays");
        menu::draw();
        if (overlap) {
            ws::beginFrame();
            overlapPolicyReady = true;
        }
#ifdef __SWITCH__
        const auto t2aFase = RelojFase::now();
#endif
        precache::frame();
        port::setPhase("game loop: present hold");
        pacing::beforeEnd();
#ifdef __SWITCH__
        const auto t2bFase = RelojFase::now();
#endif
        if (overlap && running && !menu::quitRequested() && !frame::interpolating()) {
            // Consume arrays, textures and lists still pointing to MEM1 before the guest CPU can reuse that memory.
            AuroraGXSync();
            frame::resumeGuestCpu();
        }
        port::setPhase("game loop: aurora_end_frame (GX FIFO drain, hand-off to the render worker)");
        aurora_end_frame();
        port::setPhase("game loop");
        pacing::afterFrame();
#ifdef __SWITCH__
        lmnx_pgo_tick();  // PGO training only: periodic dump of the counters (empty otherwise; same CFG in GENERATE/USE)
#endif
        interp::hostMark(3);
#ifdef __SWITCH__
        const auto t3Fase = RelojFase::now();
#endif
        frame::pace();
        interp::hostMark(4);
#ifdef __SWITCH__
        {
            const auto t4Fase = RelojFase::now();
            const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
            sFase[0] += ms(t0Fase, t1Fase); sFase[1] += ms(t1Fase, t2Fase); sFase[2] += ms(t2Fase, t3Fase); sFase[3] += ms(t3Fase, t4Fase);
            sFase[4] += ms(t2Fase, t2aFase); sFase[5] += ms(t2bFase, t3Fase);
            if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {  // panel de rendimiento
                lmnx::perfil::guardar(lmnx::perfil::HostMenu, ms(t2Fase, t2aFase));
                lmnx::perfil::guardar(lmnx::perfil::HostFin, ms(t2bFase, t3Fase));
            }
            ++sVueltas;
            if (std::chrono::duration<double>(t4Fase - sDesde).count() >= 5.0) {
                const double n = double(sVueltas);
                std::fprintf(stderr, "[bucle principal] ms per frame: begin=%.1f game(hostFrame)=%.1f end=%.1f "
                                     "(menu %.1f, aurora_end_frame %.1f) pacing=%.1f\n",
                             sFase[0] / n, sFase[1] / n, sFase[2] / n, sFase[4] / n, sFase[5] / n, sFase[3] / n);
                for (double& f : sFase) f = 0;
                sVueltas = 0; sDesde = t4Fase;
            }
        }
#endif
        if (!running || menu::quitRequested()) break;
    }

    port::setPhase("shutdown (releasing graphics objects, writing caches)");
    frame::finishGuestCpuOverlap();
    menu::shutdown();
    vi::stopTimer();
    frame::stop();
    audio::outShutdown();
    aurora_shutdown();
#ifdef __SWITCH__
    lmnx_pgo_volcar();
#endif
    port::recovery::onExit();
    std::_Exit(0);
}
