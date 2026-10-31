// Menu size (issue #20; settings interface.menu_scale, F1 > Visuals > General "Menu size").
//
// Dear ImGui here is 1.91 (fixed-size font atlas): a larger menu rebuilds the atlas with the
// default font (ProggyClean, made for 13 px) at round(13 x scale) physical pixels and replaces the
// style with ImGui's default style scaled once (ScaleAllSizes on a clean copy, never cumulative),
// so text is rasterized at its real size instead of a 13 px atlas being stretched (blurry). Both
// happen in Aurora's ImGui frame callback before NewFrame (Aurora patch 0238: the atlas is locked
// during a frame), so a change applies from the next frame.
//
// Auto: the window height (1080 lines = 100%, 4K = 200%), the display's scaling setting (Windows
// 150% = 150%) or 150% on a Steam Deck (7" 800p), whichever is largest, rounded down to a step;
// the setup screen (a full-window page) is larger by default (600 lines = 100%, as before). Any
// size is limited to what still fits the window (fitPercent).
//
// Windows with a size the player can change and imgui.ini keeps (the F1 menu, the model viewer,
// debug viewers) go through beforeWindow(): default and minimum size scale with the menu, a
// saved size is rescaled when the menu size changes (imgui.ini also records the scale it was saved
// at, section [LMUiScale]; a size saved by an older version is taken as 100%), and the window is
// kept on the screen.
#include "recomp/ui_scale.h"

#include "port/settings.h"

#include <aurora/aurora.h>
#include <aurora/imgui.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

// The start-up notice's font, built into the NRO (tools/build_switch_direct.py); weak: absent elsewhere.
extern "C" const unsigned char lmnx_fuente_aviso[] __attribute__((weak));
extern "C" const unsigned long lmnx_fuente_aviso_tam __attribute__((weak));

namespace recomp::ui {
namespace {

Context sContext = Context::Game;
bool sInstalled = false;
float sScale = 1.0f;
int sPercent = 100, sAuto = 100, sFit = kSteps[kStepCount - 1];
float sFontPx = 0.0f;       // atlas font size, physical pixels
float sFontDensity = 0.0f;  // physical pixels per ImGui unit the atlas was built for
ImFont* sLarge = nullptr;     // the same font kLarge times larger (largeFont)
constexpr float kLarge = 1.5f;
ImFont* sAvisoTitulo = nullptr;  // the start-up notice's fonts (Switch: platforms/switch/aviso.cpp)
ImFont* sAvisoTexto = nullptr;
float sAvisoPx = 0.0f;  // body size the notice fonts were built for, physical pixels
ImGuiStyle sBaseStyle;
bool sHaveBase = false;
float sStyleScale = 0.0f;

// Largest step at or below a scale (at least the smallest step).
int stepDown(float s) {
    int best = kSteps[0];
    for (int p : kSteps)
        if (float(p) <= s * 100.0f + 0.5f) best = p;
    return best;
}
int stepNearest(int pct) {
    int best = kSteps[0];
    for (int p : kSteps)
        if (std::abs(p - pct) < std::abs(best - pct)) best = p;
    return best;
}

bool steamDeck() {
    // Steam sets SteamDeck=1 on the Deck (game mode and desktop mode).
    static const bool sDeck = [] {
        const char* v = std::getenv("SteamDeck");
        return v && *v && std::strcmp(v, "0") != 0;
    }();
    return sDeck;
}

// Saved scale per window (imgui.ini [LMUiScale][<window name>] Scale=<f>), and the scale each
// window was last drawn at.
struct WinState {
    std::string name;
    float iniScale = 0.0f;  // from imgui.ini (0: none saved)
    float scale = 0.0f;     // last frame's (0: not drawn yet this session)
};
std::unordered_map<ImGuiID, WinState> sWins;
ImGuiID sAnchorWindow = 0;  // window to rescale about sAnchor at the next size change
ImVec2 sAnchor;

void* iniReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    WinState& w = sWins[ImHashStr(name)];
    w.name = name;
    return &w;
}
void iniReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    float f = 0.0f;
    if (std::sscanf(line, "Scale=%f", &f) == 1 && f > 0.1f && f < 10.0f) static_cast<WinState*>(entry)->iniScale = f;
}
void iniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    for (const auto& [id, w] : sWins) {
        const float s = w.scale > 0.0f ? w.scale : w.iniScale;
        if (s <= 0.0f || w.name.empty()) continue;
        buf->appendf("[%s][%s]\nScale=%.3f\n\n", handler->TypeName, w.name.c_str(), s);
    }
}

