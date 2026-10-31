// PORT (ProjectLM-NX): the notice shown once at every start, before the shader screen: who made the port, what it is
// based on, and that it must not be sold. It stays until A (or B, or +) is pressed; presses in the first moment do not
// count, so it is read. Text in the Delfino font (src/recomp/ui_scale.cpp noticeFont), every line centred.
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace recomp::ui {
ImFont* noticeFont(bool title);  // src/recomp/ui_scale.cpp
}

namespace {
bool cerrarPedido() {
    bool cerrar = false;
    for (const AuroraEvent* ev = aurora_update(); ev && ev->type != AURORA_NONE; ++ev)
        if (ev->type == AURORA_EXIT) cerrar = true;
    return cerrar;
}

// The text wrapped at `ancho` and each line centred in the window.
void centrado(const char* texto, float ancho) {
    ImFont* f = ImGui::GetFont();
    const float escala = ImGui::GetFontSize() / f->FontSize;
    const char* fin = texto + std::strlen(texto);
    const float x0 = ImGui::GetCursorPosX();
    const float hueco = ImGui::GetContentRegionAvail().x;
    for (const char* p = texto; p < fin;) {
        const char* corte = f->CalcWordWrapPositionA(escala, p, fin, ancho);
        if (corte == p) corte = p + 1;  // a word wider than the line: at least one character
        const char* finLinea = corte;
        while (finLinea > p && finLinea[-1] == ' ') --finLinea;
        const float w = ImGui::CalcTextSize(p, finLinea).x;
        ImGui::SetCursorPosX(x0 + std::max(0.0f, (hueco - w) * 0.5f));
        ImGui::TextUnformatted(p, finLinea);
        p = corte;
        while (p < fin && *p == ' ') ++p;
    }
}
}  // namespace

// false if the program was asked to close meanwhile.
bool lmnx_aviso_mostrar() {
    using Reloj = std::chrono::steady_clock;
    const auto inicio = Reloj::now();
    constexpr auto kMinimo = std::chrono::milliseconds(2500);  // before this, a press does not skip it
    while (true) {
        if (cerrarPedido()) return false;
        const auto pasado = Reloj::now() - inicio;
        if (!aurora_begin_frame()) continue;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                      IM_COL32(0, 0, 0, 255));
        const float escala = vp->Size.y / 720.0f;  // the layout was made for a 720-line screen
        const float ancho = vp->Size.x * 0.62f;
        ImFont* titulo = recomp::ui::noticeFont(true);
        ImFont* texto = recomp::ui::noticeFont(false);
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(ancho + 40.0f * escala, 0), ImGuiCond_Always);
        ImGui::Begin("##aviso", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        // (the first frame can come before the font is ready: the default font, enlarged, for that frame)
        if (titulo) ImGui::PushFont(titulo);
        else ImGui::SetWindowFontScale(2.4f * escala);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(76, 195, 90, 255));
        centrado("Disclaimer", ancho);
        ImGui::PopStyleColor();
        if (titulo) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 10.0f * escala));
        if (texto) ImGui::PushFont(texto);
        else ImGui::SetWindowFontScale(1.6f * escala);
        centrado("This is a community-made port. It's based on the work of rob646464 (Luigi's Mansion Recompiled) "
                 "adapted to Nintendo Switch.", ancho);
        ImGui::Dummy(ImVec2(0, 8.0f * escala));
        centrado("This port is free. Selling it isn't allowed. If you paid for it or for having it installed, "
                 "you were scammed.", ancho);
        ImGui::Dummy(ImVec2(0, 8.0f * escala));
        centrado("Tools like AIO Switch Updater and similar and sites like NXMaster aren't affiliated with or endorsed by "
                 "this project.", ancho);
        ImGui::Dummy(ImVec2(0, 8.0f * escala));
        centrado("Luigi's Mansion is a trademark of Nintendo. This project is not affiliated with Nintendo.", ancho);
        ImGui::Dummy(ImVec2(0, 24.0f * escala));
        // "Press (A) to continue", as on the Wii's start-up screen: it fades in once the notice can be skipped and then
        // breathes, fading out completely and back in (opacity 0 -> 1 -> 0 every 1.6 s). The cycle starts at 0, so the
        // first appearance is the same smooth fade as the rest. The A sits in a drawn circle, like the button.
        {
            const float u = std::chrono::duration<float>(pasado - kMinimo).count();
            const float alfa = u >= 0.0f ? 0.5f - 0.5f * std::cos(6.2831853f * u / 1.6f) : 0.0f;
            const ImU32 col = IM_COL32(200, 206, 212, int(255.0f * alfa));
            const char* izq = "Press ";
            const char* der = " to continue";
            const float tam = ImGui::GetFontSize();
            const float r = tam * 0.6f;
            const float wIzq = ImGui::CalcTextSize(izq).x, wDer = ImGui::CalcTextSize(der).x;
            const float total = wIzq + 2.0f * r + wDer;
            const ImVec2 inicioLinea = ImGui::GetCursorScreenPos();
            const float x = ImGui::GetWindowPos().x + (ImGui::GetWindowSize().x - total) * 0.5f;
            const float y = inicioLinea.y;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(ImVec2(x, y), col, izq);
            const ImVec2 centro(x + wIzq + r, y + tam * 0.52f);
            dl->AddCircle(centro, r, col, 0, std::max(2.0f, tam * 0.09f));
            const ImVec2 wa = ImGui::CalcTextSize("A");
            dl->AddText(ImVec2(centro.x - wa.x * 0.5f, centro.y - wa.y * 0.5f), col, "A");
            dl->AddText(ImVec2(x + wIzq + 2.0f * r, y), col, der);
            ImGui::Dummy(ImVec2(total, tam));
        }
        if (texto) ImGui::PopFont();
        const bool pulsado = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) ||
                             ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) ||
                             ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false) ||
                             ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        ImGui::End();
        aurora_end_frame();
        if (pulsado && pasado >= kMinimo) return true;
    }
}
