// PORT (ProjectLM-NX): first display frames, before the shader screen. The table (vi_tabla.cpp, made by
// tools/build_switch_direct.py) is a zstd stream of 8-bit planes, read one plane at a time on a fixed clock.
// The planes are summed as they are read; lmnx_vi_valida() compares that sum with the one the build computed from
// the same data, so a build whose table was removed or replaced stops at start-up with a clear message.
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/imgui.h>
#include <imgui.h>
#include <xxhash.h>
#include <zstd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

extern "C" const unsigned char lmnx_vi_tabla[] __attribute__((weak));
extern "C" const unsigned long lmnx_vi_tam __attribute__((weak));
extern "C" const unsigned lmnx_vi_w __attribute__((weak));
extern "C" const unsigned lmnx_vi_h __attribute__((weak));
extern "C" const unsigned lmnx_vi_n __attribute__((weak));
extern "C" const unsigned lmnx_vi_hz __attribute__((weak));
extern "C" const unsigned long long lmnx_vi_esperada __attribute__((weak));

namespace {
uint64_t sSuma = 0;  // sum of every plane, set only when the whole table was read

inline void expandir(const uint8_t* plano, uint8_t* rgba, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const unsigned g = plano[i];
        const unsigned k = 255 - g;
        rgba[4 * i] = uint8_t(g - (g * k * 10) / (255 * 100));
        rgba[4 * i + 1] = uint8_t(g - (g * k * 18) / (255 * 100));
        rgba[4 * i + 2] = uint8_t(std::min(255u, g + (g * k * 45) / (255 * 100)));
        rgba[4 * i + 3] = 255;
    }
}

bool cerrarPedido() {
    bool cerrar = false;
    for (const AuroraEvent* ev = aurora_update(); ev && ev->type != AURORA_NONE; ++ev)
        if (ev->type == AURORA_EXIT) cerrar = true;
    return cerrar;
}
}  // namespace

// false if the program was asked to close meanwhile.
bool lmnx_vi_ajustar() {
    if (&lmnx_vi_tabla == nullptr || &lmnx_vi_tam == nullptr || &lmnx_vi_n == nullptr) return true;
    const unsigned w = lmnx_vi_w, h = lmnx_vi_h, total = lmnx_vi_n;
    const double hz = lmnx_vi_hz ? double(lmnx_vi_hz) : 60.0;
    if (!w || !h || !total) return true;

    ZSTD_DStream* flujo = ZSTD_createDStream();
    if (!flujo) return true;
    ZSTD_initDStream(flujo);
    ZSTD_inBuffer entrada{lmnx_vi_tabla, size_t(lmnx_vi_tam), 0};
    std::vector<uint8_t> plano(size_t(w) * h), rgba(size_t(w) * h * 4);
    XXH64_state_t* suma = XXH64_createState();
    XXH64_reset(suma, 0);
    const auto siguiente = [&]() {
        ZSTD_outBuffer salida{plano.data(), plano.size(), 0};
        while (salida.pos < salida.size) {
            const size_t antes = salida.pos;
            const size_t r = ZSTD_decompressStream(flujo, &salida, &entrada);
            if (ZSTD_isError(r)) return false;
            if (entrada.pos == entrada.size && salida.pos == antes) return false;
        }
        XXH64_update(suma, plano.data(), plano.size());
        return true;
    };

    // The table stands still from plane 101 to 135 (0.6 s), too short to read the name: the clock stops on plane 120
    // for kPausa seconds, then the fade-out plays as recorded.
    constexpr double kPlanoPausa = 120.0, kPausa = 3.0;
    using Reloj = std::chrono::steady_clock;
    const auto inicio = Reloj::now();
    ImTextureID textura{};
    unsigned leidos = 0;
    bool seguir = true, cerrar = false;
    while (seguir) {
        if (cerrarPedido()) {
            cerrar = true;
            break;
        }
        double t = std::chrono::duration<double>(Reloj::now() - inicio).count() * hz;
        if (total > kPlanoPausa && t > kPlanoPausa) t = std::max(kPlanoPausa, t - kPausa * hz);
        const unsigned objetivo = unsigned(t) + 1;
        if (objetivo > total) break;
        bool nuevo = false;
        while (leidos < objetivo) {  // behind the clock: the planes in between are read and skipped
            if (!siguiente()) {
                seguir = false;
                break;
            }
            ++leidos;
            nuevo = true;
        }
        if (!seguir) break;
        if (nuevo) {
            expandir(plano.data(), rgba.data(), plano.size());
            if (!textura) textura = aurora_imgui_add_texture(w, h, rgba.data());
            else aurora_imgui_update_texture(textura, w, h, rgba.data());
        }
        if (!aurora_begin_frame()) continue;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList* lista = ImGui::GetBackgroundDrawList();
        lista->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(0, 0, 0, 255));
        if (textura) {
            const float k = std::min(vp->Size.x / float(w), vp->Size.y / float(h));
            const ImVec2 tam(float(w) * k, float(h) * k);
            const ImVec2 pos(vp->Pos.x + (vp->Size.x - tam.x) * 0.5f, vp->Pos.y + (vp->Size.y - tam.y) * 0.5f);
            lista->AddImage(textura, pos, ImVec2(pos.x + tam.x, pos.y + tam.y));
        }
        aurora_end_frame();
    }
    // The clock can step past the last planes: read whatever is left so the sum covers the whole table.
    while (seguir && !cerrar && leidos < total && siguiente()) ++leidos;
    if (leidos == total) sSuma = XXH64_digest(suma);
    XXH64_freeState(suma);
    ZSTD_freeDStream(flujo);
    return !cerrar;
}

// True when this build has no table (test NROs) or the table read at start-up is the one the build was made from.
bool lmnx_vi_valida() {
    if (&lmnx_vi_esperada == nullptr || lmnx_vi_esperada == 0) return true;
    return sSuma == lmnx_vi_esperada;
}

// Start-up stops here: says why on screen until the player closes the program (HOME) or 30 s pass.
void lmnx_vi_aviso() {
    std::printf("[arranque] start-up data missing or modified: rebuild with the original files\n");
    const auto inicio = std::chrono::steady_clock::now();
    while (!cerrarPedido() && std::chrono::steady_clock::now() - inicio < std::chrono::seconds(30)) {
        if (!aurora_begin_frame()) continue;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(vp->Size.x * 0.6f, 0), ImGuiCond_Always);
        ImGui::Begin("##arranque", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
        ImGui::TextWrapped("This build cannot start: some of its start-up data is missing or was modified.");
        ImGui::Spacing();
        ImGui::TextWrapped("Build it again from the original source files, or use the official release.");
        ImGui::Spacing();
        ImGui::TextWrapped("Press HOME to close.");
        ImGui::End();
        aurora_end_frame();
    }
}
