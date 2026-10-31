#pragma once
#include <cstddef>
#include <cstdint>
namespace lmnx {
// Validate structure before deko3d reads binary offsets.
inline bool validarDKSH(const uint8_t* datos, size_t bytes, int etapaEsperada = -1) {
    if (!datos || bytes < 256 || bytes > 8 * 1024 * 1024 || etapaEsperada > 5) return false;
    // DkStage and DkshProgramType use different numbering.
    constexpr uint32_t tipoPrograma[] = {0, 3, 4, 2, 1, 5};
    auto word = [&](size_t offset) -> uint32_t {
        return uint32_t(datos[offset]) | uint32_t(datos[offset+1]) << 8 |
            uint32_t(datos[offset+2]) << 16 | uint32_t(datos[offset+3]) << 24;
    };
    if (word(0) != 0x48534B44 || word(4) != 24) return false;
    const uint64_t control = word(8), codigo = word(12), programas = word(16), cantidad = word(20);
    if (control < 256 || (control & 255) || !codigo || (codigo & 255) ||
        control + codigo != bytes || programas < 24 || !cantidad ||
        programas + cantidad * 64 > control) return false;
    for (uint64_t i=0; i<cantidad; ++i) {
        const size_t p = size_t(programas + 64*i);
        if (word(p) > 5 || (etapaEsperada >= 0 && word(p) != tipoPrograma[etapaEsperada]) || word(p+4) >= codigo || word(p+8) < 4 || word(p+8) > 256 ||
            uint64_t(word(p+12)) + word(p+16) > codigo) return false;
    }
    return true;
}
}
