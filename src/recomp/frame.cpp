#include "recomp/frame.h"

#include "port/recovery.h"
#include "port/settings.h"
#include "recomp/cpu_qos.h"
#include "recomp/gx_bridge.h"
#include "recomp/hidden_mansion.h"
#include "recomp/hle.h"
#include "recomp/interp.h"
#include "recomp/model_viewer.h"
#include "recomp/runtime.h"
#include "recomp/sched.h"
#include "recomp/skip.h"

#include <aurora/aurora.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <atomic>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#ifdef __SWITCH__
#include <lmnx/perfil_vivo.hpp>
#include "perfil_hilos.h"
#include "runtime.hpp"
#endif

namespace recomp {
u32 viRetraceCount();
u32 frameStamp();  // hle_vi.cpp: the frame's time stamp in NTSC fields
}

namespace recomp::frame {
// PORT (ProjectLM-NX), read by the inline fast path of gxWaitForGuestTurn (frame.h), once per GX call.
std::atomic<bool> detail::hostOwnsFrame{false};
std::atomic<bool> detail::gxBusy{false};

namespace {

enum class Turn { Host, Guest };
std::mutex sM;
std::condition_variable sCv;
Turn sTurn = Turn::Host;
bool sFinished = false;
bool sStopping = false;
// Set while the guest waits in present() for the host to finish the frame (lockstep). The
// waiting thread gives up the guest CPU meanwhile (sched::releaseForHost), so interrupts and
// other guest threads keep running as on the console; GX calls wait for the guest's turn
// (gxWaitForGuestTurn), since the host is using the Aurora frame.
std::atomic<bool>& sHostOwnsFrame = detail::hostOwnsFrame;
std::atomic<bool>& sGxBusy = detail::gxBusy;  // gxBlockingWait(): a guest thread waits for GX work, such as GXDrawDone. CPU permission is separate from the GX
// turn. After menu preparation and FIFO draining, the CPU advances during frame closing; GX stays closed until
// runUntilPresent(). Protected by sM; enabled only with solapar_cpu.flag.
bool sGuestCpuMayRun = false;
// Under sM; only the thread released by present() confirms that it is parked.
bool sGuestCpuOverlapPending = false;
thread_local bool tGuestCpuOverlap = false;
std::thread sBoot;

// Interpolated presentation (interp.h). The guest is "decoupled": it no longer
// waits for the host in present() but captures each frame into a tape, while the
// host replays tapes at display rate. Entered and left at a guest present.
std::atomic<u32> sPresents{0};
std::atomic<u32> sGuestFrames{0};  // every present() call (hang reports: guestFrames)
bool sEnterDecoupled = false;   // host -> guest, under sM: continue decoupled after this wait
std::atomic<bool> sStayDecoupled{false};  // host -> guest: keep going decoupled at the next present
bool sGuestDecoupled = false;   // guest thread
bool sHostDecoupled = false;    // host thread
constexpr u32 kDecoupleAfter = 60;  // presents; keeps boot (GXInit, first loads) in lockstep

// The model viewer (F4) runs at the game's 30 fps without interpolation: its camera, spin and
// picture fit change per guest frame, and blending its draws (and the game frame under it)
// between tapes only distorts what it shows. Only real frames are presented while it is open;
// the frame-rate setting applies again when it closes.
// Test aid LM_VIEWER_INTERP=1: keep interpolating with the viewer open (the old behaviour).
bool interpolationWanted() {
    static const bool sViewerInterp = [] {
        const char* v = std::getenv("LM_VIEWER_INTERP");
        return v && *v == '1';
    }();
    return targetFps() > port::kMinFps && sPresents.load() >= kDecoupleAfter && (sViewerInterp || !viewer::isOpen());
}

}  // namespace

bool readOverlapFlag();

void start(Entry boot) {
    detail::g_overlapCpu = readOverlapFlag();  // before any guest code runs
    sBoot = std::thread([boot] {
#ifdef __SWITCH__
        lmnx::plataforma::configurarHilo(lmnx::plataforma::Rol::Juego);
        lmnx_perfil_nombrar("game (guest main)");  // panel de rendimiento: «Thread CPU use»
#else
        cpu::setCurrent(cpu::Role::Guest);
#endif
        {
            std::unique_lock lk(sM);
            sCv.wait(lk, [] { return sTurn == Turn::Guest || sStopping; });
        }
        if (!sStopping) {
            try {
                boot();
            } catch (const sched::ThreadExit&) {
            }
        }
        std::lock_guard lk(sM);
        sFinished = true;
        sGuestCpuOverlapPending = false;
        sTurn = Turn::Host;
        sCv.notify_all();
    });
}

bool runUntilPresent() {
    std::unique_lock lk(sM);
    if (sFinished) return false;
    sTurn = Turn::Guest;
    sHostOwnsFrame = false;
    sCv.notify_all();
    sCv.wait(lk, [] { return sTurn == Turn::Host; });
    return !sFinished;
}

bool hostFrame() {
    const bool want = interpolationWanted();
    if (!sHostDecoupled) {
        if (!runUntilPresent()) return false;  // the guest drew this frame directly
        if (want) {
            // Let the guest continue without waiting for the next host frame; from
            // its next frame on it records tapes.
            std::lock_guard lk(sM);
            sStayDecoupled = true;
            sEnterDecoupled = true;
            sTurn = Turn::Guest;
            sHostOwnsFrame = false;
            sCv.notify_all();
            sHostDecoupled = true;
        }
        return true;
    }
    sStayDecoupled = want;
    bool guestBack;
    {
        std::lock_guard lk(sM);
        if (sFinished) return false;
        guestBack = sTurn == Turn::Host;  // left decoupled mode and waits in present()
    }
    if (guestBack) {
        interp::replay(true);  // its last tape, as is
        interp::reset();
        sHostDecoupled = false;
        return true;
    }
    interp::replay();
    return true;
}

float displayRefreshHz() {
    // Followed every half second, so moving the window to another display (or changing
    // its mode) takes effect without a restart.
    using Clock = std::chrono::steady_clock;
    static Clock::time_point checked{};
    static float hz = 0;
    const auto now = Clock::now();
    if (now - checked < std::chrono::milliseconds(500) && checked != Clock::time_point{}) return hz;
    checked = now;
    if (SDL_Window* w = aurora_get_window())
        if (const SDL_DisplayID d = SDL_GetDisplayForWindow(w))
            if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(d)) hz = m->refresh_rate;
    static float logged = -1;
    if (hz != logged) log("[frame] display refresh rate %.2f Hz", logged = hz);
    return hz;
}

