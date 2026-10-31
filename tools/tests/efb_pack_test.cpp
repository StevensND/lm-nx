#include <lmnx/efb_pack.hpp>
#include <cassert>
#include <cstdio>
int main() {
    using namespace lmnx::gfx;
    const uint8_t rojo[] = {255, 0, 0, 255};
    auto gx = empaquetarEFB(rojo, 1, 1, FormatoLectura::GX_RGBA8);
    assert(gx.size() == 64);
    for (size_t i = 0; i < 32; i += 2) assert(gx[i] == 255 && gx[i+1] == 255 && gx[i+32] == 0 && gx[i+33] == 0);
    auto rgb = empaquetarEFB(rojo, 1, 1, FormatoLectura::GX_RGB565);
    assert(rgb.size() == 32);
    for (size_t i = 0; i < 32; i += 2) assert(rgb[i] == 0xf8 && rgb[i+1] == 0);
    std::vector<uint8_t> imagen(5 * 5 * 4);
    for (int y = 0; y < 5; ++y) for (int x = 0; x < 5; ++x) {
        size_t p = (y * 5 + x) * 4;
        imagen[p] = x; imagen[p+1] = y; imagen[p+2] = 91; imagen[p+3] = 128;
    }
    gx = empaquetarEFB(imagen, 5, 5, FormatoLectura::GX_RGBA8);
    assert(gx.size() == 256);
    // Four blocks at (0,0), (4,0), (0,4), (4,4).
    assert(gx[0] == 128 && gx[1] == 0 && gx[32] == 0 && gx[33] == 91);
    assert(gx[64+1] == 4 && gx[128+32] == 4 && gx[192+1] == 4 && gx[192+32] == 4);
    assert(gx[254] == 4 && gx[255] == 91); // Replicate the last texel.
    bool rechazo = false;
    try { empaquetarEFB(imagen, 6, 5, FormatoLectura::GX_RGBA8); } catch (...) { rechazo = true; }
    assert(rechazo);
    std::puts("EFB: AR/GB, big-endian RGB565, 4x4 order, edges and bounds correct with synthetic pixels.");
}
