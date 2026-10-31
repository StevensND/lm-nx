// Synthetic graphics test; contains and runs no Luigi's Mansion code.
#include "runtime.hpp"
#include <lmnx/native_gfx.hpp>
#include <switch.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace {
FILE* archivo = nullptr;
void mensaje(const char* texto) {
    if (archivo) { std::fprintf(archivo, "%s\n", texto); std::fflush(archivo); }
}
void comprobar(bool condicion, const char* texto) {
    mensaje(texto);
    if (!condicion) throw std::runtime_error(texto);
}
}
int main() {
    std::string error;
    lmnx::plataforma::prepararDirectorios(error);
    archivo = std::fopen("sdmc:/switch/LM-NX/logs/graphics-probe.log", "w");
    mensaje("Synthetic deko3d test. This log exists only if the NRO was run.");
    plume::SetDeko3DLogCallback(mensaje);
    lmnx::abrirCacheShaders("sdmc:/switch/LM-NX/cache");
    int codigo = 0;
    try {
        using namespace lmnx::gfx;
        using namespace plume;
        // Do not call consoleInit; deko3d exclusively owns NWindow.
        Contexto contexto;
        const auto w = contexto.ancho(), h = contexto.alto();
        auto color = contexto.crearTextura(RenderTextureDesc::ColorTarget(w, h, RenderFormat::R8G8B8A8_UNORM));
        auto copia = contexto.crearTextura(RenderTextureDesc::ColorTarget(w, h, RenderFormat::R8G8B8A8_UNORM));
        auto profundidad = contexto.crearTextura(RenderTextureDesc::DepthTarget(w, h, RenderFormat::D32_FLOAT));
        SolicitudPipeline solicitud;
        solicitud.vertice = R"(#version 460
void main() {
    vec2 p[3] = vec2[3](vec2(-0.5,-0.5), vec2(0.5,-0.5), vec2(0.0,0.5));
    gl_Position = vec4(p[gl_VertexID], 0.5, 1.0);
}
)";
        solicitud.fragmento = R"(#version 460
layout(location = 0) out vec4 color;
void main() { color = vec4(0.0,1.0,0.0,1.0); }
)";
        solicitud.estado.renderTargetCount = 1;
        solicitud.estado.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
        solicitud.estado.renderTargetBlend[0] = RenderBlendDesc::Copy();
        solicitud.estado.depthTargetFormat = RenderFormat::D32_FLOAT;
        solicitud.estado.depthClipEnabled = true;
        auto futuro = contexto.compilar(solicitud);
        // The compilation thread runs uam; only this test needs the wait.
        auto pipeline = futuro.get();
        Dibujo dibujo; dibujo.pipeline = pipeline; dibujo.cantidad = 3;
        Paso paso;
        paso.colores = {color}; paso.profundidad = profundidad;
        paso.limpiarColores = {RenderColor(1.f, 0.f, 0.f, 1.f)};
        paso.limpiarProfundidad = true; paso.z = 0.25f;
        paso.comandos = {Scissor{-8, -8, int32_t(w) + 8, int32_t(h) + 8}, dibujo};
        Paquete paquete;
        paquete.operaciones.emplace_back(paso);
        paquete.operaciones.emplace_back(Copia{color, copia, 0, 0, w, h});
        paquete.operaciones.emplace_back(Lectura{copia, 3, 7, 1, 1, [](auto bytes) {
            comprobar(bytes.size() == 4 && bytes[0] == 255 && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 255,
                "Clear + color copy + readback at (3,7): should be red RGBA8");
        }});
        paquete.operaciones.emplace_back(Lectura{copia, w / 2, h / 2, 1, 1, [](auto bytes) {
            comprobar(bytes.size() == 4 && bytes[0] == 0 && bytes[1] == 255 && bytes[2] == 0 && bytes[3] == 255,
                "Center triangle: should be green RGBA8");
        }});
        paquete.operaciones.emplace_back(Lectura{profundidad, 3, 7, 1, 1, [](auto bytes) {
            float z = -1.f; if (bytes.size() == 4) std::memcpy(&z, bytes.data(), 4);
            comprobar(std::abs(z - 0.25f) < 0.00001f, "Depth clear + D32 readback: should be 0.25");
        }});
        paquete.presentar = copia;
        contexto.ejecutar(paquete);
        contexto.esperarVblank();
        mensaje("Draw, color copy, color/depth readback and vblank event checks completed.");
        PadState pad;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
        Paquete presentar; presentar.presentar = copia;
        while (appletMainLoop()) {
            padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            contexto.ejecutar(presentar);
        }
    } catch (const std::exception& e) {
        mensaje(e.what()); codigo = 1;
        // Contexto has been destroyed, so the console can acquire NWindow.
        consoleInit(nullptr);
        std::printf("Graphics test failed\n%s\nSee logs/graphics-probe.log\n", e.what());
        consoleUpdate(nullptr); svcSleepThread(3000000000ull); consoleExit(nullptr);
    }
    lmnx::cerrarCacheShaders(); plume::SetDeko3DLogCallback(nullptr);
    if (archivo) std::fclose(archivo);
    return codigo;
}