int targetFps() {
    const auto& v = port::settings().video;
    int fps = v.targetFps;
    if (v.unlimitedFps) return port::kMaxFps;
    if (v.matchDisplay) {
        const float hz = displayRefreshHz();
        fps = hz > 0 ? int(std::lround(hz)) : 60;
    }
    return std::clamp(fps, port::kMinFps, port::kMaxFps);
}

bool interpolating() { return sHostDecoupled; }
u32 guestFrames() { return sGuestFrames.load(std::memory_order_relaxed); }

void pace() {
    // Interpolated presentation is paced by pacing::beforeFrame (pacing.cpp); lockstep frames are
    // paced by the guest (VI retraces) and the swap chain.
}

void stop() {
    {
        std::lock_guard lk(sM);
        sStopping = true;
        sCv.notify_all();
    }
    sched::shutdown();
    // Let the guest threads unwind (ThreadExit) before the caller tears down Aurora and audio:
    // the one running guest code only leaves at its next present/scheduling point and would
    // otherwise still be calling into GX while it is destroyed. Bounded, in case a thread
    // spins without reaching one.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    {
        std::unique_lock lk(sM);
        sCv.wait_until(lk, deadline, [] { return sFinished; });
    }
    const double left = std::chrono::duration<double>(deadline - std::chrono::steady_clock::now()).count();
    if (!sched::waitForThreads(std::max(left, 0.0))) {
#ifdef __SWITCH__
        fatal("AOT threads have not stopped; aborting before releasing Aurora");
#else
        log("[frame] guest threads still running at exit");
#endif
    }
#ifdef __SWITCH__
    {
        std::lock_guard lk(sM);
        if (!sFinished) fatal("The AOT startup thread has not stopped");
    }
    if (sBoot.joinable()) sBoot.join();
