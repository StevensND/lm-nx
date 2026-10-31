#pragma once
#ifdef LMNX_PC_VULKAN
#include "plume_vulkan.h"
// Internal stage identifiers preserved in translation keys.
enum DkStage { DkStage_Vertex=0, DkStage_Fragment=4, DkStage_Compute=5 };
#else
#include "plume_deko3d.h"
#endif
#include <filesystem>

namespace lmnx {
// Configure before creating the device and close after destroying it. One device/compiler per process because
// uam is not reentrant.
void abrirCacheShaders(const std::filesystem::path& carpeta);
void cerrarCacheShaders();
struct EstadisticasCacheShaders {
    uint32_t hits=0, misses=0, stores=0;
    uint64_t loadNs=0, storeNs=0;
    bool broken=false;
    uint32_t entriesAtOpen=0;
};
EstadisticasCacheShaders estadisticasCacheShaders();
void podarCacheShaders(uint64_t limiteBytes = 256ull * 1024 * 1024);
void borrarCacheShaders();
std::unique_ptr<plume::RenderInterface> crearBackend();
}
