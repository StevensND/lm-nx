#pragma once
#include <lmnx/backend.hpp>
namespace lmnx::pc {
std::vector<uint32_t> traducirSpirv(const std::string&,uint32_t);
std::unique_ptr<plume::RenderShader> compilar(plume::RenderDevice&,const std::string&,uint32_t);
void enlazarBuffer(plume::RenderDescriptorSet&,uint32_t,plume::RenderBuffer&,uint64_t,uint64_t);
void contarDibujo();
void registrarEnvio(uint64_t gpuNs,bool presentacion);
}