#else
    if (sBoot.joinable()) sBoot.detach();
#endif
}

// While the host is paused (window minimized or unfocused) a decoupled guest
// would keep playing; stopping the VI retraces holds it like lockstep does.
std::atomic<bool> sPauseGuest{false};
void setHostPaused(bool paused) { sPauseGuest = paused && sHostDecoupled; }
bool gamePaused() { return sPauseGuest.load(std::memory_order_relaxed); }

bool detail::g_overlapCpu = false;

// read once by start() (overlapGuestCpu() in frame.h is now a plain load).
bool readOverlapFlag() {
#ifdef __SWITCH__
    FILE* f = std::fopen("sdmc:/switch/LM-NX/solapar_cpu.flag", "rb");
    if (f) std::fclose(f);
    if (f) log("[frame] Game CPU overlaps frame closing (solapar_cpu.flag)");
    return f != nullptr;
#else
    const char* e = std::getenv("LM_OVERLAP_CPU");
    return e && e[0] == '1' && e[1] == '\0';
#endif
}

void resumeGuestCpu() {
    if (!overlapGuestCpu() || sHostDecoupled || sPresents.load() < kDecoupleAfter) return;
    std::lock_guard lk(sM);
    if (sStopping || sFinished || sTurn != Turn::Host || !sHostOwnsFrame.load()) return;
    sGuestCpuMayRun = true;
    sGuestCpuOverlapPending = true;
    sCv.notify_all();
}

void finishGuestCpuOverlap() {
    if (!overlapGuestCpu()) return;
    std::unique_lock lk(sM);
    sCv.wait(lk, [] { return !sGuestCpuOverlapPending || sFinished || sStopping; });
}

void present(Ctx& c, u32, bool) {
    // A guest thread let go early (resumeGuestCpu) must not hand over a second frame before the host opened the next
    // one: like any GX call, wait for the guest's turn first. Only with the overlap on (default: unchanged).
    if (overlapGuestCpu()) gxWaitForGuestTurn();
    cpu::tick();
    // Test aid LM_TEST_GUEST_STALL=<frame>[,<seconds>]: the game thread stops for <seconds> (default
    // 40) at that frame, holding the guest CPU (hang reports: main.cpp, port::reportStall).
    static const std::pair<u32, int> sStall = [] {
        const char* e = std::getenv("LM_TEST_GUEST_STALL");
        if (!e) return std::pair<u32, int>{0, 0};
        const char* comma = std::strchr(e, ',');
        return std::pair<u32, int>{u32(std::strtoul(e, nullptr, 10)), comma ? std::atoi(comma + 1) : 40};
    }();
    if (const u32 n = sGuestFrames.fetch_add(1, std::memory_order_relaxed) + 1; sStall.first && n == sStall.first) {
        log("[test] game thread stalls for %d s at frame %u (LM_TEST_GUEST_STALL)", sStall.second, n);
        std::this_thread::sleep_for(std::chrono::seconds(sStall.second));
    }
    verifyLowMemory();
    gx::endFrame();
    hm::onFrame();
    if (skip::onGuestFrame(c)) return;  // fast-forward: this frame is not shown
    const u32 presents = ++sPresents;
    if (presents == 600) port::recovery::markRunning();  // the start succeeded (port/recovery.h)
    if (presents % 30 == 0 || presents < 60) log("[frame] %u presents, %u retraces", presents, viRetraceCount());
    if (sGuestDecoupled) {
        interp::endTape(frameStamp());
        if (sStopping) throw sched::ThreadExit{};
        if (sStayDecoupled) {
            interp::beginTape();
            return;
        }
        sGuestDecoupled = false;  // back to lockstep: wait for the host below
    }
#ifdef __SWITCH__
    // Performance panel: the game thread handing its frame to the host and waiting for its turn back is this port's
    // frame limiter ("Frame Limiter Wait", with the retrace waits).
    const bool perfMeasure = lmnx::perfil::activo.load(std::memory_order_relaxed);
    const auto perfStart = perfMeasure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
#endif
    std::unique_lock lk(sM);
    sGuestCpuMayRun = false;
    sTurn = Turn::Host;
    sHostOwnsFrame = true;
    sCv.notify_all();
    // On the console VISetNextFrameBuffer returns at once; here the guest waits for the host
    // frame (up to a display refresh). Holding the guest CPU through that wait delayed every
    // interrupt by as much: AI DMA interrupts then arrived in pairs, JAudio's DSP frames ran
    // back to back, its overload check (UpdateDSPchannelAll) stopped a voice each time, and
    // threads like the movie player's decoder starved. A caller with interrupts off (a
    // critical section) keeps the CPU, since no other thread may run in it (from SMS).
    const bool release = sched::interruptsEnabled();
    lk.unlock();
    if (release) sched::releaseForHost();
    lk.lock();
    sCv.wait(lk, [] { return sGuestCpuMayRun || sTurn == Turn::Guest || sStopping; });
    if (sStopping) {
        lk.unlock();
        throw sched::ThreadExit{};
    }
    tGuestCpuOverlap = sGuestCpuMayRun && sTurn == Turn::Host;
    const bool enterDecoupled = sEnterDecoupled;
    if (sEnterDecoupled) {
        sEnterDecoupled = false;
        sGuestDecoupled = true;
    }
    lk.unlock();
#ifdef __SWITCH__
    const auto perfWoken = perfMeasure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
#endif
    if (release) sched::reacquireAfterHost();
#ifdef __SWITCH__
    if (perfMeasure) {
        // Frame Limiter Wait = the whole hand-off; of it, getting the guest CPU back after being woken.
        const auto now = std::chrono::steady_clock::now();
        const auto ns = [](auto d) { return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count()); };
        lmnx::perfil::esperaVblankNs.fetch_add(ns(now - perfStart), std::memory_order_relaxed);
        lmnx::perfil::readquirirNs.fetch_add(ns(now - perfWoken), std::memory_order_relaxed);
    }
