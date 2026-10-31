#include "gpu_pc.hpp"
namespace lmnx::pc {
std::unique_ptr<plume::RenderShader> compilar(plume::RenderDevice& d,const std::string& fuente,uint32_t etapa) {
    auto datos=traducirSpirv(fuente,etapa);return d.createShader(datos.data(),datos.size()*4,"main",plume::RenderShaderFormat::SPIRV);
}
void enlazarBuffer(plume::RenderDescriptorSet& s,uint32_t indice,plume::RenderBuffer& b,uint64_t offset,uint64_t bytes) {
    auto& set=static_cast<plume::VulkanDescriptorSet&>(s);auto& buffer=static_cast<plume::VulkanBuffer&>(b);
    VkDescriptorBufferInfo info{buffer.vk,offset,bytes};set.setDescriptor(indice,&info,nullptr,nullptr,nullptr);
}
}
