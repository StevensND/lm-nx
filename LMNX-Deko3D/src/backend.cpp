#include <lmnx/backend.hpp>
namespace lmnx {
std::unique_ptr<plume::RenderInterface> crearBackend() {
    plume::Deko3DOptions opciones;
    // Keep Banjo's tested generic set. Wide loads at bindings 13/15 first require proof of GX ABI alignment.
    opciones.uamFlags = 447;
    opciones.vsOutputs = true;
    opciones.mergeBarriers = true;
    opciones.leanBarriers = true;
    opciones.depthCompression = true;
    // Keep Zcull disabled until decals, bias and Z copies are checked in LM.
    opciones.zcull = false;
    opciones.compileCore = 2;
    opciones.compilePriority = 0x3B;
    plume::Deko3DSetOptions(opciones);
    return plume::CreateDeko3DInterface();
}
}