#endif
    if (enterDecoupled) interp::beginTape();
}

void detail::gxWaitForGuestTurnSlow() {
    const auto ready = [] { return (sTurn == Turn::Guest && !sGxBusy) || sStopping; };
    {
        std::lock_guard lk(sM);
        if (ready()) return;
    }
#ifdef __SWITCH__
    // Performance panel: game threads waiting for GX to be theirs (with solapar_cpu.flag, the time moved here).
    const bool perfMeasure = lmnx::perfil::activo.load(std::memory_order_relaxed);
    const auto perfStart = perfMeasure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto perfAdd = [&] {
        if (perfMeasure)
            lmnx::perfil::esperaGxNs.fetch_add(
                uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - perfStart)
                             .count()),
                std::memory_order_relaxed);
    };
#endif
    // Wait like a blocked thread (the CPU goes to others meanwhile), unless the caller is in a
    // critical section (interrupts off), where no other thread may run.
    const bool release = sched::interruptsEnabled();
    if (release) sched::releaseForHost();
    {
        std::unique_lock lk(sM);
        // releaseForHost also writes scheduler state in MEM1. Confirm after yielding the CPU and before waiting for the
        // GX turn.
        if (tGuestCpuOverlap) {
            tGuestCpuOverlap = false;
            sGuestCpuOverlapPending = false;
            sCv.notify_all();
        }
        sCv.wait(lk, ready);
        if (sStopping) {
            lk.unlock();
            throw sched::ThreadExit{};
        }
    }
    if (!release) {
#ifdef __SWITCH__
        perfAdd();
#endif
        return;
    }
    sched::reacquireAfterHost();
#ifdef __SWITCH__
    perfAdd();
#endif
    // Another thread may have taken GX again before this one got the CPU back.
    gxWaitForGuestTurn();
}

void gxBlockingWait(void (*wait)()) {
    overlapGraphicsBarrier();
    {
        std::lock_guard lk(sM);
        sGxBusy = true;
    }
    const bool release = sched::interruptsEnabled();  // (critical sections keep the CPU; from SMS)
    if (release) sched::releaseForHost();
    wait();
    {
        std::lock_guard lk(sM);
        sGxBusy = false;
        sCv.notify_all();
    }
    if (release) sched::reacquireAfterHost();
}

}  // namespace recomp::frame
