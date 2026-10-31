#include <lmnx/cache_format.hpp>
#include <lmnx/shader_source.hpp>
#include <cassert>
#include <cstdio>

int main() {
    // Structural DKSH without executable instructions; never loaded onto the GPU.
    std::vector<uint8_t> d(512);
    auto poner = [&](size_t p, uint32_t w) { for (int i = 0; i < 4; ++i) d[p+i] = uint8_t(w >> (8*i)); };
    poner(0, 0x48534b44); poner(4, 24); poner(8, 256); poner(12, 256); poner(16, 24); poner(20, 1);
    poner(24, 0); poner(28, 0); poner(32, 4);
    const std::string s = lmnx::prepararGLSL("#version 460\nvoid main() {}\n", 0, 0);
    auto paquete = lmnx::empaquetarCache(0, s, d);
    std::vector<uint8_t> salida;
    assert(!paquete.empty() && lmnx::desempaquetarCache(0, s, paquete, salida) && salida == d);
    assert(!lmnx::desempaquetarCache(4, s, paquete, salida) && salida.empty());
    assert(!lmnx::desempaquetarCache(0, s + " ", paquete, salida));
    for (size_t i = 0; i < paquete.size(); ++i) {
        auto roto = paquete; roto[i] ^= 1;
        assert(!lmnx::desempaquetarCache(0, s, roto, salida) && salida.empty());
    }
    for (size_t n = 0; n < paquete.size(); ++n) {
        auto corto = paquete; corto.resize(n);
        assert(!lmnx::desempaquetarCache(0, s, corto, salida));
    }
    auto largo = paquete; largo.push_back(0);
    assert(!lmnx::desempaquetarCache(0, s, largo, salida));
    assert(lmnx::crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926);
    assert(lmnx::hashFuente("") == 0xcbf29ce484222325ull);
    poner(24, 1); // DKSH fragment=1, DkStage fragment=4.
    assert(!lmnx::empaquetarCache(4, s, d).empty());
    assert(lmnx::empaquetarCache(1, s, d).empty());
    std::puts("Cache: round trip, stage/source, known CRC, per-byte corruption and all truncations checked.");
}
