#pragma once
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace lmnx {
inline std::string prepararGLSL(std::string fuente, uint32_t ubo, uint32_t ssbo) {
    const size_t nl = fuente.find('\n');
    if (fuente.size() > 4 * 1024 * 1024 || fuente.find('\0') != std::string::npos ||
        !fuente.starts_with("#version ") || nl == std::string::npos || fuente.find("// banjo-dk") != std::string::npos ||
        (ubo & ~0xffffu) || (ssbo & ~0x3fffu)) throw std::runtime_error("Invalid GLSL or binding masks");
    char metadatos[96];
    std::snprintf(metadatos, sizeof metadatos, "// banjo-dk ubo=%x ssbo=%x tex=0\n", ubo, ssbo);
    fuente.insert(nl + 1, metadatos);
    return fuente;
}
inline uint64_t hashFuente(const std::string& fuente) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : fuente) h = (h ^ c) * 0x100000001b3ull;
    return h;
}
}