void frameCallback(const AuroraWindowSize* size) {
    if (ImGui::GetCurrentContext() == nullptr || size == nullptr) return;
    ImGuiIO& io = ImGui::GetIO();
    const float logicalW = float(std::max(size->width, 1u)), logicalH = float(std::max(size->height, 1u));
    const float density = size->width > 0 && size->native_fb_width > 0 ? float(size->native_fb_width) / logicalW : 1.0f;
    // Display scaling relative to ImGui units (Windows: SDL's display scale, pixels = units;
    // macOS/Wayland: units are already scaled points).
    const float dpi = size->scale > 0.0f ? size->scale / density : 1.0f;

    float autoScale = std::max({1.0f, logicalH / 1080.0f, dpi});
#ifdef __SWITCH__
    // Switch: 1280x720 handheld, 1920x1080 docked (sdl3_shim): the menu keeps the size it has in handheld mode.
    autoScale = std::max(autoScale, logicalH / 720.0f);
#endif
    if (steamDeck()) autoScale = std::max(autoScale, 1.5f);
    if (sContext == Context::Setup) autoScale = std::max(autoScale, logicalH / 600.0f);
    // The menu needs about 560 x 330 units at 100% to be usable (tab bar, a few rows).
    const float fit = std::min(logicalW / 560.0f, logicalH / 330.0f);
    sFit = stepDown(fit);
    sAuto = std::min(stepDown(autoScale), sFit);
    const int setting = port::settings().ui.menuScale;
    int pct = setting <= 0 ? sAuto : std::min(stepNearest(setting), sFit);
    pct = std::max(pct, kSteps[0]);
    const float s = float(pct) / 100.0f;
    if (pct != sPercent || sFontPx == 0.0f) {
        std::printf("[ui] menu size %d%% (%s; auto %d%%, fits %d%%; window %.0fx%.0f, display scale %.2f)\n", pct,
                    setting <= 0 ? "auto" : "set", sAuto, sFit, logicalW, logicalH, size->scale);
        std::fflush(stdout);
    }
    sPercent = pct;
    sScale = s;

    // Font: rasterized at the size it is shown at, in physical pixels.
    const float fontPx = std::round(13.0f * s * density);
    const bool conAviso = &lmnx_fuente_aviso != nullptr && &lmnx_fuente_aviso_tam != nullptr;
    // the notice's body text: 28 px on a 720-line screen, following the window height
    const float avisoPx = conAviso ? std::round(28.0f * logicalH / 720.0f * density) : 0.0f;
    if (fontPx != sFontPx || density != sFontDensity || avisoPx != sAvisoPx) {
        io.Fonts->Clear();
        ImFontConfig cfg;
        cfg.OversampleH = cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        cfg.SizePixels = fontPx;
        io.Fonts->AddFontDefault(&cfg);  // Fonts[0]: ImGui's default
        cfg.SizePixels = std::round(13.0f * s * kLarge * density);
        sLarge = io.Fonts->AddFontDefault(&cfg);
        if (conAviso) {
            ImFontConfig ttf;
            ttf.FontDataOwnedByAtlas = false;  // the data lives in the NRO
            ttf.OversampleH = 2;
            ttf.OversampleV = 1;
            void* datos = const_cast<unsigned char*>(lmnx_fuente_aviso);
            const int tam = int(lmnx_fuente_aviso_tam);
            sAvisoTexto = io.Fonts->AddFontFromMemoryTTF(datos, tam, avisoPx, &ttf);
            sAvisoTitulo = io.Fonts->AddFontFromMemoryTTF(datos, tam, std::round(avisoPx * 46.0f / 28.0f), &ttf);
        }
        io.FontGlobalScale = 1.0f / density;  // physical pixels back to ImGui units: 1:1 on screen
        sFontPx = fontPx;
        sFontDensity = density;
        sAvisoPx = avisoPx;
        // (left unbuilt: Aurora builds and uploads it after this callback)
    }

    // Style: ImGui's default sizes scaled once from a clean copy.
    if (!sHaveBase) {
        sBaseStyle = ImGui::GetStyle();
        sHaveBase = true;
    }
    if (s != sStyleScale) {
        ImGuiStyle st = sBaseStyle;
        st.ScaleAllSizes(s);
        // (ScaleAllSizes leaves line widths alone; the heading rule of SeparatorText grows with
        // the text, other borders stay 1 px.)
        st.SeparatorTextBorderSize = std::max(1.0f, std::round(sBaseStyle.SeparatorTextBorderSize * s));
        st.MouseCursorScale = std::max(1.0f, sBaseStyle.MouseCursorScale * s);
        ImGui::GetStyle() = st;
        sStyleScale = s;
    }
}

}  // namespace

