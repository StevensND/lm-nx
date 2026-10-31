#pragma once
#include "diagnostico_estado.hpp"
// Readback instrumentation shared by the reference and native paths. Included only on PC; the tools do not run
// the game.
#include <png.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
namespace lmnx::diagnostico {
inline uint64_t objetivo=[] { const char* e=std::getenv("LMNX_VOLCADO");return e?std::strtoull(e,nullptr,10):UINT64_MAX; }();
inline uint64_t fotograma=UINT64_MAX, secuencia=0, reservados=0;
inline std::vector<std::function<void()>> mapas;
inline std::mutex escritura;
inline std::filesystem::path carpeta;
inline void iniciar(uint64_t n) {
    fotograma=n;
    if(n==UINT64_MAX || n!=objetivo)return;
    carpeta=std::filesystem::path("volcados")/("fotograma-"+std::to_string(n));
    std::error_code ec;std::filesystem::create_directories(carpeta,ec);
    std::fprintf(stderr,"[volcado] game frame %llu, directory %s\n",(unsigned long long)n,carpeta.string().c_str());
}
inline bool activo(){return fotograma==objetivo && objetivo!=UINT64_MAX;}
inline void registro(const std::string& texto) {
    if(!activo())return;std::lock_guard lock(escritura);
    if(auto* f=std::fopen((carpeta/"pases.log").string().c_str(),"a")){std::fprintf(f,"%s\n",texto.c_str());std::fclose(f);}
}
inline void textura(const wgpu::Device& device,const wgpu::CommandEncoder& encoder,const wgpu::Texture& textura,const std::string& etiqueta) try {
    if(!activo() || !textura)return;
    const auto formato=textura.GetFormat();const uint32_t w=textura.GetWidth(),h=textura.GetHeight();
    const uint64_t id=secuencia++;
    registro(std::to_string(id)+" "+etiqueta+" format="+std::to_string(uint32_t(formato))+" size="+std::to_string(w)+"x"+std::to_string(h)+" samples="+std::to_string(textura.GetSampleCount()));
    const bool rgba=formato==wgpu::TextureFormat::RGBA8Unorm || formato==wgpu::TextureFormat::RGBA8UnormSrgb;
    const bool bgra=formato==wgpu::TextureFormat::BGRA8Unorm || formato==wgpu::TextureFormat::BGRA8UnormSrgb;
    const bool z32=formato==wgpu::TextureFormat::Depth32Float;
    const bool r8=formato==wgpu::TextureFormat::R8Unorm;
    const bool rg8=formato==wgpu::TextureFormat::RG8Unorm;
    if((!rgba&&!bgra&&!z32&&!r8&&!rg8) || textura.GetSampleCount()!=1){registro("  readback skipped: diagnostic conversion unavailable for this format or MSAA");return;}
    const uint32_t bpp=r8?1:rg8?2:4,pitch=(w*bpp+255)&~255u;
    const uint64_t bytes=uint64_t(pitch)*h;
    const bool presentada=etiqueta.starts_with("presentada-");
    const uint64_t limite=uint64_t(presentada?192:160)*1024*1024;
    if(!w || !h || bytes>limite || reservados+bytes>limite || (id>=126&&!presentada)){registro("  readback skipped: diagnostic limit (128 images / 192 MiB)");return;}
    reservados+=bytes;
    const wgpu::BufferDescriptor bd{.label="LMNX volcado",.usage=wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst,.size=bytes};
    const auto buffer=device.CreateBuffer(&bd);
    const wgpu::TexelCopyTextureInfo origen{.texture=textura};
    const wgpu::TexelCopyBufferInfo destino{.layout={.bytesPerRow=pitch,.rowsPerImage=h},.buffer=buffer};
    const wgpu::Extent3D size{w,h,1};encoder.CopyTextureToBuffer(&origen,&destino,&size);
    const auto ruta=carpeta/(std::to_string(id)+".png");
    mapas.emplace_back([=]{buffer.MapAsync(wgpu::MapMode::Read,0,bytes,wgpu::CallbackMode::AllowSpontaneous,
      [=](wgpu::MapAsyncStatus status,wgpu::StringView mensaje){
        if(status!=wgpu::MapAsyncStatus::Success){std::fprintf(stderr,"[volcado] mapping failed %s\n",ruta.string().c_str());return;}
        try {
        const auto* datos=static_cast<const uint8_t*>(buffer.GetConstMappedRange(0,bytes));
        std::vector<uint8_t> pixeles(size_t(w)*h*4,255);
        for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){const auto* s=datos+size_t(y)*pitch+x*bpp;auto* d=pixeles.data()+(size_t(y)*w+x)*4;
          if(z32){float v;std::memcpy(&v,s,4);uint8_t g=uint8_t(std::clamp(std::isfinite(v)?v:0.f,0.f,1.f)*255);d[0]=d[1]=d[2]=g;}
          else if(rgba||bgra){d[0]=s[bgra?2:0];d[1]=s[1];d[2]=s[bgra?0:2];d[3]=s[3];}
          else {d[0]=s[0];d[1]=rg8?s[1]:s[0];d[2]=r8?s[0]:0;}
        }
        buffer.Unmap();png_image imagen{};imagen.version=PNG_IMAGE_VERSION;imagen.width=w;imagen.height=h;imagen.format=PNG_FORMAT_RGBA;
        std::lock_guard lock(escritura);
        unsigned minimo[4]={255,255,255,255},maximo[4]={};uint64_t visibles=0;
        for(size_t i=0;i<pixeles.size();i+=4){if(pixeles[i]||pixeles[i+1]||pixeles[i+2])++visibles;for(unsigned k=0;k<4;++k){minimo[k]=std::min(minimo[k],unsigned(pixeles[i+k]));maximo[k]=std::max(maximo[k],unsigned(pixeles[i+k]));}}
        if(auto* f=std::fopen((ruta.parent_path()/"estadisticas.log").string().c_str(),"a")){
            std::fprintf(f,"%s rgb_no_negro=%llu min=%u,%u,%u,%u max=%u,%u,%u,%u\n",ruta.filename().string().c_str(),(unsigned long long)visibles,minimo[0],minimo[1],minimo[2],minimo[3],maximo[0],maximo[1],maximo[2],maximo[3]);std::fclose(f);
        }
        if(!png_image_write_to_file(&imagen,ruta.string().c_str(),0,pixeles.data(),0,nullptr))std::fprintf(stderr,"[volcado] PNG: %s\n",imagen.message);
        png_image_free(&imagen);
        } catch(const std::exception& e) {
            std::fprintf(stderr,"[volcado] readback/PNG skipped: %s\n",e.what());
            try {buffer.Unmap();}catch(...){}
        } catch(...) {std::fprintf(stderr,"[volcado] readback/PNG skipped: unknown exception\n");try{buffer.Unmap();}catch(...){} }
      });});
} catch(const std::exception& e) {
    std::fprintf(stderr,"[volcado] capture skipped: %s\n",e.what());
} catch(...) {
    std::fprintf(stderr,"[volcado] capture skipped: unknown exception\n");
}
inline void despuesDeSubmit(){auto pendientes=std::move(mapas);mapas.clear();for(auto& f:pendientes)f();}
}
