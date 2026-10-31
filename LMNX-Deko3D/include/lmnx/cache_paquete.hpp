#pragma once
// Store shaders in one file, like SwitchWaker's initial_dksh_cache.bin, to avoid opening thousands of individual
// SD files. Read the package into one memory block; the index stores entry offsets to avoid per-entry
// allocations and heap fragmentation. Append records as LMPK, a 64-bit key, a 32-bit length and data. Ignore
// incomplete trailing records. Each entry validates its source and CRC, so key collisions cause only misses.
// Read legacy loose entries and import them on first use.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lmnx {
class PaqueteCache {
public:
    ~PaqueteCache() { cerrar(); }
    void abrir(const std::filesystem::path& archivo) {
        std::lock_guard lock(cerrojo);
        cerrarSinCerrojo();
        base.clear(); base.shrink_to_fit(); indice.clear(); nuevos.clear();
        ruta = archivo;
        if (ruta.empty()) return;
        size_t total = 0, valido = 0;
        if (FILE* f = std::fopen(ruta.string().c_str(), "rb")) {
            std::fseek(f, 0, SEEK_END);
            const long bytes = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            total = bytes > 0 ? size_t(bytes) : 0;
            base.resize(total);
            total = total ? std::fread(base.data(), 1, total, f) : 0;
            std::fclose(f);
            base.resize(total);
            indice.reserve(total / 2048 + 16);
            size_t p = 0;
            while (p + kCabecera <= total) {
                uint32_t magia, n; uint64_t clave;
                std::memcpy(&magia, &base[p], 4); std::memcpy(&clave, &base[p + 4], 8); std::memcpy(&n, &base[p + 12], 4);
                if (magia != kMagia || n > total - p - kCabecera) break;
                indice[clave] = {uint32_t(p + kCabecera), n};
                p += kCabecera + n;
            }
            valido = p;
        }
        // Incomplete trailing record: rewrite valid records before appending.
        if (total != valido) reescribir();
        salida = std::fopen(ruta.string().c_str(), "ab");
        if (salida) std::setvbuf(salida, nullptr, _IOFBF, 256 * 1024);
        entradasAlAbrir = indice.size();
    }
    void cerrar() { std::lock_guard lock(cerrojo); cerrarSinCerrojo(); }
    bool buscar(uint64_t clave, std::vector<uint8_t>& salidaDatos) {
        std::lock_guard lock(cerrojo);
        if (const auto it = nuevos.find(clave); it != nuevos.end()) { salidaDatos = it->second; return true; }
        const auto it = indice.find(clave);
        if (it == indice.end()) return false;
        salidaDatos.assign(base.begin() + it->second.first, base.begin() + it->second.first + it->second.second);
        return true;
    }
    void guardar(uint64_t clave, const std::vector<uint8_t>& bytes) {
        std::lock_guard lock(cerrojo);
        if (const auto it = nuevos.find(clave); it != nuevos.end() && it->second == bytes) return;
        if (const auto it = indice.find(clave); it != indice.end() && it->second.second == bytes.size() &&
            std::memcmp(base.data() + it->second.first, bytes.data(), bytes.size()) == 0) return;
        nuevos[clave] = bytes;
        if (!salida) return;
        escribir(salida, clave, bytes.data(), uint32_t(bytes.size()));
        // Write in batches because each SD write is a synchronous system request.
        const auto ahora = std::chrono::steady_clock::now();
        if (++pendientes >= 64 || ahora - ultimoVaciado > std::chrono::seconds(2)) {
            std::fflush(salida); pendientes = 0; ultimoVaciado = ahora;
        }
    }
    void vaciar() { std::lock_guard lock(cerrojo); if (salida) std::fflush(salida); pendientes = 0; }
    size_t entradasIniciales() { std::lock_guard lock(cerrojo); return entradasAlAbrir; }

private:
    static constexpr uint32_t kMagia = 0x4B504D4C;  // «LMPK»
    static constexpr size_t kCabecera = 16;
    static void escribir(FILE* f, uint64_t clave, const uint8_t* datos, uint32_t n) {
        const uint32_t magia = kMagia;
        std::fwrite(&magia, 4, 1, f); std::fwrite(&clave, 8, 1, f); std::fwrite(&n, 4, 1, f);
        if (n) std::fwrite(datos, 1, n, f);
    }
    void cerrarSinCerrojo() { if (salida) { std::fclose(salida); salida = nullptr; } }
    void reescribir() {
        auto temporal = ruta; temporal += ".tmp";
        FILE* f = std::fopen(temporal.string().c_str(), "wb");
        if (!f) return;
        for (const auto& [clave, sitio] : indice) escribir(f, clave, base.data() + sitio.first, sitio.second);
        const bool ok = std::fflush(f) == 0;
        std::fclose(f);
        std::error_code ec;
        if (!ok) { std::filesystem::remove(temporal, ec); return; }
        std::filesystem::remove(ruta, ec);  // Switch SD rename does not replace an existing file.
        std::filesystem::rename(temporal, ruta, ec);
    }
    std::mutex cerrojo;
    std::vector<uint8_t> base;                                                  // Original file contents.
    std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> indice;         // clave -> (inicio, bytes) en base
    std::unordered_map<uint64_t, std::vector<uint8_t>> nuevos;                  // Appended during this session.
    std::filesystem::path ruta;
    FILE* salida = nullptr;
    size_t pendientes = 0, entradasAlAbrir = 0;
    std::chrono::steady_clock::time_point ultimoVaciado = std::chrono::steady_clock::now();
};
}  // namespace lmnx
