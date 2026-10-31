#include <cstring>
#include <cstdio>
// The native producer contract, implemented with plume/Vulkan on PC.
#include <lmnx/native_gfx.hpp>
#include "gpu_pc.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>
#include <mutex>
namespace aurora::window {SDL_Window* get_sdl_window();}
namespace lmnx {
std::unique_ptr<plume::RenderInterface> crearBackend(){return std::make_unique<plume::VulkanInterface>();}
void abrirCacheShaders(const std::filesystem::path&){} // DKSH runs only on Switch.
void cerrarCacheShaders(){}
void podarCacheShaders(uint64_t){}
void borrarCacheShaders(){}
}
namespace lmnx::gfx {
using namespace plume;
static void exigir(bool b,const char* e){if(!b){std::fprintf(stderr,"[lmnx-pc] ERROR: %s\n",e);std::fflush(stderr);throw std::runtime_error(e);}}
Pipeline::~Pipeline(){objeto.reset();}
struct Contexto::Impl {
    std::unique_ptr<RenderInterface> interfaz=crearBackend();
    std::shared_ptr<RenderDevice> dispositivo=interfaz->createDevice();
    std::unique_ptr<RenderCommandQueue> cola;
    std::unique_ptr<RenderCommandList> comandos;
    std::unique_ptr<RenderCommandFence> valla;
    std::unique_ptr<RenderSwapChain> ventana;
    std::unique_ptr<RenderCommandSemaphore> adquirida;
    std::unique_ptr<RenderQueryPool> tiempos;
    std::mutex mutex;
    Impl(){
        exigir(bool(dispositivo),"No Vulkan device");
        const auto tipo=dispositivo->getDescription().type;
        exigir(tipo==RenderDeviceType::INTEGRATED || tipo==RenderDeviceType::DISCRETE,"The test requires a physical GPU; CPU renderers and virtual GPUs are unsupported");
        const auto& limites=static_cast<VulkanDevice&>(*dispositivo).physicalDeviceProperties.limits;
        exigir(limites.minUniformBufferOffsetAlignment && 256%limites.minUniformBufferOffsetAlignment==0 &&
            limites.minStorageBufferOffsetAlignment && 16%limites.minStorageBufferOffsetAlignment==0,"GPU does not support Switch ABI alignment (UBO 256 / SSBO 16)");
        cola=dispositivo->createCommandQueue(RenderCommandListType::DIRECT);comandos=cola->createCommandList();valla=dispositivo->createCommandFence();
        auto* w=aurora::window::get_sdl_window();
        auto hwnd=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(w),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
        ventana=cola->createSwapChain({hwnd,RenderFormat::R8G8B8A8_UNORM,2});exigir(bool(ventana),"Could not create the Vulkan swapchain");
        // plume creates the Vulkan swapchain lazily in resize(), as in banjo-nx. Leave it empty until the window has a
        // size, then create it at presentation.
        ventana->resize();
        adquirida=dispositivo->createCommandSemaphore();
        if(std::getenv("LMNX_PERF_LOG") && dispositivo->getCapabilities().queryPools)tiempos=dispositivo->createQueryPool(2);
    }
};
Contexto::Contexto():impl(std::make_unique<Impl>()){}
Contexto::~Contexto()=default;
std::shared_ptr<RenderDevice> Contexto::dispositivo()const{return impl->dispositivo;}
RefTextura Contexto::crearTextura(const RenderTextureDesc& d){auto r=std::make_shared<Textura>();r->dispositivo=impl->dispositivo;r->descripcion=d;r->objeto=r->dispositivo->createTexture(d);exigir(bool(r->objeto),"PC texture was not created");return r;}
RefBuffer Contexto::crearBuffer(uint64_t n,RenderBufferFlags flags){auto r=std::make_shared<Buffer>();r->dispositivo=impl->dispositivo;r->bytes=n;r->objeto=r->dispositivo->createBuffer(RenderBufferDesc::DefaultBuffer(n,flags));return r;}
RefSampler Contexto::crearSampler(const RenderSamplerDesc& d){auto r=std::make_shared<Sampler>();r->dispositivo=impl->dispositivo;r->objeto=r->dispositivo->createSampler(d);return r;}
uint32_t Contexto::ancho()const{return impl->ventana->getWidth();}
uint32_t Contexto::alto()const{return impl->ventana->getHeight();}
void Contexto::configurarVsync(bool b){impl->ventana->setVsyncEnabled(b);}
void Contexto::esperarVblank(){impl->ventana->wait();}
void Contexto::ejecutar(const Paquete& p){
    auto& i=*impl;std::lock_guard lock(i.mutex);auto& c=*i.comandos;
    if(i.ventana->needsResize())i.ventana->resize();
    // Run commands without presenting when the window is minimized or has no size.
    const auto& presentarA=(p.presentar && !i.ventana->isEmpty())?p.presentar:decltype(p.presentar){};
    uint32_t imagen=0;if(presentarA)exigir(i.ventana->acquireTexture(i.adquirida.get(),&imagen),"Failed to acquire Vulkan image");
    c.begin();if(i.tiempos){c.resetQueryPool(i.tiempos.get(),0,2);c.writeTimestamp(i.tiempos.get(),0);}
    std::vector<std::unique_ptr<RenderBuffer>> subidas; // Keep alive until the fence wait below.
    for(const auto& op:p.operaciones) {
        // Aurora Queue::WriteBuffer arrives as CargaBuffer, as on Switch: upload buffer and copy.
        if(auto* carga=std::get_if<CargaBuffer>(&op)) {
            exigir(carga->destino && carga->destino->objeto && carga->offset+carga->datos.size()<=carga->destino->bytes,"Buffer upload out of bounds");
            if(carga->datos.empty())continue;
            // plume's mappable UPLOAD buffers lack TRANSFER_DST, so write by mapping. This is safe because each packet waits
            // for its fence and the GPU is idle between submissions.
            if(static_cast<VulkanBuffer&>(*carga->destino->objeto).desc.heapType==RenderHeapType::UPLOAD) {
                auto* m=static_cast<uint8_t*>(carga->destino->objeto->map());exigir(m!=nullptr,"Could not map the destination buffer");
                std::memcpy(m+carga->offset,carga->datos.data(),carga->datos.size());carga->destino->objeto->unmap();continue;
            }
            auto subida=i.dispositivo->createBuffer(RenderBufferDesc::UploadBuffer(carga->datos.size()));exigir(bool(subida),"Could not create the PC upload buffer");
            void* m=subida->map();exigir(m!=nullptr,"Could not map the upload buffer");std::memcpy(m,carga->datos.data(),carga->datos.size());subida->unmap();
            RenderBuffer* destino=carga->destino->objeto.get();
            const RenderBufferBarrier antes(destino,RenderBufferAccess::WRITE);c.barriers(RenderBarrierStage::COPY,&antes,1,nullptr,0);
            c.copyBufferRegion(RenderBufferReference(destino,carga->offset),RenderBufferReference(subida.get()),carga->datos.size());
            const RenderBufferBarrier despues(destino,RenderBufferAccess::READ);c.barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE,&despues,1,nullptr,0);
            subidas.push_back(std::move(subida));continue;
        }
        auto* n=std::get_if<OperacionNativa>(&op);
        exigir(n && bool(n->codificar),"PC producer must use native Aurora operations");n->codificar(c);
    }
    if(presentarA){
        auto* destino=i.ventana->getTexture(imagen);auto* origen=presentarA->objeto.get();
        c.barriers(RenderBarrierStage::ALL,RenderTextureBarrier(origen,RenderTextureLayout::COPY_SOURCE));
        c.barriers(RenderBarrierStage::ALL,RenderTextureBarrier(destino,RenderTextureLayout::COPY_DEST));
        if(presentarA->descripcion.width==ancho() && presentarA->descripcion.height==alto())c.copyTexture(destino,origen);
        else {
            auto& cmd=static_cast<VulkanCommandList&>(c);auto& src=static_cast<VulkanTexture&>(*origen);auto& dst=static_cast<VulkanTexture&>(*destino);
            VkClearColorValue negro{{0,0,0,1}};VkImageSubresourceRange rango{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            vkCmdClearColorImage(cmd.vk,dst.vk,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&negro,1,&rango);
            const uint64_t sw=presentarA->descripcion.width,sh=presentarA->descripcion.height;
            const bool ajustar=uint64_t(ancho())*sh<=uint64_t(alto())*sw;
            int w=int(ajustar?ancho():uint64_t(alto())*sw/sh),h=int(ajustar?uint64_t(ancho())*sh/sw:alto());
            int x=(int(ancho())-w)/2,y=(int(alto())-h)/2;
            VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};blit.dstSubresource=blit.srcSubresource;
            blit.srcOffsets[1]={int32_t(sw),int32_t(sh),1};blit.dstOffsets[0]={x,y,0};blit.dstOffsets[1]={x+w,y+h,1};
            vkCmdBlitImage(cmd.vk,src.vk,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,dst.vk,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
        }
        c.barriers(RenderBarrierStage::ALL,RenderTextureBarrier(destino,RenderTextureLayout::PRESENT));
    }
    if(i.tiempos)c.writeTimestamp(i.tiempos.get(),1);c.end();
    const RenderCommandList* listas[]={&c};RenderCommandSemaphore* esperas[]={i.adquirida.get()};
    i.cola->executeCommandLists(listas,1,presentarA?esperas:nullptr,presentarA?1:0,nullptr,0,i.valla.get());
    i.cola->waitForCommandFence(i.valla.get());
    uint64_t ns=UINT64_MAX;if(i.tiempos && i.tiempos->queryResults(2)){auto* t=i.tiempos->getResults();ns=t[1]-t[0];}
    if(presentarA)exigir(i.ventana->present(imagen,nullptr,0),"Vulkan presentation failed");
    lmnx::pc::registrarEnvio(ns,bool(presentarA));
}
}