void install(Context context) {
    sContext = context;
    if (sInstalled) return;
    sInstalled = true;
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiSettingsHandler h;
        h.TypeName = "LMUiScale";
        h.TypeHash = ImHashStr("LMUiScale");
        h.ReadOpenFn = iniReadOpen;
        h.ReadLineFn = iniReadLine;
        h.WriteAllFn = iniWriteAll;
        ImGui::AddSettingsHandler(&h);
    }
    aurora_imgui_set_frame_callback(frameCallback);
}

float scale() { return sScale; }
ImFont* largeFont() { return sLarge; }
ImFont* noticeFont(bool title) { return title ? sAvisoTitulo : sAvisoTexto; }
int autoPercent() { return sAuto; }
int fitPercent() { return sFit; }
int percent() { return sPercent; }

void setAnchor(const char* window, ImVec2 p) {
    sAnchorWindow = ImHashStr(window);
    sAnchor = p;
}

void beforeWindow(const char* name, ImVec2 defaultSize, ImVec2 minSize) {
    const float s = sScale;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const ImGuiID id = ImHashStr(name);
    WinState& st = sWins[id];
    if (st.name.empty()) st.name = name;
    const auto fitTo = [&](ImVec2 v) { return ImVec2(std::min(v.x, ds.x), std::min(v.y, ds.y)); };
    ImGui::SetNextWindowSize(fitTo(ImVec2(defaultSize.x * s, defaultSize.y * s)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(fitTo(ImVec2(minSize.x * s, minSize.y * s)), ds);

    ImGuiWindow* w = ImGui::FindWindowByID(id);
    ImVec2 pos, sz, scroll(0, 0);
    bool have = false;
    if (w != nullptr) {
        pos = w->Pos;
        sz = w->Size;
        scroll = w->Scroll;
        have = true;
    } else if (const ImGuiWindowSettings* ws = ImGui::FindWindowSettingsByID(id)) {
        // First Begin of the session: the size and place imgui.ini saved.
        pos = ImVec2(ws->Pos.x, ws->Pos.y);
        sz = ImVec2(ws->Size.x, ws->Size.y);
        have = sz.x > 0 && sz.y > 0;
    }
    // Scale the window was last sized at: this session's, else the one saved with it (a size from
    // a version without this option was made at 100%).
    const float prev = st.scale > 0.0f ? st.scale : have ? (st.iniScale > 0.0f ? st.iniScale : 1.0f) : s;
    if (have && std::fabs(prev - s) > 0.001f) {
        // Rescale about the anchor (the point under the mouse when the size was changed with it,
        // so what was under the mouse stays there), else about the window's centre.
        const float r = s / prev;
        const ImVec2 a = sAnchorWindow == id && w != nullptr ? sAnchor : ImVec2(pos.x + sz.x * 0.5f, pos.y + sz.y * 0.5f);
        ImVec2 nsz = fitTo(ImVec2(sz.x * r, sz.y * r));
        ImVec2 npos(a.x - (a.x - pos.x) * r, a.y - (a.y - pos.y) * r);
        npos.x = std::clamp(npos.x, 0.0f, std::max(0.0f, ds.x - nsz.x));
        npos.y = std::clamp(npos.y, 0.0f, std::max(0.0f, ds.y - nsz.y));
        ImGui::SetNextWindowSize(nsz, ImGuiCond_Always);
        ImGui::SetNextWindowPos(npos, ImGuiCond_Always);
        if (w != nullptr) ImGui::SetNextWindowScroll(ImVec2(scroll.x * r, scroll.y * r));
        ImGui::MarkIniSettingsDirty();
    } else if (w != nullptr) {
        // Keep it on the screen (a smaller window, a larger menu size, a saved place off-screen).
        const ImVec2 nsz = fitTo(sz);
        const ImVec2 npos(std::clamp(pos.x, 0.0f, std::max(0.0f, ds.x - nsz.x)),
                          std::clamp(pos.y, 0.0f, std::max(0.0f, ds.y - nsz.y)));
        if (npos.x != pos.x || npos.y != pos.y) ImGui::SetNextWindowPos(npos, ImGuiCond_Always);
    }
    if (st.scale != s && sAnchorWindow == id) sAnchorWindow = 0;
    st.scale = s;
}

}  // namespace recomp::ui
