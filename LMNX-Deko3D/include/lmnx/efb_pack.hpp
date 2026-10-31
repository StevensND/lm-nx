#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace lmnx::gfx {
enum class FormatoLectura { Lineal, GX_RGBA8, GX_RGB565 };
// Repack pixels read from the native GPU into big-endian 4x4 blocks. Partial edges replicate the last texel, as
// Aurora efb_ram does. No TEV/EFB6 conversion, filtering, gamma or Z quantization.
inline std::vector<uint8_t> empaquetarEFB(std::span<const uint8_t> rgba, uint32_t w, uint32_t h, FormatoLectura formato) {
    if (!w || !h || w > 8192 || h > 8192 || uint64_t(w) * h * 4 != rgba.size() ||
        (formato != FormatoLectura::GX_RGBA8 && formato != FormatoLectura::GX_RGB565))
        throw std::runtime_error("Invalid EFB repacking dimensions or format");
    const uint32_t nx = (w + 3) / 4, ny = (h + 3) / 4;
    const size_t bloque = formato == FormatoLectura::GX_RGBA8 ? 64 : 32;
    std::vector<uint8_t> salida(size_t(nx) * ny * bloque);
    for (uint32_t ty = 0; ty < ny; ++ty) for (uint32_t tx = 0; tx < nx; ++tx) {
        const size_t base = (size_t(ty) * nx + tx) * bloque;
        for (uint32_t y = 0; y < 4; ++y) for (uint32_t x = 0; x < 4; ++x) {
            const size_t p = (size_t(std::min(ty * 4 + y, h - 1)) * w + std::min(tx * 4 + x, w - 1)) * 4;
            const size_t t = base + (y * 4 + x) * 2;
            if (formato == FormatoLectura::GX_RGBA8) {
                salida[t] = rgba[p + 3]; salida[t + 1] = rgba[p];
                salida[t + 32] = rgba[p + 1]; salida[t + 33] = rgba[p + 2];
            } else {
                const uint16_t v = uint16_t((rgba[p] >> 3) << 11) | uint16_t((rgba[p + 1] >> 2) << 5) | (rgba[p + 2] >> 3);
                salida[t] = uint8_t(v >> 8); salida[t + 1] = uint8_t(v);
            }
        }
    }
    return salida;
}
}
