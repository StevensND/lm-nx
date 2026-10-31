#pragma once
// Live performance panel (Enhancements > Interface > Performance panel), based on banjo-nx
// app/src/profiler/profiler_overlay.h and its UnleashedRecomp origin. Uses this port's threads and extra detail
// lines. Excluded from shader identity (tools/shader_build_id.py, AJENOS_A_SHADERS).
#include <atomic>
#include <cstdint>

// hidden on the console (one static executable built -fPIE): the GX HLEs read `activo` on every call, and
// with default visibility each read went through the GOT first.
#ifdef __SWITCH__
#pragma GCC visibility push(hidden)
#endif
namespace lmnx::perfil {

// Set by the menu while the panel is shown: without it nothing is measured nor published.
inline std::atomic<bool> activo{false};
// Bumped by the menu each time the panel is switched on: every measuring thread restarts its bases and drops only its
// first sample (no slow frame is ever filtered out).
inline std::atomic<uint32_t> generacion{0};

// banjo-nx's series, in its order, mapped to this port (names in menu.cpp):
//   Application        = game frame: GXCopyDisp to GXCopyDisp, on the game thread
//   Update             = CPU time of the game thread in that frame (Horizon thread ticks)
//   Draw               = GX FIFO thread translating that frame's GX stream into draws (wall clock)
//   Frame Limiter Wait = the game thread waiting in that frame for the retrace or for its turn back from the host
//   GPU Frame          = GPU busy time of the last presented frame
//   Present            = host present to host present
//   Render thread      = render thread work of the last frame (record + execute + submit + present)
enum Serie : int { Aplicacion, Juego, Fifo, Limitador, Gpu, Presentar, Render, kSeries,
                   // Extra values (ours): per game frame too, averaged like the series but not drawn in the graph.
                   EsperaFifo = kSeries,  // game thread waiting for the GX FIFO thread (GXDrawDone)
                   FifoCpu,               // CPU time the GX FIFO thread actually ran (vs Draw, its wall clock)
                   HostMenu,              // main thread after the game's frame: the menu and overlays (ImGui)
                   HostFin,               // main thread after the game's frame: aurora_end_frame
                   Readquirir,            // of Frame Limiter Wait: getting the guest CPU back once woken
                   EsperaGx,              // game threads waiting for GX to be theirs (gxWaitForGuestTurn)
                   Publicaciones,         // count, not ms: GX FIFO publishes per game frame
                   Despertares,           // count, not ms: GX FIFO worker wake-ups per game frame
                   Instantaneas,          // vertex snapshots (interp_vtx.cpp) on the game thread, ms per game frame
                   InstantaneasKiB,       // KiB, not ms: vertex data those snapshots copied per game frame
                   LlamadasGx,            // wall time inside the GX HLEs (Aurora's GX encoding + snapshots), ms
                   LlamadasGxN,           // count, not ms: GX HLE calls per game frame
                   kValores };
// Accumulated by the GX HLEs (hle_gx.cpp), in system ticks (19.2 MHz), and taken once per game frame.
inline std::atomic<uint64_t> llamadasGxTicks{0};
inline std::atomic<uint64_t> llamadasGxN{0};

// Current value of each series (banjo: profiler::Current / HasValue). Written by the measuring threads.
inline std::atomic<double> actual[kValores]{};
inline std::atomic<bool> hay[kValores]{};
inline void guardar(int s, double ms) {
    actual[s].store(ms, std::memory_order_relaxed);
    hay[s].store(true, std::memory_order_relaxed);
}

// Nanoseconds added during the frame by other threads; taken at the frame boundary (GXCopyDisp) or at the present.
inline std::atomic<uint64_t> esperaFifoNs{0};    // game thread waiting for the GX FIFO thread (GXDrawDone -> drain)
inline std::atomic<uint64_t> esperaVblankNs{0};  // game thread waiting for the retrace (VIWaitForRetrace) or for its
                                                 // turn back after handing the frame to the host (frame::present)
inline std::atomic<uint64_t> fifoOcupadoNs{0};   // GX FIFO thread translating the stream (process_to), wall clock
inline std::atomic<uint64_t> readquirirNs{0};    // frame::present: woken, until the guest CPU is back
inline std::atomic<uint64_t> publicaciones{0};   // GX FIFO: publish() calls that advanced the published mark
inline std::atomic<uint64_t> despertares{0};     // GX FIFO: times the worker went to sleep and was woken again
inline std::atomic<uint64_t> esperaGxNs{0};      // frame::gxWaitForGuestTurn: waiting for GX to be the guest's
inline std::atomic<uint64_t> fifoCpuNs{0};       // the same batches, CPU time actually run (Horizon thread ticks):
                                                 // far below the wall clock = the thread loses its core meanwhile

// Every game frame time (Application), in order, for the frame-pacing stats (ours): written by the game thread
// (perfFrameBoundary), read by the menu, which takes the samples between its last count and `aplicacionCuenta`.
inline constexpr uint32_t kAnilloAplicacion = 1024;
inline std::atomic<float> anilloAplicacion[kAnilloAplicacion]{};
inline std::atomic<uint32_t> aplicacionCuenta{0};
inline void anotarAplicacion(float ms) {
    const uint32_t n = aplicacionCuenta.load(std::memory_order_relaxed);
    anilloAplicacion[n % kAnilloAplicacion].store(ms, std::memory_order_relaxed);
    aplicacionCuenta.store(n + 1, std::memory_order_release);
}

// Aurora's per-frame work (ours), summed by the render worker at each frame's end (Aurora's frame.cpp, patched by
// tools/prepare_aurora_native.py) so the panel averages it without touching Aurora's own stats: frames, GX draws,
// draws merged into the previous one, and bytes uploaded (vertices, indices, uniforms, storage, textures).
enum AuroraValor : int { AFrames, ADraws, AMerged, AVert, AIndex, AUniform, AStorage, ATexture, kAuroraValores };
inline std::atomic<uint64_t> aurora[kAuroraValores]{};

// Extra lines (ours), averaged over 0.5 s by native_gfx.cpp.
struct Detalle {
    float grabar = 0, ejecutar = 0, enviar = 0, presentar = 0;      // render thread, ms per present
    float dibujos = 0;                                              // draws per present
    float lotes = 0, pipelines = 0, texturas = 0, uniformes = 0;    // batches and state changes per present
    float barreras = 0, segmentos = 0;                              // full barriers, command segments per present
    float esperaGpu = 0, envios = 0;                                // render thread waiting for GPU fences (ms), submits
    float gpuMemMiB = 0, gpuMemPicoMiB = 0;                         // GPU memory blocks alive / peak
    char pasada[4][48] = {};                                        // most expensive GPU passes
    float pasadaMs[4] = {};
    uint32_t version = 0;  // 0: nothing yet
};
void leer(Detalle& destino);  // native_gfx.cpp

}  // namespace lmnx::perfil
#ifdef __SWITCH__
#pragma GCC visibility pop
#endif
