#pragma once
#include "dksh.hpp"
#include <cstring>
#include <string_view>
#include <vector>

namespace lmnx {
inline uint32_t crc32(const uint8_t* datos, size_t n) {
    uint32_t crc = ~0u;
    for (size_t i = 0; i < n; ++i) {
        crc ^= datos[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
// Little-endian. Store full GLSL so an FNV name collision only causes a cache miss and never executes another
// shader.
inline std::vector<uint8_t> empaquetarCache(uint32_t etapa, std::string_view fuente,
                                          const std::vector<uint8_t>& dksh) {
    if (etapa > 5 || fuente.empty() || fuente.size() > 4 * 1024 * 1024 || !validarDKSH(dksh.data(), dksh.size(), int(etapa))) return {};
    const uint32_t palabras[] = {0x32434d4c, 1, etapa, uint32_t(fuente.size()), uint32_t(dksh.size()),
        crc32(reinterpret_cast<const uint8_t*>(fuente.data()), fuente.size()), crc32(dksh.data(), dksh.size())};
    std::vector<uint8_t> salida;
    for (uint32_t w : palabras) for (int i = 0; i < 4; ++i) salida.push_back(uint8_t(w >> (8 * i)));
    salida.insert(salida.end(), fuente.begin(), fuente.end());
    salida.insert(salida.end(), dksh.begin(), dksh.end());
    return salida;
}
inline bool desempaquetarCache(uint32_t etapa, std::string_view fuente, const std::vector<uint8_t>& archivo,
                              std::vector<uint8_t>& dksh) {
    dksh.clear();
    if (archivo.size() < 28 || archivo.size() > 12 * 1024 * 1024 + 28) return false;
    auto word = [&](size_t p) { return uint32_t(archivo[p]) | uint32_t(archivo[p+1]) << 8 |
        uint32_t(archivo[p+2]) << 16 | uint32_t(archivo[p+3]) << 24; };
    const uint64_t ns = word(12), nd = word(16);
    if (word(0) != 0x32434d4c || word(4) != 1 || word(8) != etapa || etapa > 5 ||
        ns != fuente.size() || !ns || ns > 4 * 1024 * 1024 || nd > 8 * 1024 * 1024 || 28 + ns + nd != archivo.size()) return false;
    const uint8_t* s = archivo.data() + 28;
    const uint8_t* d = s + ns;
    if (std::memcmp(s, fuente.data(), ns) || crc32(s, ns) != word(20) || crc32(d, nd) != word(24) ||
        !validarDKSH(d, nd, int(etapa))) return false;
    dksh.assign(d, d + nd);
    return true;
}
}
