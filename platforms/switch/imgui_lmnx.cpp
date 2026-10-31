// Dear ImGui to the native LMNX producer. All draws use deko3d.
#include "imgui_lmnx.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>
namespace lmnx::imgui {
namespace {
gpu::Device dispositivo;
gpu::RenderPipeline pipeline;
gpu::Sampler sampler;
gpu::Texture fuente;
std::vector<std::unique_ptr<gpu::TextureView>> vistas;
// Known limit: each menu size change in ui_scale.cpp rebuilds the font atlas and registers another view. Old
// views remain until shutdown, using a few hundred KB each. Retiring them requires finishing pending
// render-worker ImGui lists, which resolve TexID asynchronously through `vistas`.
constexpr const char* shader=R"(
struct Proyeccion { escala: vec2f, traslado: vec2f };
@group(0) @binding(0) var<uniform> proyeccion: Proyeccion;
@group(0) @binding(1) var filtro: sampler;
@group(0) @binding(2) var imagen: texture_2d<f32>;
struct Salida { @builtin(position) posicion: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };
@vertex fn vs_main(@location(0) posicion: vec2f, @location(1) uv: vec2f, @location(2) color: vec4f) -> Salida {
  var s: Salida; s.posicion=vec4f(posicion*proyeccion.escala+proyeccion.traslado,0.,1.); s.uv=uv; s.color=color; return s;
}
@fragment fn fs_main(s: Salida) -> @location(0) vec4f { return s.color*textureSample(imagen,filtro,s.uv); }
)";
gpu::Buffer cargar(const void* datos,size_t n,gpu::BufferUsage uso) {
    gpu::BufferDescriptor d{.usage=uso,.size=(n+3)&~size_t(3),.mappedAtCreation=true};auto b=dispositivo.CreateBuffer(&d);
    std::memcpy(b.GetMappedRange(),datos,n);b.Unmap();return b;
}
}
ImTextureID registrarVista(const gpu::TextureView& v) {
    vistas.push_back(std::make_unique<gpu::TextureView>(v));return ImTextureID(reinterpret_cast<uintptr_t>(vistas.back().get()));
}
void iniciar(const gpu::Device& device,gpu::TextureFormat formato) {
    dispositivo=device;
    ImGuiIO& io=ImGui::GetIO();io.BackendRendererName="LMNX-Deko3D";io.BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset;
    gpu::ShaderSourceWGSL codigo{{.code=shader}};gpu::ShaderModuleDescriptor md{.nextInChain=&codigo,.label="ImGui LMNX"};auto modulo=device.CreateShaderModule(&md);
    const std::array atributos{
        gpu::VertexAttribute{.format=gpu::VertexFormat::Float32x2,.offset=offsetof(ImDrawVert,pos),.shaderLocation=0},
        gpu::VertexAttribute{.format=gpu::VertexFormat::Float32x2,.offset=offsetof(ImDrawVert,uv),.shaderLocation=1},
        gpu::VertexAttribute{.format=gpu::VertexFormat::Unorm8x4,.offset=offsetof(ImDrawVert,col),.shaderLocation=2}};
    gpu::VertexBufferLayout vb{.stepMode=gpu::VertexStepMode::Vertex,.arrayStride=sizeof(ImDrawVert),.attributeCount=atributos.size(),.attributes=atributos.data()};
    gpu::BlendState mezcla{.color={.operation=gpu::BlendOperation::Add,.srcFactor=gpu::BlendFactor::SrcAlpha,.dstFactor=gpu::BlendFactor::OneMinusSrcAlpha},
        .alpha={.operation=gpu::BlendOperation::Add,.srcFactor=gpu::BlendFactor::One,.dstFactor=gpu::BlendFactor::OneMinusSrcAlpha}};
    gpu::ColorTargetState target{.format=formato,.blend=&mezcla};gpu::FragmentState fs{.module=modulo,.entryPoint="fs_main",.targetCount=1,.targets=&target};
    gpu::RenderPipelineDescriptor desc{.label="ImGui LMNX",.vertex={.module=modulo,.entryPoint="vs_main",.bufferCount=1,.buffers=&vb},.fragment=&fs};
    pipeline=device.CreateRenderPipeline(&desc);
    gpu::SamplerDescriptor sd{.magFilter=gpu::FilterMode::Linear,.minFilter=gpu::FilterMode::Linear};sampler=device.CreateSampler(&sd);
    prepararFuentes();
}
void prepararFuentes() {
    auto* atlas=ImGui::GetIO().Fonts;if(atlas->IsBuilt() && atlas->TexID)return;
    unsigned char* pixels=nullptr;int w=0,h=0;atlas->GetTexDataAsRGBA32(&pixels,&w,&h);
    gpu::TextureDescriptor d{.usage=gpu::TextureUsage::TextureBinding|gpu::TextureUsage::CopyDst,.size={uint32_t(w),uint32_t(h),1},.format=gpu::TextureFormat::RGBA8Unorm};
    fuente=dispositivo.CreateTexture(&d);uint32_t pitch=(uint32_t(w)*4+255)&~255u;std::vector<uint8_t> datos(size_t(pitch)*h);
    for(int y=0;y<h;++y)std::memcpy(datos.data()+size_t(y)*pitch,pixels+size_t(y)*w*4,size_t(w)*4);
    gpu::TexelCopyTextureInfo t{.texture=fuente};gpu::TexelCopyBufferLayout l{.bytesPerRow=pitch,.rowsPerImage=uint32_t(h)};gpu::Extent3D n{uint32_t(w),uint32_t(h),1};
    dispositivo.GetQueue().WriteTexture(&t,datos.data(),datos.size(),&l,&n);atlas->SetTexID(registrarVista(fuente.CreateView()));
    std::fprintf(stderr,"[imgui] fuente RGBA8 cargada: %dx%d, paso=%u, bytes=%zu\n",w,h,pitch,datos.size());std::fflush(stderr);
}
void cerrar() {
    if(ImGui::GetCurrentContext()){auto& io=ImGui::GetIO();io.Fonts->SetTexID(0);io.BackendRendererName=nullptr;io.BackendFlags&=~ImGuiBackendFlags_RendererHasVtxOffset;}
    vistas.clear();fuente={};sampler={};pipeline={};dispositivo={};
}
void dibujar(const gpu::RenderPassEncoder& pass,ImDrawData* data) {
    if(!data || data->TotalVtxCount==0 || data->DisplaySize.x<=0 || data->DisplaySize.y<=0)return;
    const int width=int(data->DisplaySize.x*data->FramebufferScale.x),height=int(data->DisplaySize.y*data->FramebufferScale.y);
    static bool informado=false;
    if(!informado){informado=true;std::fprintf(stderr,"[imgui] primer pase: %dx%d, listas=%d, vertices=%d, indices=%d, fuente=%s\n",width,height,data->CmdListsCount,data->TotalVtxCount,data->TotalIdxCount,fuente?"si":"no");std::fflush(stderr);}
    float proyeccion[4]={2.f/data->DisplaySize.x,-2.f/data->DisplaySize.y,-1.f-data->DisplayPos.x*2.f/data->DisplaySize.x,1.f+data->DisplayPos.y*2.f/data->DisplaySize.y};
    auto uniform=cargar(proyeccion,sizeof proyeccion,gpu::BufferUsage::Uniform);
    auto estado=[&]{pass.SetPipeline(pipeline);pass.SetViewport(0,0,float(width),float(height),0,1);};estado();
    for(int k=0;k<data->CmdListsCount;++k) {
        const auto* list=data->CmdLists[k];auto vertices=cargar(list->VtxBuffer.Data,list->VtxBuffer.Size*sizeof(ImDrawVert),gpu::BufferUsage::Vertex);
        auto indices=cargar(list->IdxBuffer.Data,list->IdxBuffer.Size*sizeof(ImDrawIdx),gpu::BufferUsage::Index);
        pass.SetVertexBuffer(0,vertices);pass.SetIndexBuffer(indices,sizeof(ImDrawIdx)==2?gpu::IndexFormat::Uint16:gpu::IndexFormat::Uint32);
        for(const auto& cmd:list->CmdBuffer) {
            if(cmd.UserCallback){if(cmd.UserCallback==ImDrawCallback_ResetRenderState)estado();else cmd.UserCallback(list,&cmd);continue;}
            auto id=cmd.GetTexID();auto it=std::find_if(vistas.begin(),vistas.end(),[&](auto& v){return ImTextureID(reinterpret_cast<uintptr_t>(v.get()))==id;});
            if(it==vistas.end())throw std::runtime_error("Unknown ImGui texture in LMNX");
            const int x0=std::clamp(int((cmd.ClipRect.x-data->DisplayPos.x)*data->FramebufferScale.x),0,width),y0=std::clamp(int((cmd.ClipRect.y-data->DisplayPos.y)*data->FramebufferScale.y),0,height);
            const int x1=std::clamp(int((cmd.ClipRect.z-data->DisplayPos.x)*data->FramebufferScale.x),x0,width),y1=std::clamp(int((cmd.ClipRect.w-data->DisplayPos.y)*data->FramebufferScale.y),y0,height);
            if(x1==x0 || y1==y0)continue;
            const std::array entradas{gpu::BindGroupEntry{.binding=0,.buffer=uniform,.size=16},gpu::BindGroupEntry{.binding=1,.sampler=sampler},gpu::BindGroupEntry{.binding=2,.textureView=**it}};
            gpu::BindGroupDescriptor bg{.layout=pipeline.GetBindGroupLayout(0),.entryCount=entradas.size(),.entries=entradas.data()};auto grupo=dispositivo.CreateBindGroup(&bg);
            pass.SetBindGroup(0,grupo);pass.SetScissorRect(x0,y0,x1-x0,y1-y0);pass.DrawIndexed(cmd.ElemCount,1,cmd.IdxOffset,cmd.VtxOffset);
        }
    }
}
}
