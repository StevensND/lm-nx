#include <lmnx/backend.hpp>
#include <lmnx/cache_format.hpp>
#include <lmnx/cache_paquete.hpp>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <chrono>

#ifndef LMNX_SHADER_BUILD_ID
#error "Define LMNX_SHADER_BUILD_ID with the compiler and backend fingerprint"
#endif
namespace lmnx {
namespace {
std::filesystem::path raiz;
std::mutex cerrojo;
EstadisticasCacheShaders estadisticas;
PaqueteCache paquete;  // DKSH in one file (cache_paquete.hpp).
uint64_t clavePaquete(uint32_t etapa, uint64_t hash) { return hash ^ (uint64_t(etapa) << 59); }
struct Medir {
    uint64_t& total;
    std::chrono::steady_clock::time_point inicio=std::chrono::steady_clock::now();
    ~Medir(){total+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-inicio).count();}
};
constexpr size_t limite = 12 * 1024 * 1024 + 28;
std::filesystem::path ruta(uint32_t etapa, uint64_t hash) {
    char nombre[48];
    std::snprintf(nombre, sizeof nombre, "s%u-%016llx.lmc", unsigned(etapa), static_cast<unsigned long long>(hash));
    return raiz / nombre;
}
}
void abrirCacheShaders(const std::filesystem::path& carpeta) {
    std::lock_guard lock(cerrojo);
    estadisticas={};
    raiz = carpeta / ("lmnx-f447-gxabi2-" LMNX_SHADER_BUILD_ID);
    std::error_code error;
    std::filesystem::create_directories(raiz, error);
    if (error) { estadisticas.broken=true; plume::SetDeko3DShaderCache({}, {}); return; }
    paquete.abrir(raiz / "paquete.lmp");
    for(auto it=std::filesystem::directory_iterator(raiz,error); !error && it!=std::filesystem::directory_iterator();it.increment(error))
        if(it->is_regular_file(error) && it->path().extension()==".lmc")++estadisticas.entriesAtOpen;
    plume::SetDeko3DShaderCache([](uint32_t etapa, uint64_t hash, const std::string& fuente, std::vector<uint8_t>& datos) {
        std::lock_guard lock(cerrojo);
        Medir medir{estadisticas.loadNs};
        // Count as a miss until content, source and full CRC validate.
        ++estadisticas.misses;
        {
            std::vector<uint8_t> contenido;
            if (paquete.buscar(clavePaquete(etapa, hash), contenido) && desempaquetarCache(etapa, fuente, contenido, datos)) {
                --estadisticas.misses; ++estadisticas.hits; return true;
            }
        }
        // Read a legacy loose entry and import it into the package.
        std::ifstream archivo(ruta(etapa, hash), std::ios::binary | std::ios::ate);
        if (!archivo) return false;
        const auto bytes = archivo.tellg();
        if (bytes < 28 || bytes > static_cast<std::streamoff>(limite)) return false;
        std::vector<uint8_t> contenido(static_cast<size_t>(bytes));
        archivo.seekg(0);
        archivo.read(reinterpret_cast<char*>(contenido.data()), bytes);
        const bool valida=bool(archivo) && desempaquetarCache(etapa, fuente, contenido, datos);
        if(valida){--estadisticas.misses;++estadisticas.hits;paquete.guardar(clavePaquete(etapa, hash), contenido);}
        return valida;
    }, [](uint32_t etapa, uint64_t hash, const std::string& fuente, const std::vector<uint8_t>& datos) {
        std::lock_guard lock(cerrojo);
        Medir medir{estadisticas.storeNs};
        const auto contenido = empaquetarCache(etapa, fuente, datos);
        if (contenido.empty()) return;
        paquete.guardar(clavePaquete(etapa, hash), contenido);
        ++estadisticas.stores;
    });
}
EstadisticasCacheShaders estadisticasCacheShaders() {
    std::lock_guard lock(cerrojo);return estadisticas;
}
void cerrarCacheShaders() {
    // The device and worker must have finished first.
    plume::SetDeko3DShaderCache({}, {});
    paquete.cerrar();
}
void podarCacheShaders(uint64_t limiteBytes) {
    std::lock_guard lock(cerrojo);
    if(raiz.empty())return;
    struct Entrada { std::filesystem::path ruta; uint64_t bytes; std::filesystem::file_time_type fecha; };
    std::vector<Entrada> entradas;uint64_t total=0;std::error_code ec;
    for(auto it=std::filesystem::directory_iterator(raiz,ec); !ec && it!=std::filesystem::directory_iterator();it.increment(ec)) {
        if(!it->is_regular_file(ec) || it->path().extension()!=".lmc")continue;
        auto n=it->file_size(ec);auto fecha=it->last_write_time(ec);if(ec)break;
        total+=n;entradas.push_back({it->path(),n,fecha});
    }
    std::sort(entradas.begin(),entradas.end(),[](auto& a,auto& b){return a.fecha<b.fecha;});
    for(auto& e:entradas) {if(total<=limiteBytes)break;if(std::filesystem::remove(e.ruta,ec))total-=e.bytes;ec.clear();}
}
void borrarCacheShaders() {
    podarCacheShaders(0);
    std::lock_guard lock(cerrojo);
    if(raiz.empty())return;
    paquete.cerrar();std::error_code ec;std::filesystem::remove(raiz / "paquete.lmp", ec);paquete.abrir(raiz / "paquete.lmp");
}
}
