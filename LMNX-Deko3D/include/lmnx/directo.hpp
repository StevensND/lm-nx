#pragma once
// Encode a whole Aurora GX pass directly into deko3d, as SwitchWaker dk_encode does
// (switch/deko/aurora/dk_encode.cpp). Cache pipeline, uniforms, textures, immediates, viewport and scissor
// state, avoiding per-draw state copies, deferred functions and barriers. Outside shader identity
// (tools/shader_build_id.py) because this does not affect DKSH.
#include <lmnx/aurora_gpu.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace lmnx::gpu::directo {

struct Orden {
    enum class Tipo : uint8_t { Dibujo, Viewport, Scissor };
    Tipo tipo = Tipo::Dibujo;
    // GX draw (gx::DrawData): main pipeline and optional alpha prepass pipeline.
    RenderPipeline principal;
    RenderPipeline previo;
    BindGroup texturas;  // Empty: keep the previous draw's textures, as in Aurora's WebGPU pass.
    uint32_t offsetUniformes = 0;
    uint32_t primerIndice = 0;
    uint32_t indices = 0;     // Zero: non-indexed draw.
    uint32_t vertices = 0;
    uint32_t instancias = 1;
    uint32_t dstAlpha = UINT32_MAX;
    std::array<uint8_t, 64> inmediatos{};
    // Viewport (x, y, width, height, zMin, zMax resolved by Aurora) and scissor (x, y, width, height already clipped).
    float viewport[6]{};
    uint32_t scissor[4]{};
};

// Record one operation containing the whole batch. estatico and uniformes are Aurora groups 0 and 1
// (staticBindGroup and uniformBindGroup, the latter with dynamic offset); indices is the shared u16 index
// buffer.
void grabarLote(const RenderPassEncoder& pase, const BindGroup& estatico, const BindGroup& uniformes,
                const Buffer& indices, std::vector<Orden>&& lote);

}  // namespace lmnx::gpu::directo
