#include "runtime.hpp"
#include <switch.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <limits>
namespace lmnx::plataforma {
namespace {
uint32_t be32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
bool leer(FILE* archivo, uint64_t offset, void* destino, size_t bytes, uint64_t longitud) {
    if (offset > longitud || bytes > longitud - offset || offset > uint64_t(std::numeric_limits<off_t>::max()))
        return false;
    return fseeko(archivo, static_cast<off_t>(offset), SEEK_SET) == 0 && fread(destino, 1, bytes, archivo) == bytes;
}
}
bool prepararDirectorios(std::string& error) {
    for (const auto* nombre : {raiz, guardados, "sdmc:/switch/LM-NX/cache", "sdmc:/switch/LM-NX/logs"}) {
        std::error_code codigo;
        std::filesystem::create_directories(nombre, codigo);
        if (codigo) { error = std::string("Could not create ") + nombre + ": " + codigo.message(); return false; }
    }
    return true;
}
bool extraerDol(const char* ruta, std::vector<uint8_t>& dol, std::string& error) {
    dol.clear();
    FILE* archivo = fopen(ruta, "rb");
    if (!archivo) { error = "Could not open the user's ISO"; return false; }
    struct Cerrar { FILE* f; ~Cerrar() { fclose(f); } } cierre{archivo};
    if (fseeko(archivo, 0, SEEK_END) != 0) { error = "Could not determine ISO size"; return false; }
    const auto fin = ftello(archivo);
    if (fin < 0) { error = "Longitud de ISO invalida"; return false; }
    const uint64_t longitud = uint64_t(fin);
    std::array<uint8_t, 0x440> cabecera{};
    if (!leer(archivo, 0, cabecera.data(), cabecera.size(), longitud) ||
        std::memcmp(cabecera.data(), "GLME01", 6) || cabecera[7] != 0 ||
        be32(cabecera.data() + 0x1C) != 0xC2339F3D) {
        error = "An uncompressed GameCube GLME01 revision 0 ISO is required"; return false;
    }
    const uint64_t inicio = be32(cabecera.data() + 0x420);
    std::array<uint8_t, 0x100> dh{};
    if (!leer(archivo, inicio, dh.data(), dh.size(), longitud)) { error = "DOL header outside ISO"; return false; }
    uint64_t bytes = dh.size();
    for (unsigned i = 0; i < 18; ++i) {
        const uint64_t offset = be32(dh.data() + 4 * i);
        const uint64_t tamano = be32(dh.data() + 0x90 + 4 * i);
        if (tamano && offset < dh.size()) { error = "DOL section overlaps its header"; return false; }
        if (tamano && offset + tamano > bytes) bytes = offset + tamano;
    }
    if (bytes > 32 * 1024 * 1024 || inicio > longitud || bytes > longitud - inicio) {
        error = "DOL sections out of bounds"; return false;
    }
    dol.resize(size_t(bytes));
    if (!leer(archivo, inicio, dol.data(), dol.size(), longitud)) {
        dol.clear(); error = "Incomplete DOL read"; return false;
    }
    return true;
}
bool configurarHilo(Rol rol) {
    const int core = rol == Rol::Juego ? 0 : rol == Rol::Render ? 1 : 2;
    const int prioridad = rol == Rol::Audio ? 0x2B : rol == Rol::Compilador ? 0x3B : 0x2C;
    uint64_t permitidos = 0;
    if (R_FAILED(svcGetInfo(&permitidos, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0))) return false;
    const auto mascara = permitidos & 7;
    if (!mascara) return false;
    const int preferido = mascara & (1u << core) ? core : __builtin_ctzll(mascara);
    return R_SUCCEEDED(svcSetThreadPriority(CUR_THREAD_HANDLE, prioridad)) &&
        R_SUCCEEDED(svcSetThreadCoreMask(CUR_THREAD_HANDLE, preferido, mascara));
}
}
