// Native resources and copies for the Aurora producer; no Dawn device.
#include <lmnx/aurora_gpu.hpp>
#include <lmnx/native_gfx.hpp>
#include <lmnx/shader_source.hpp>
#include "shader_translate.h"
#include <lmnx/translation_cache.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <map>
#include <stdexcept>
#ifdef LMNX_PC_VULKAN
#include "gpu_pc.hpp"
#include "diagnostico_gfx.hpp"
#endif

// Per-frame CPU measurements, shared with native_gfx.cpp and logged to SD every five seconds.
namespace lmnx::gfx { void esperarGpuGlobal(); uint64_t envioActual(); bool envioTerminado(uint64_t); void esperarEnvio(uint64_t); }
// Per-pass GPU timing (native_gfx.cpp); not measured on PC.
#ifndef LMNX_PC_VULKAN
namespace lmnx::gfx { void marcaGpu(plume::RenderCommandList&, const std::string&); }
#else
namespace lmnx::gfx { inline void marcaGpu(plume::RenderCommandList&, const std::string&) {} }
#endif
namespace lmnx::medidas { inline std::atomic<uint64_t> grabarNs{0}, dibujosGrabados{0}, ejecutarNs{0}, enviarNs{0}, presentarNs{0}, cambiosPipeline{0}, enlacesTexturas{0}, enlacesUbo{0}, lotes{0}; }
namespace lmnx::gpu {
using namespace plume;
namespace {
void exigir(bool v, const char* texto) { if (!v) throw std::runtime_error(texto); }
RenderViewport viewportNativo(float x,float y,float w,float h,float zn=0,float zf=1) {
#ifdef LMNX_PC_VULKAN
    // Same coordinate convention as Aurora: Vulkan flips the viewport. See Dawn CommandBufferVk.cpp, SetViewport.
    if(w==0){w=1;h=0;x=0;}return RenderViewport(x,y+h,w,-h,zn,zf);
#else
    return RenderViewport(x,y,w,h,zn,zf);
#endif
}
RenderFormat formato(TextureFormat f) {
    switch (f) {
#define F(a,b) case TextureFormat::a: return RenderFormat::b
    F(R8Unorm,R8_UNORM); F(R8Snorm,R8_SNORM); F(R8Uint,R8_UINT); F(R8Sint,R8_SINT);
    F(RG8Unorm,R8G8_UNORM); F(RG8Snorm,R8G8_SNORM); F(RG8Uint,R8G8_UINT); F(RG8Sint,R8G8_SINT);
    F(RGBA8Unorm,R8G8B8A8_UNORM); F(RGBA8Snorm,R8G8B8A8_SNORM); F(RGBA8Uint,R8G8B8A8_UINT); F(RGBA8Sint,R8G8B8A8_SINT);
    F(BGRA8Unorm,B8G8R8A8_UNORM);
    F(RGBA8UnormSrgb,R8G8B8A8_UNORM_SRGB); F(BGRA8UnormSrgb,B8G8R8A8_UNORM_SRGB); F(RGB10A2Unorm,R10G10B10A2_UNORM);
    F(R16Float,R16_FLOAT); F(R16Unorm,R16_UNORM); F(R16Snorm,R16_SNORM); F(R16Uint,R16_UINT); F(R16Sint,R16_SINT);
    F(RG16Float,R16G16_FLOAT); F(RG16Unorm,R16G16_UNORM); F(RG16Snorm,R16G16_SNORM); F(RG16Uint,R16G16_UINT); F(RG16Sint,R16G16_SINT);
    F(RGBA16Float,R16G16B16A16_FLOAT); F(RGBA16Unorm,R16G16B16A16_UNORM); F(RGBA16Snorm,R16G16B16A16_SNORM); F(RGBA16Uint,R16G16B16A16_UINT); F(RGBA16Sint,R16G16B16A16_SINT);
    F(R32Float,R32_FLOAT); F(R32Uint,R32_UINT); F(R32Sint,R32_SINT);
    F(RG32Float,R32G32_FLOAT); F(RG32Uint,R32G32_UINT); F(RG32Sint,R32G32_SINT);
    F(RGBA32Float,R32G32B32A32_FLOAT); F(RGBA32Uint,R32G32B32A32_UINT); F(RGBA32Sint,R32G32B32A32_SINT);
    F(Depth16Unorm,D16_UNORM); F(Depth32Float,D32_FLOAT); F(Depth32FloatStencil8,D32_FLOAT_S8_UINT);
    F(Depth24PlusStencil8,D24_UNORM_S8_UINT); F(Depth24Plus,D24_UNORM_S8_UINT); F(Stencil8,S8_UINT);
    F(BC1RGBAUnorm,BC1_UNORM); F(BC1RGBAUnormSrgb,BC1_UNORM_SRGB); F(BC2RGBAUnorm,BC2_UNORM); F(BC2RGBAUnormSrgb,BC2_UNORM_SRGB);
    F(BC3RGBAUnorm,BC3_UNORM); F(BC3RGBAUnormSrgb,BC3_UNORM_SRGB); F(BC4RUnorm,BC4_UNORM); F(BC4RSnorm,BC4_SNORM);
    F(BC5RGUnorm,BC5_UNORM); F(BC5RGSnorm,BC5_SNORM); F(BC7RGBAUnorm,BC7_UNORM); F(BC7RGBAUnormSrgb,BC7_UNORM_SRGB);
#undef F
    default: throw std::runtime_error("Aurora format has no native mapping yet: " + std::to_string(uint32_t(f)));
    }
}
uint32_t bytesPixel(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8Unorm: case TextureFormat::R8Uint: case TextureFormat::R8Snorm: case TextureFormat::R8Sint: case TextureFormat::Stencil8: return 1;
    case TextureFormat::RG8Unorm: case TextureFormat::RG8Snorm: case TextureFormat::RG8Uint: case TextureFormat::RG8Sint:
    case TextureFormat::R16Float: case TextureFormat::R16Uint: case TextureFormat::R16Sint: case TextureFormat::R16Unorm: case TextureFormat::R16Snorm: case TextureFormat::Depth16Unorm: return 2;
    case TextureFormat::RGBA16Float: case TextureFormat::RGBA16Uint: case TextureFormat::RGBA16Sint: case TextureFormat::RGBA16Unorm: case TextureFormat::RGBA16Snorm:
    case TextureFormat::RG32Float: case TextureFormat::RG32Uint: case TextureFormat::RG32Sint: case TextureFormat::Depth32FloatStencil8: return 8;
    case TextureFormat::RGBA32Float: case TextureFormat::RGBA32Uint: case TextureFormat::RGBA32Sint: return 16;
    default: return 4;
    }
}
}
struct DeviceData {
    std::shared_ptr<gfx::Contexto> contexto = std::make_shared<gfx::Contexto>();
    std::mutex envio;
    bool cerrado = false;
};
struct QueueData { std::shared_ptr<DeviceData> dispositivo; };
struct BufferData { gfx::RefBuffer nativo; BufferUsage uso{}; bool mapeado=false, mapeando=false, destruido=false; uint64_t generacionMapeo=0;
                    bool lecturaConCache=false; };  // MapRead in CPU-cached memory: invalidate before reading.
#ifndef LMNX_PC_VULKAN
extern "C" void armDCacheFlush(void* addr, size_t size);  // libnx
#endif
struct ColorBlend6;
struct TextureData { gfx::RefTextura nativo; TextureFormat formato{}; bool destruido=false;
    std::shared_ptr<ColorBlend6> blend6; };
struct TextureViewData { Texture textura; std::unique_ptr<RenderTextureView> nativa; TextureViewDescriptor descriptor;
    bool descriptorExplicito=false; std::array<TextureView,2> fisicas; };
// The logical identity owns both surfaces. Physical surfaces never point to the logical one. Recorded commands
// retain their surface without cycles or native-handle swaps.
struct ColorBlend6 { std::array<Texture,2> superficies; uint32_t actual=0; };
struct SamplerData { gfx::RefSampler nativo; };
struct BindGroupLayoutData { std::vector<BindGroupLayoutEntry> entradas; };
struct BindGroupData { BindGroupLayout layout; std::vector<BindGroupEntry> entradas; };
struct PipelineLayoutData { std::vector<BindGroupLayout> grupos; uint32_t inmediatos=0; };
struct ShaderModuleData { std::string codigo, etiqueta; };
struct EncoderData { std::shared_ptr<DeviceData> dispositivo; gfx::Paquete paquete; bool terminado=false, paso=false; };
struct CommandBufferData { std::shared_ptr<DeviceData> dispositivo; gfx::Paquete paquete; bool enviado=false; };
struct Etapa {
    std::unique_ptr<RenderShader> shader;
    swdk::TranslateResult reflejo;
    DkStage etapa{};
};
struct PipelineDatos {
    inline static std::atomic<uint32_t> vivas{0};
    PipelineDatos(){if(vivas.fetch_add(1)>=16384){vivas.fetch_sub(1);throw std::runtime_error("Limit of 16384 native pipelines reached; skipping new variant");}}
    ~PipelineDatos(){vivas.fetch_sub(1);}
    std::string identificador;
    std::shared_ptr<RenderDevice> dispositivo;
    std::vector<Etapa> etapas;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::vector<BindGroupLayout> grupos;
    std::vector<RenderInputSlot> slots;
    std::unique_ptr<plume::RenderPipeline> pipeline;
#ifdef LMNX_PC_VULKAN
    std::vector<std::vector<RenderDescriptorRange>> rangosPC;
#endif
};
struct RenderPipelineData : PipelineDatos {};
struct ComputePipelineData : PipelineDatos {};
namespace {
// [identidad-shaders inicio] Only this region contributes to cache identity (tools/shader_build_id.py).
std::mutex traduccionMutex;
std::filesystem::path cacheTraduccion;
thread_local std::vector<std::string> errores;
Etapa compilar(const std::shared_ptr<RenderDevice>& dispositivo,const ShaderModule& modulo,StringView entry,DkStage etapa) {
    exigir(bool(modulo), "Null shader module"); Etapa salida; salida.etapa=etapa;
    {
        std::lock_guard lock(traduccionMutex);
        static std::once_flag inicio;
        auto esquema=modulo.p->etiqueta.starts_with("GX Shader ")?swdk::BindingScheme::GX:swdk::BindingScheme::Sequential;
        const auto clave=claveTraduccion(modulo.p->codigo,std::string(std::string_view(entry)),uint32_t(etapa),esquema);
        if(!leerTraduccion(cacheTraduccion,clave,salida.reflejo)) {
            std::call_once(inicio,[]{swdk::translate_init();});
            salida.reflejo=swdk::wgsl_to_glsl(modulo.p->codigo,std::string(std::string_view(entry)),swdk::ShaderStage(etapa),esquema);
            if(salida.reflejo.ok)guardarTraduccion(cacheTraduccion,clave,salida.reflejo);
        }
    }
    if (!salida.reflejo.ok) throw std::runtime_error("Error Tint " + modulo.p->etiqueta + ": " + salida.reflejo.error);
    uint32_t ubo=0,ssbo=0;
    for(const auto& b:salida.reflejo.bindings) {
        if(b.kind==swdk::SlotBinding::Kind::Uniform || b.kind==swdk::SlotBinding::Kind::Immediates) ubo|=1u<<b.slot;
        if(b.kind==swdk::SlotBinding::Kind::Storage) ssbo|=1u<<b.slot;
    }
    auto glsl=prepararGLSL(salida.reflejo.glsl,ubo,ssbo);
#ifdef LMNX_PC_VULKAN
    salida.shader=lmnx::pc::compilar(*dispositivo,salida.reflejo.glsl,uint32_t(etapa));
#else
    Deko3DShaderBlob cabecera;cabecera.stage=uint32_t(etapa);cabecera.glslSize=glsl.size();
    std::vector<uint8_t> blob(sizeof cabecera+glsl.size());std::memcpy(blob.data(),&cabecera,sizeof cabecera);std::memcpy(blob.data()+sizeof cabecera,glsl.data(),glsl.size());
    salida.shader=dispositivo->createShader(blob.data(),blob.size(),"main",RenderShaderFormat::DEKO3D);
#endif
    exigir(bool(salida.shader),"Could not create the translated shader");
    return salida;
}
// [identidad-shaders fin]
void layoutNativo(PipelineDatos& salida,const PipelineLayout& layout) {
    if(layout) salida.grupos=layout.p->grupos;
    else {
        for(const auto& etapa:salida.etapas)for(const auto& b:etapa.reflejo.bindings) {
            if(b.kind==swdk::SlotBinding::Kind::Immediates)continue;
            auto agregar=[&](uint32_t grupo,uint32_t binding,bool sampler) {
                exigir(grupo<4,"Demasiados grupos de recursos");
                while(salida.grupos.size()<=grupo)salida.grupos.push_back(BindGroupLayout(std::make_shared<BindGroupLayoutData>()));
                auto& es=salida.grupos[grupo].p->entradas;
                if(std::any_of(es.begin(),es.end(),[&](auto& e){return e.binding==binding;}))return;
                BindGroupLayoutEntry e{};e.binding=binding;
                if(sampler)e.sampler.type=SamplerBindingType::Filtering;
                else if(b.kind==swdk::SlotBinding::Kind::Sampler)e.texture.sampleType=TextureSampleType::Float;
                else e.buffer.type=b.kind==swdk::SlotBinding::Kind::Uniform?BufferBindingType::Uniform:BufferBindingType::ReadOnlyStorage;
                es.push_back(e);
            };
            agregar(b.group,b.binding,false);
            if(b.kind==swdk::SlotBinding::Kind::Sampler && b.samplerGroup!=1000 && b.samplerGroup!=UINT32_MAX)agregar(b.samplerGroup,b.samplerBinding,true);
        }
    }
#ifdef LMNX_PC_VULKAN
    std::vector<RenderDescriptorSetDesc> descripciones;
    for(const auto& etapa:salida.etapas) {
        auto& rangos=salida.rangosPC.emplace_back();
        for(const auto& b:etapa.reflejo.bindings) {
            using K=swdk::SlotBinding::Kind;
            if(b.kind==K::Sampler){rangos.emplace_back(RenderDescriptorRangeType::TEXTURE,32+b.slot,1);rangos.emplace_back(RenderDescriptorRangeType::SAMPLER,64+b.slot,1);}
            else rangos.emplace_back(b.kind==K::Storage?RenderDescriptorRangeType::STRUCTURED_BUFFER:RenderDescriptorRangeType::CONSTANT_BUFFER,(b.kind==K::Storage?16:0)+b.slot,1);
        }
    }
    for(const auto& rangos:salida.rangosPC)descripciones.emplace_back(rangos.data(),rangos.size());
    salida.layout=salida.dispositivo->createPipelineLayout({nullptr,0,descripciones.data(),uint32_t(descripciones.size())});
#else
    const RenderPushConstantRange rango{swdk::kImmediatesUboSlot,0,0,64,RenderShaderStageFlag::VERTEX|RenderShaderStageFlag::PIXEL|RenderShaderStageFlag::COMPUTE};
    salida.layout=salida.dispositivo->createPipelineLayout({&rango,1,nullptr,0});
#endif
    exigir(bool(salida.layout),"Could not create the native layout");
}
RenderBlend mezcla(BlendFactor v) {
    switch(v) {
#define B(a,b) case BlendFactor::a:return RenderBlend::b
    B(Zero,ZERO);B(One,ONE);B(Src,SRC_COLOR);B(OneMinusSrc,INV_SRC_COLOR);B(SrcAlpha,SRC_ALPHA);B(OneMinusSrcAlpha,INV_SRC_ALPHA);
    B(Dst,DEST_COLOR);B(OneMinusDst,INV_DEST_COLOR);B(DstAlpha,DEST_ALPHA);B(OneMinusDstAlpha,INV_DEST_ALPHA);B(SrcAlphaSaturated,SRC_ALPHA_SAT);
    B(Constant,BLEND_FACTOR);B(OneMinusConstant,INV_BLEND_FACTOR);B(Src1,SRC1_COLOR);B(OneMinusSrc1,INV_SRC1_COLOR);B(Src1Alpha,SRC1_ALPHA);B(OneMinusSrc1Alpha,INV_SRC1_ALPHA);
#undef B
    default:throw std::runtime_error("Invalid blend factor");
    }
}
RenderStencilFaceDesc stencil(const StencilFaceState& s) {
    auto op=[](StencilOperation x) { switch(x) {
    case StencilOperation::Keep:return RenderStencilOp::KEEP;case StencilOperation::Zero:return RenderStencilOp::ZERO;case StencilOperation::Replace:return RenderStencilOp::REPLACE;
    case StencilOperation::Invert:return RenderStencilOp::INVERT;case StencilOperation::IncrementClamp:return RenderStencilOp::INCREMENT_AND_CLAMP;case StencilOperation::DecrementClamp:return RenderStencilOp::DECREMENT_AND_CLAMP;
    case StencilOperation::IncrementWrap:return RenderStencilOp::INCREMENT_AND_WRAP;case StencilOperation::DecrementWrap:return RenderStencilOp::DECREMENT_AND_WRAP;default:throw std::runtime_error("Invalid stencil operation"); } };
    return {op(s.passOp),op(s.failOp),op(s.depthFailOp),RenderComparisonFunction(uint32_t(s.compare))};
}
RenderFormat atributo(VertexFormat f) {
    switch(f) {
#define A(a,b) case VertexFormat::a:return RenderFormat::b
    A(Float32,R32_FLOAT);A(Float32x2,R32G32_FLOAT);A(Float32x3,R32G32B32_FLOAT);A(Float32x4,R32G32B32A32_FLOAT);
    A(Uint32,R32_UINT);A(Uint32x2,R32G32_UINT);A(Uint32x3,R32G32B32_UINT);A(Uint32x4,R32G32B32A32_UINT);
    A(Sint32,R32_SINT);A(Sint32x2,R32G32_SINT);A(Sint32x3,R32G32B32_SINT);A(Sint32x4,R32G32B32A32_SINT);
    A(Unorm8x4,R8G8B8A8_UNORM);A(Uint8x4,R8G8B8A8_UINT);A(Snorm8x4,R8G8B8A8_SNORM);
    A(Float16x2,R16G16_FLOAT);A(Float16x4,R16G16B16A16_FLOAT);
#undef A
    default:throw std::runtime_error("Unsupported vertex attribute");
    }
}
}
RenderPipeline Device::CreateRenderPipeline(const RenderPipelineDescriptor* d) const {
    try {
        exigir(p && d && d->vertex.module,"Invalid pipeline");auto datos=std::make_shared<RenderPipelineData>();datos->dispositivo=p->contexto->dispositivo();
        datos->etapas.push_back(compilar(datos->dispositivo,d->vertex.module,d->vertex.entryPoint,DkStage_Vertex));
        if(d->fragment)datos->etapas.push_back(compilar(datos->dispositivo,d->fragment->module,d->fragment->entryPoint,DkStage_Fragment));
        datos->identificador=std::to_string(hashFuente(d->vertex.module.p->codigo));
        layoutNativo(*datos,d->layout);
        RenderGraphicsPipelineDesc n;n.pipelineLayout=datos->layout.get();n.vertexShader=datos->etapas[0].shader.get();n.pixelShader=datos->etapas.size()>1?datos->etapas[1].shader.get():nullptr;
        n.cullMode=d->primitive.cullMode==CullMode::Back?RenderCullMode::BACK:d->primitive.cullMode==CullMode::Front?RenderCullMode::FRONT:RenderCullMode::NONE;
        n.frontFace=d->primitive.frontFace==FrontFace::CCW?RenderFrontFace::COUNTER_CLOCKWISE:RenderFrontFace::CLOCKWISE;n.depthClipEnabled=!d->primitive.unclippedDepth;
        const std::array topologias{RenderPrimitiveTopology::UNKNOWN,RenderPrimitiveTopology::POINT_LIST,RenderPrimitiveTopology::LINE_LIST,RenderPrimitiveTopology::LINE_STRIP,RenderPrimitiveTopology::TRIANGLE_LIST,RenderPrimitiveTopology::TRIANGLE_STRIP};
        exigir(uint32_t(d->primitive.topology)<topologias.size(),"Invalid topology");n.primitiveTopology=topologias[uint32_t(d->primitive.topology)];
        n.multisampling.sampleCount=d->multisample.count;n.alphaToCoverageEnabled=d->multisample.alphaToCoverageEnabled;
        if(d->depthStencil) {const auto& z=*d->depthStencil;n.depthTargetFormat=formato(z.format);n.depthEnabled=z.depthCompare!=CompareFunction::Undefined;n.depthFunction=RenderComparisonFunction(uint32_t(z.depthCompare));n.depthWriteEnabled=z.depthWriteEnabled.value==OptionalBool::True;
            n.depthBias=z.depthBias;n.depthBiasClamp=z.depthBiasClamp;n.slopeScaledDepthBias=z.depthBiasSlopeScale;n.stencilReadMask=z.stencilReadMask;n.stencilWriteMask=z.stencilWriteMask;n.stencilFrontFace=stencil(z.stencilFront);n.stencilBackFace=stencil(z.stencilBack);
            n.stencilEnabled=z.format==TextureFormat::Depth24PlusStencil8 || z.format==TextureFormat::Depth32FloatStencil8 || z.format==TextureFormat::Stencil8;
            // ALWAYS without writes means depth testing is disabled; preserve stencil state and original pass/fail
            // operations.
            if(z.depthCompare==CompareFunction::Always && !n.depthWriteEnabled)n.depthEnabled=false;
        }
        if(d->fragment) {exigir(d->fragment->targetCount<=8,"Demasiados attachments");n.renderTargetCount=d->fragment->targetCount;
            // As for pass attachments, a trailing Undefined format means an unused slot.
            while(n.renderTargetCount && d->fragment->targets[n.renderTargetCount-1].format==TextureFormat::Undefined)--n.renderTargetCount;
            for(size_t k=0;k<n.renderTargetCount;++k) {const auto& t=d->fragment->targets[k];n.renderTargetFormat[k]=formato(t.format);auto& b=n.renderTargetBlend[k];b=RenderBlendDesc::Copy();b.renderTargetWriteMask=uint8_t(t.writeMask);
                if(t.blend) {b.blendEnabled=true;b.srcBlend=mezcla(t.blend->color.srcFactor);b.dstBlend=mezcla(t.blend->color.dstFactor);b.blendOp=RenderBlendOperation(uint32_t(t.blend->color.operation));b.srcBlendAlpha=mezcla(t.blend->alpha.srcFactor);b.dstBlendAlpha=mezcla(t.blend->alpha.dstFactor);b.blendOpAlpha=RenderBlendOperation(uint32_t(t.blend->alpha.operation));}
            }
        }
        std::vector<RenderInputElement> elementos;
        for(uint32_t slot=0;slot<d->vertex.bufferCount;++slot) {const auto& b=d->vertex.buffers[slot];datos->slots.emplace_back(slot,b.arrayStride,b.stepMode==VertexStepMode::Instance?RenderInputSlotClassification::PER_INSTANCE_DATA:RenderInputSlotClassification::PER_VERTEX_DATA);
            for(size_t k=0;k<b.attributeCount;++k) {auto a=b.attributes[k];elementos.emplace_back(nullptr,0,a.shaderLocation,atributo(a.format),slot,a.offset);}
        }
        n.inputSlots=datos->slots.data();n.inputSlotsCount=datos->slots.size();n.inputElements=elementos.data();n.inputElementsCount=elementos.size();
#ifndef LMNX_PC_VULKAN
        // Tint preserves locations but emits no Renut vin= metadata. Without this table plume treats every attribute as
        // constant zero. Preserve gaps so DKSH reads the original indices without renumbering.
        auto& entradas=static_cast<Deko3DShader*>(datos->etapas[0].shader.get())->usage.vertexInputs;
        for(const auto& e:elementos) {
            exigir(e.location<16,"Vertex location exceeds uam limit");
            if(entradas.size()<=e.location)entradas.resize(e.location+1);
        }
        for(uint32_t i=0;i<entradas.size();++i)entradas[i]=i;
#endif
        datos->pipeline=datos->dispositivo->createGraphicsPipeline(n);
#ifndef LMNX_PC_VULKAN
        exigir(datos->pipeline && static_cast<Deko3DGraphicsPipeline*>(datos->pipeline.get())->valid,"Invalid deko3d pipeline");
#endif
        exigir(bool(datos->pipeline),"Invalid native pipeline");return RenderPipeline(std::move(datos));
    } catch(const std::exception& e) { if(errores.empty())throw;errores.back()=e.what();return {}; }
}
ComputePipeline Device::CreateComputePipeline(const ComputePipelineDescriptor* d) const {
    try {exigir(p && d,"Invalid compute pipeline");auto datos=std::make_shared<ComputePipelineData>();datos->dispositivo=p->contexto->dispositivo();
        datos->etapas.push_back(compilar(datos->dispositivo,d->compute.module,d->compute.entryPoint,DkStage_Compute));auto& e=datos->etapas.front();
        layoutNativo(*datos,d->layout);
        RenderComputePipelineDesc n{datos->layout.get(),e.shader.get(),e.reflejo.workgroupX,e.reflejo.workgroupY,e.reflejo.workgroupZ};
        datos->pipeline=datos->dispositivo->createComputePipeline(n);
#ifndef LMNX_PC_VULKAN
        exigir(datos->pipeline && static_cast<Deko3DComputePipeline*>(datos->pipeline.get())->valid,"Invalid deko3d compute pipeline");
#endif
        exigir(bool(datos->pipeline),"Invalid native compute pipeline");
        return ComputePipeline(std::move(datos));
    } catch(const std::exception& e) {if(errores.empty())throw;errores.back()=e.what();return {};}
}
BindGroupLayout RenderPipeline::GetBindGroupLayout(uint32_t n) const { exigir(p && n<p->grupos.size(),"Group outside layout");return p->grupos[n]; }
BindGroupLayout ComputePipeline::GetBindGroupLayout(uint32_t n) const { exigir(p && n<p->grupos.size(),"Group outside layout");return p->grupos[n]; }
void Device::PushErrorScope(ErrorFilter) const {errores.emplace_back();}
void Device::pop(std::function<void(PopErrorScopeStatus,ErrorType,StringView)> f) const {
    exigir(!errores.empty(),"Empty error stack");auto mensaje=std::move(errores.back());errores.pop_back();f(PopErrorScopeStatus::Success,mensaje.empty()?ErrorType::NoError:ErrorType::Validation,mensaje);
}

Device crearDispositivo() { return Device(std::make_shared<DeviceData>()); }
std::string nombreDispositivo(const Device& d) {exigir(bool(d.p),"Dispositivo nulo");return d.p->contexto->dispositivo()->getDescription().name;}
uint32_t muestrasSoportadas(const Device& d) {
    exigir(bool(d.p),"Dispositivo nulo");auto dispositivo=d.p->contexto->dispositivo();
    return dispositivo->getSampleCountsSupported(RenderFormat::R8G8B8A8_UNORM) &
        dispositivo->getSampleCountsSupported(RenderFormat::R16G16B16A16_FLOAT) &
        dispositivo->getSampleCountsSupported(RenderFormat::D32_FLOAT) & 15u;
}
void configurarCacheTraduccion(const std::filesystem::path& ruta) {std::lock_guard lock(traduccionMutex);cacheTraduccion=ruta;abrirPaqueteTraduccion(ruta);}
Queue Device::GetQueue() const { exigir(bool(p), "Dispositivo nulo"); return Queue(std::make_shared<QueueData>(QueueData{p})); }
Buffer Device::CreateBuffer(const BufferDescriptor* d) const {
    exigir(p && d && d->size && d->size <= UINT32_MAX, "Invalid buffer descriptor");
    auto datos=std::make_shared<BufferData>(); datos->uso=d->usage; datos->mapeado=d->mappedAtCreation;
    auto dispositivo=p->contexto->dispositivo();
#ifndef LMNX_PC_VULKAN
    // Use CPU-cached memory for GPU readbacks, such as game-read EFB copies, to avoid slow copies on the game
    // thread. plume provides this through HOST_CACHED uploads. Unmapping flushes the initial memset; GetMappedRange
    // invalidates before reading after GPU completion. The CPU does not write afterward.
    const bool lecturaConCache = uint64_t(d->usage & BufferUsage::MapRead) != 0;
    RenderBufferDesc nativo = lecturaConCache ? RenderBufferDesc::UploadBuffer(d->size, RenderBufferFlag::HOST_CACHED) : RenderBufferDesc::UploadBuffer(d->size);
    datos->lecturaConCache = lecturaConCache;
#else
    RenderBufferDesc nativo = uint64_t(d->usage & BufferUsage::MapRead) ? RenderBufferDesc::ReadbackBuffer(d->size) : RenderBufferDesc::UploadBuffer(d->size);
#endif
    nativo.flags |= RenderBufferFlag::STORAGE;
    if(uint64_t(d->usage & BufferUsage::Uniform))nativo.flags|=RenderBufferFlag::CONSTANT;
    if(uint64_t(d->usage & BufferUsage::Vertex))nativo.flags|=RenderBufferFlag::VERTEX;
    if(uint64_t(d->usage & BufferUsage::Index))nativo.flags|=RenderBufferFlag::INDEX;
    datos->nativo=std::make_shared<gfx::Buffer>(); datos->nativo->dispositivo=dispositivo;
    datos->nativo->bytes=d->size; datos->nativo->objeto=dispositivo->createBuffer(nativo);
    exigir(bool(datos->nativo->objeto), "Could not allocate the Aurora buffer");
    void* direccion=datos->nativo->objeto->map();exigir(direccion,"Could not map the Aurora buffer");
    std::memset(direccion, 0, d->size);
    if(!d->mappedAtCreation)datos->nativo->objeto->unmap();
    return Buffer(std::move(datos));
}
void* Buffer::GetMappedRange(size_t offset, size_t size) const {
    exigir(p && !p->destruido && p->mapeado, "Buffer no mapeado");
    exigir(offset <= p->nativo->bytes && (size == SIZE_MAX || size <= p->nativo->bytes-offset), "Mapping outside buffer");
    auto* direccion=static_cast<uint8_t*>(p->nativo->objeto->map());exigir(direccion,"Mapeo nativo fallido");
#ifndef LMNX_PC_VULKAN
    if (p->lecturaConCache) armDCacheFlush(direccion+offset, size == SIZE_MAX ? p->nativo->bytes-offset : size);
#endif
    return direccion+offset;
}
const void* Buffer::GetConstMappedRange(size_t o,size_t n) const { return GetMappedRange(o,n); }
void Buffer::Unmap() const {
    // Unmap cancels an in-flight read, matching WebGPU; the callback receives Aborted.
    if (p && p->mapeando) { p->mapeando=false; ++p->generacionMapeo; return; }
    exigir(p && p->mapeado, "Buffer no mapeado");
#ifndef LMNX_PC_VULKAN
    // For cached readback the CPU wrote nothing, so avoid plume flushing the entire buffer with dc cvac when no
    // range is supplied. CreateBuffer's initial mapping still flushes to publish its memset.
    if (p->lecturaConCache) { RenderRange vacio{}; p->nativo->objeto->unmap(0, &vacio); p->mapeado=false; return; }
#endif
    p->nativo->objeto->unmap(); p->mapeado=false;
}
void Buffer::Destroy() const { if (p) p->destruido=true; }
uint64_t Buffer::GetSize() const { return p ? p->nativo->bytes : 0; }
#ifndef LMNX_PC_VULKAN
// Asynchronous readbacks: record the latest submission number, whose write is already submitted. Invoke
// callbacks when its fence signals through ProcessEvents each frame, WaitAny or the next submission. Waiting for
// the whole GPU at every EFB readback would serialize CPU and GPU work.
namespace {
constexpr uint64_t kFuturoEnvio = 1ull << 63;  // Future.id for a submission wait: high bit plus submission number.
struct EsperaGpu { uint64_t envio; std::shared_ptr<BufferData> buffer; uint64_t generacion; std::function<void(MapAsyncStatus,StringView)> mapeo; std::function<void(QueueWorkDoneStatus,StringView)> terminado; };
std::mutex g_esperasMutex;
std::deque<EsperaGpu> g_esperas;
// Invoke completed-submission callbacks outside locks; hasta requests a blocking wait through that submission.
void completarEsperas(uint64_t hasta = 0) {
    if (hasta) lmnx::gfx::esperarEnvio(hasta);
    std::vector<EsperaGpu> listas;
    {
        std::lock_guard lock(g_esperasMutex);
        for (auto it = g_esperas.begin(); it != g_esperas.end();) {
            if (lmnx::gfx::envioTerminado(it->envio)) { listas.push_back(std::move(*it)); it = g_esperas.erase(it); }
            else ++it;
        }
    }
    for (auto& e : listas) {
        if (e.buffer) {
            if (e.generacion != e.buffer->generacionMapeo) { e.mapeo(MapAsyncStatus::Aborted, "Mapeo cancelado"); continue; }
            e.buffer->mapeando = false;
            if (e.buffer->destruido) { e.mapeo(MapAsyncStatus::Aborted, "Buffer destruido"); continue; }
            e.buffer->mapeado = true; e.mapeo(MapAsyncStatus::Success, {});
        } else if (e.terminado) e.terminado(QueueWorkDoneStatus::Success, {});
    }
}
Future esperarDespues(EsperaGpu e) {
    const uint64_t envio = e.envio;
    { std::lock_guard lock(g_esperasMutex); g_esperas.push_back(std::move(e)); }
    return {kFuturoEnvio | envio};
}
}  // namespace
void Instance::ProcessEvents() const { completarEsperas(); }
WaitStatus Instance::WaitAny(Future future, uint64_t timeoutNs) const {
    if (!activo || !future.id) return WaitStatus::Error;
    if (!(future.id & kFuturoEnvio)) return WaitStatus::Success;
    const uint64_t envio = future.id & ~kFuturoEnvio;
    if (timeoutNs == 0) { completarEsperas(); return lmnx::gfx::envioTerminado(envio) ? WaitStatus::Success : WaitStatus::TimedOut; }
    completarEsperas(envio); return WaitStatus::Success;
}
#else
void Instance::ProcessEvents() const {}
WaitStatus Instance::WaitAny(Future future, uint64_t) const { return activo && future.id ? WaitStatus::Success : WaitStatus::Error; }
#endif
Future Buffer::map(MapMode, size_t o,size_t n,std::function<void(MapAsyncStatus,StringView)> f) const {
    if (!p || p->destruido || p->mapeado || p->mapeando || o>GetSize() || n>GetSize()-o) {
        f(MapAsyncStatus::Error,"Invalid mapping"); return {1};
    }
#ifndef LMNX_PC_VULKAN
    // With submissions in flight, reading requires completion of GPU writes and MapWrite requires completion of GPU
    // reads. Aurora returns upload buffers to its pool on mapping completion, so this prevents reuse while the GPU
    // still copies from them.
    if (uint64_t(p->uso & (BufferUsage::MapRead | BufferUsage::MapWrite))) {
        const uint64_t envio = lmnx::gfx::envioActual();
        if (!lmnx::gfx::envioTerminado(envio)) {
            p->mapeando = true;
            return esperarDespues({envio, p, p->generacionMapeo, std::move(f), {}});
        }
    }
#endif
    p->mapeado=true; f(MapAsyncStatus::Success,{}); return {1};
}
Texture Device::CreateTexture(const TextureDescriptor* d) const {
    exigir(p && d && d->size.width && d->size.height, "Invalid texture descriptor");
    auto nativo=RenderTextureDesc::Texture2D(d->size.width,d->size.height,d->mipLevelCount,formato(d->format));
    nativo.arraySize=d->size.depthOrArrayLayers;
    if (d->dimension==TextureDimension::e3D) { nativo.dimension=RenderTextureDimension::TEXTURE_3D; nativo.depth=d->size.depthOrArrayLayers; nativo.arraySize=1; }
    nativo.multisampling.sampleCount=d->sampleCount;
    if (uint64_t(d->usage & TextureUsage::RenderAttachment)) {
        const bool profundidad=d->format==TextureFormat::Depth32Float || d->format==TextureFormat::Depth24PlusStencil8 || d->format==TextureFormat::Depth24Plus || d->format==TextureFormat::Depth16Unorm || d->format==TextureFormat::Depth32FloatStencil8;
        nativo.flags |= profundidad ? RenderTextureFlag::DEPTH_TARGET : RenderTextureFlag::RENDER_TARGET;
    }
    auto datos=std::make_shared<TextureData>(); datos->formato=d->format; datos->nativo=p->contexto->crearTextura(nativo);
    return Texture(std::move(datos));
}
TextureView Texture::CreateView(const TextureViewDescriptor* d) const {
    exigir(p && !p->destruido, "Vista de textura destruida");
    auto datos=std::make_shared<TextureViewData>(); datos->textura=*this;
    if (d) { datos->descriptor=*d; datos->descriptorExplicito=true; }
    auto nativo=RenderTextureViewDesc::Texture2D(formato(d && d->format!=TextureFormat::Undefined ? d->format : p->formato));
    if (d) { nativo.mipSlice=d->baseMipLevel; nativo.mipLevels=d->mipLevelCount; nativo.arrayIndex=d->baseArrayLayer; nativo.arraySize=d->arrayLayerCount; }
    datos->nativa=p->nativo->objeto->createTextureView(nativo); exigir(bool(datos->nativa), "Could not create the texture view");
    return TextureView(std::move(datos));
}
namespace {
TextureView vistaBlend6(const TextureView& vista,uint32_t version) {
    auto& fisica=vista.p->fisicas[version];
    if(!fisica)fisica=vista.p->textura.p->blend6->superficies[version].CreateView(
        vista.p->descriptorExplicito ? &vista.p->descriptor : nullptr);
    return fisica;
}
}
// Resolve in encoding order before creating framebuffers/descriptors or capturing commands. A returned physical
// reference represents that exact version and is never resolved again.
Texture blend6TexturaActual(const Texture& textura) {
    return textura && textura.p->blend6 ? textura.p->blend6->superficies[textura.p->blend6->actual] : textura;
}
TextureView blend6VistaActual(const TextureView& vista) {
    return vista && vista.p->textura.p->blend6 ? vistaBlend6(vista,vista.p->textura.p->blend6->actual) : vista;
}
TextureView blend6Alternativa(const Device& dispositivo,const Texture& textura,const TextureView& vista) {
    exigir(dispositivo && textura && vista && vista.p->textura.p==textura.p && textura.GetSampleCount()==1,
           "blend6 alternation: color and source must be the same single-sample EFB");
    if(!textura.p->blend6) {
        auto par=std::make_shared<ColorBlend6>();
        auto primera=std::make_shared<TextureData>();primera->nativo=textura.p->nativo;primera->formato=textura.p->formato;
        auto segunda=std::make_shared<TextureData>();segunda->formato=textura.p->formato;
        segunda->nativo=dispositivo.p->contexto->crearTextura(textura.p->nativo->descripcion);
        par->superficies={Texture(std::move(primera)),Texture(std::move(segunda))};
        textura.p->blend6=std::move(par);
    }
    return vistaBlend6(vista,1u-textura.p->blend6->actual);
}
void blend6Publicar(const Texture& textura) {
    exigir(textura && textura.p->blend6,"blend6 publication has no surfaces");
    textura.p->blend6->actual=1u-textura.p->blend6->actual;
}
uint32_t Texture::GetWidth() const { return p->nativo->descripcion.width; }
uint32_t Texture::GetHeight() const { return p->nativo->descripcion.height; }
uint32_t Texture::GetMipLevelCount() const { return p->nativo->descripcion.mipLevels; }
uint32_t Texture::GetSampleCount() const { return p->nativo->descripcion.multisampling.sampleCount; }
TextureFormat Texture::GetFormat() const { return p->formato; }
void Texture::Destroy() const { if(p) p->destruido=true; }
Sampler Device::CreateSampler(const SamplerDescriptor* d) const {
    exigir(p && d, "Descriptor de sampler nulo");
    auto direccion=[](AddressMode v) { switch(v) { case AddressMode::Repeat: return RenderTextureAddressMode::WRAP; case AddressMode::MirrorRepeat:return RenderTextureAddressMode::MIRROR; default:return RenderTextureAddressMode::CLAMP; } };
    RenderSamplerDesc n; n.addressU=direccion(d->addressModeU); n.addressV=direccion(d->addressModeV); n.addressW=direccion(d->addressModeW);
    n.minFilter=d->minFilter==FilterMode::Linear?RenderFilter::LINEAR:RenderFilter::NEAREST;
    n.magFilter=d->magFilter==FilterMode::Linear?RenderFilter::LINEAR:RenderFilter::NEAREST;
    n.mipmapMode=d->mipmapFilter==MipmapFilterMode::Linear?RenderMipmapMode::LINEAR:RenderMipmapMode::NEAREST;
    n.minLOD=d->lodMinClamp; n.maxLOD=d->lodMaxClamp; n.maxAnisotropy=d->maxAnisotropy; n.anisotropyEnabled=n.maxAnisotropy>1;
    n.comparisonEnabled=d->compare!=CompareFunction::Undefined; n.comparisonFunc=RenderComparisonFunction(uint32_t(d->compare));
    return Sampler(std::make_shared<SamplerData>(SamplerData{p->contexto->crearSampler(n)}));
}
BindGroupLayout Device::CreateBindGroupLayout(const BindGroupLayoutDescriptor* d) const {
    exigir(p && d, "Layout de grupo nulo"); auto v=std::make_shared<BindGroupLayoutData>();
    v->entradas.assign(d->entries,d->entries+d->entryCount);
    std::sort(v->entradas.begin(),v->entradas.end(),[](auto& a,auto& b){return a.binding<b.binding;});
    return BindGroupLayout(std::move(v));
}
BindGroup Device::CreateBindGroup(const BindGroupDescriptor* d) const {
    exigir(p && d && d->layout, "Group has no layout"); auto v=std::make_shared<BindGroupData>(); v->layout=d->layout;
    v->entradas.assign(d->entries,d->entries+d->entryCount); return BindGroup(std::move(v));
}
PipelineLayout Device::CreatePipelineLayout(const PipelineLayoutDescriptor* d) const {
    exigir(p && d && d->immediateSize<=64, "Invalid native layout"); auto v=std::make_shared<PipelineLayoutData>();
    v->grupos.assign(d->bindGroupLayouts,d->bindGroupLayouts+d->bindGroupLayoutCount); v->inmediatos=d->immediateSize; return PipelineLayout(std::move(v));
}
ShaderModule Device::CreateShaderModule(const ShaderModuleDescriptor* d) const {
    exigir(p && d && d->nextInChain,"El shader necesita fuente WGSL"); auto fuente=static_cast<const ShaderSourceWGSL*>(d->nextInChain);
    auto v=std::make_shared<ShaderModuleData>(); v->codigo=std::string(std::string_view(fuente->code)); v->etiqueta=std::string(std::string_view(d->label));
    return ShaderModule(std::move(v));
}
CommandEncoder Device::CreateCommandEncoder(const CommandEncoderDescriptor*) const {
    auto v=std::make_shared<EncoderData>(); v->dispositivo=p; return CommandEncoder(std::move(v));
}
CommandBuffer CommandEncoder::Finish(const CommandBufferDescriptor*) const {
    exigir(p && !p->terminado && !p->paso, "Encoder finished or pass still open");
    auto v=std::make_shared<CommandBufferData>(); v->dispositivo=p->dispositivo; v->paquete=std::move(p->paquete); p->terminado=true;
    return CommandBuffer(std::move(v));
}
namespace {
void grabar(const std::shared_ptr<EncoderData>& encoder,std::function<void(RenderCommandList&)> f) {
    exigir(encoder && !encoder->terminado,"Encoder finished"); encoder->paquete.operaciones.push_back(gfx::OperacionNativa{std::move(f)});
}
void limite(const Buffer& b,uint64_t o,uint64_t n) { exigir(b && !b.p->destruido && o<=b.GetSize() && n<=b.GetSize()-o,"Invalid buffer range"); }
RenderTextureCopyLocation ubicacion(const TexelCopyTextureInfo& d) {
    exigir(d.texture && !d.texture.p->destruido,"Invalid copy texture");
    return RenderTextureCopyLocation::Subresource(d.texture.p->nativo->objeto.get(),d.mipLevel,d.origin.z);
}
RenderTextureCopyLocation ubicacion(const TexelCopyBufferInfo& d,const Extent3D& size,TextureFormat format) {
    auto pitch=d.layout.bytesPerRow==UINT32_MAX?size.width*bytesPixel(format):d.layout.bytesPerRow;
    auto rows=d.layout.rowsPerImage==UINT32_MAX?size.height:d.layout.rowsPerImage;
    limite(d.buffer,d.layout.offset,uint64_t(pitch)*rows*size.depthOrArrayLayers);
    return RenderTextureCopyLocation::PlacedFootprint(d.buffer.p->nativo->objeto.get(),formato(format),size.width,size.height,size.depthOrArrayLayers,pitch/bytesPixel(format),d.layout.offset);
}
// Attachment transitions at pass start declare graphics-only usage. plume can then use a fragment barrier if no
// copy preceded it. COPY/RESOLVE or unknown states still require full barriers, selected by
// plume_deko3d_commands.cpp.
void barrera(RenderCommandList& c,const Texture& t,RenderTextureLayout l,RenderBarrierStages etapas=RenderBarrierStage::ALL) { c.barriers(etapas,RenderTextureBarrier(t.p->nativo->objeto.get(),l)); }
}
void CommandEncoder::CopyBufferToBuffer(const Buffer& a,uint64_t ao,const Buffer& b,uint64_t bo,uint64_t n) const {
    limite(a,ao,n); limite(b,bo,n); grabar(p,[=](auto& c){ c.copyBufferRegion(b.p->nativo->objeto->at(bo),a.p->nativo->objeto->at(ao),n); c.barriers(RenderBarrierStage::ALL,RenderBufferBarrier(b.p->nativo->objeto.get(),RenderBufferAccess::READ)); });
}
void CommandEncoder::CopyTextureToTexture(const TexelCopyTextureInfo* a,const TexelCopyTextureInfo* b,const Extent3D* n) const {
    exigir(a && b && n,"Copy has no descriptor"); auto x=*a,y=*b; auto z=*n;
    x.texture=blend6TexturaActual(x.texture);y.texture=blend6TexturaActual(y.texture);
    grabar(p,[=](auto& c){ lmnx::gfx::marcaGpu(c,"copia textura "+std::to_string(z.width)+"x"+std::to_string(z.height)); barrera(c,x.texture,RenderTextureLayout::COPY_SOURCE); barrera(c,y.texture,RenderTextureLayout::COPY_DEST);
        RenderBox region(int32_t(x.origin.x),int32_t(x.origin.y),int32_t(x.origin.x+z.width),int32_t(x.origin.y+z.height),0,int32_t(z.depthOrArrayLayers));
        c.copyTextureRegion(ubicacion(y),ubicacion(x),y.origin.x,y.origin.y,0,&region); });
#ifdef LMNX_PC_VULKAN
    lmnx::diagnostico::textura(Device(p->dispositivo),*this,y.texture,"copia-EFB-destino");
#endif
}
void CommandEncoder::CopyBufferToTexture(const TexelCopyBufferInfo* a,const TexelCopyTextureInfo* b,const Extent3D* n) const {
    exigir(a && b && n,"Copy has no descriptor"); auto x=*a;auto y=*b;auto z=*n;
    y.texture=blend6TexturaActual(y.texture);
    grabar(p,[=](auto& c){ lmnx::gfx::marcaGpu(c,"subida textura "+std::to_string(z.width)+"x"+std::to_string(z.height)); barrera(c,y.texture,RenderTextureLayout::COPY_DEST); c.copyTextureRegion(ubicacion(y),ubicacion(x,z,y.texture.GetFormat()),y.origin.x,y.origin.y,0); });
}
void CommandEncoder::CopyTextureToBuffer(const TexelCopyTextureInfo* a,const TexelCopyBufferInfo* b,const Extent3D* n) const {
    exigir(a && b && n,"Copy has no descriptor"); auto x=*a;auto y=*b;auto z=*n;
    x.texture=blend6TexturaActual(x.texture);
    grabar(p,[=](auto& c){ lmnx::gfx::marcaGpu(c,"texture readback "+std::to_string(z.width)+"x"+std::to_string(z.height)); barrera(c,x.texture,RenderTextureLayout::COPY_SOURCE); RenderBox region(int32_t(x.origin.x),int32_t(x.origin.y),int32_t(x.origin.x+z.width),int32_t(x.origin.y+z.height),0,int32_t(z.depthOrArrayLayers)); c.copyTextureRegion(ubicacion(y,z,x.texture.GetFormat()),ubicacion(x),0,0,0,&region); });
}
void CommandEncoder::ClearBuffer(const Buffer& b,uint64_t o,uint64_t n) const {
    if(n==UINT64_MAX)n=b.GetSize()-o; limite(b,o,n);
    BufferDescriptor descriptor{.usage=BufferUsage::CopySrc,.size=n,.mappedAtCreation=true};
    auto staging=Device(p->dispositivo).CreateBuffer(&descriptor);
    staging.Unmap(); CopyBufferToBuffer(staging,0,b,o,n);
}
void Queue::Submit(size_t n,const CommandBuffer* buffers) const {
    exigir(p && (!n || buffers),"Invalid submission"); std::lock_guard lock(p->dispositivo->envio);
    for(size_t k=0;k<n;++k) {
        exigir(buffers[k].p && !buffers[k].p->enviado && buffers[k].p->dispositivo==p->dispositivo,"Invalid command buffer or duplicate submission");
        p->dispositivo->contexto->ejecutar(buffers[k].p->paquete); buffers[k].p->enviado=true;
    }
#ifdef LMNX_PC_VULKAN
    lmnx::diagnostico::despuesDeSubmit();
#endif
}
Future Queue::completado(std::function<void(QueueWorkDoneStatus,StringView)> f) const {
    exigir(bool(p),"Cola nula"); {std::lock_guard lock(p->dispositivo->envio);}
#ifndef LMNX_PC_VULKAN
    // Nonblocking work-done completion for in-flight submissions, signaled by their fence.
    if (const uint64_t envio = lmnx::gfx::envioActual(); !lmnx::gfx::envioTerminado(envio)) return esperarDespues({envio, nullptr, 0, {}, std::move(f)});
#endif
    f(QueueWorkDoneStatus::Success,{}); return {1};
}
void Queue::WriteBuffer(const Buffer& b,uint64_t o,const void* datos,size_t n) const {
    limite(b,o,n); if(!n)return; std::lock_guard lock(p->dispositivo->envio);
    gfx::Paquete paquete; gfx::CargaBuffer carga{b.p->nativo,o,std::vector<uint8_t>(n)}; std::memcpy(carga.datos.data(),datos,n); paquete.operaciones.push_back(std::move(carga));
    p->dispositivo->contexto->ejecutar(paquete);
}
void Queue::WriteTexture(const TexelCopyTextureInfo* t,const void* datos,size_t n,const TexelCopyBufferLayout* layout,const Extent3D* size) const {
    exigir(t && layout && size && n,"Invalid texture upload");
    BufferDescriptor d{.usage=BufferUsage::CopySrc,.size=n,.mappedAtCreation=true}; auto b=Device(p->dispositivo).CreateBuffer(&d);
    std::memcpy(b.GetMappedRange(),datos,n);b.Unmap(); auto encoder=Device(p->dispositivo).CreateCommandEncoder();
    TexelCopyBufferInfo origen{.layout=*layout,.buffer=b};encoder.CopyBufferToTexture(&origen,t,size);auto cmd=encoder.Finish();Submit(1,&cmd);
}
namespace {
struct GrupoEnlazado { BindGroup grupo; std::vector<uint32_t> dinamicos; };
struct EstadoPaso {
    std::shared_ptr<EncoderData> encoder;
    std::array<GrupoEnlazado,4> grupos;
    std::array<uint8_t,64> inmediatos{};
    uint32_t bytesInmediatos=0;
    bool terminado=false;
};
BindGroup congelarGrupo(const BindGroup& grupo) {
    if(!grupo)return grupo;
    std::shared_ptr<BindGroupData> fisico;
    for(size_t i=0;i<grupo.p->entradas.size();++i) {
        const auto& entrada=grupo.p->entradas[i];
        if(!entrada.textureView || !entrada.textureView.p->textura.p->blend6)continue;
        if(!fisico)fisico=std::make_shared<BindGroupData>(*grupo.p);
        fisico->entradas[i].textureView=blend6VistaActual(entrada.textureView);
    }
    return fisico ? BindGroup(std::move(fisico)) : grupo;
}
void congelarGrupos(EstadoPaso& estado) {
    for(auto& grupo:estado.grupos)grupo.grupo=congelarGrupo(grupo.grupo);
}
const BindGroupEntry& buscar(const EstadoPaso& paso,uint32_t grupo,uint32_t binding) {
    exigir(grupo<paso.grupos.size() && paso.grupos[grupo].grupo,"Grupo de recursos ausente");
    for(const auto& e:paso.grupos[grupo].grupo.p->entradas)if(e.binding==binding)return e;
    throw std::runtime_error("Binding missing from group");
}
uint64_t dinamico(const EstadoPaso& paso,uint32_t grupo,uint32_t binding) {
    const auto& g=paso.grupos[grupo];size_t n=0;
    for(const auto& e:g.grupo.p->layout.p->entradas)if(e.buffer.hasDynamicOffset) {
        exigir(n<g.dinamicos.size(),"Missing dynamic offset");if(e.binding==binding)return g.dinamicos[n];++n;
    }
    return 0;
}
struct TexturasEtapa {
    std::shared_ptr<RenderDescriptorSet> conjunto;
#ifdef LMNX_PC_VULKAN
    std::shared_ptr<RenderBuffer> inmediatos;
#else
    std::vector<DkResHandle> handles;
    std::vector<std::shared_ptr<void>> ranuras;  // Global-set indices used by handles (aurora_gpu.cpp, PoolDescriptores).
#endif
    std::shared_ptr<RenderSampler> samplerCarga;
};
std::vector<TexturasEtapa> prepararTexturas(const EstadoPaso& estado,const PipelineDatos& pipeline) {
#ifdef LMNX_PC_VULKAN
    std::vector<TexturasEtapa> salida;
    for(size_t k=0;k<pipeline.etapas.size();++k) {
        const auto& rangos=pipeline.rangosPC[k];TexturasEtapa t;
        t.conjunto=pipeline.dispositivo->createDescriptorSet({rangos.data(),uint32_t(rangos.size())});
        uint32_t indice=0;
        for(const auto& b:pipeline.etapas[k].reflejo.bindings) {
            using K=swdk::SlotBinding::Kind;
            if(b.kind==K::Sampler) {
                const auto& e=buscar(estado,b.group,b.binding);exigir(bool(e.textureView),"Textura PC ausente");
                t.conjunto->setTexture(indice++,e.textureView.p->textura.p->nativo->objeto.get(),RenderTextureLayout::SHADER_READ,e.textureView.p->nativa.get());
                RenderSampler* sampler=nullptr;
                if(b.samplerGroup!=1000 && b.samplerGroup!=UINT32_MAX)sampler=buscar(estado,b.samplerGroup,b.samplerBinding).sampler.p->nativo->objeto.get();
                else {t.samplerCarga=pipeline.dispositivo->createSampler(RenderSamplerDesc{});sampler=t.samplerCarga.get();}
                t.conjunto->setSampler(indice++,sampler);
            } else if(b.kind==K::Immediates) {
                t.inmediatos=pipeline.dispositivo->createBuffer(RenderBufferDesc::UploadBuffer(64,RenderBufferFlag::CONSTANT));
                std::memcpy(t.inmediatos->map(),estado.inmediatos.data(),64);t.inmediatos->unmap();
                t.conjunto->setBuffer(indice++,t.inmediatos.get(),64);
            } else {
                const auto& e=buscar(estado,b.group,b.binding);auto offset=e.offset+dinamico(estado,b.group,b.binding);
                auto size=e.size==UINT64_MAX?e.buffer.GetSize()-offset:e.size;limite(e.buffer,offset,size);
                lmnx::pc::enlazarBuffer(*t.conjunto,indice++,*e.buffer.p->nativo->objeto,offset,size);
            }
        }
        salida.push_back(std::move(t));
    }
    return salida;
#else
    std::vector<TexturasEtapa> salida;
    uint32_t total=0;
    for(const auto& etapa:pipeline.etapas){uint32_t n=0;for(const auto& b:etapa.reflejo.bindings)if(b.kind==swdk::SlotBinding::Kind::Sampler)n=std::max(n,uint32_t(b.slot)+1);total+=n;}
    std::shared_ptr<RenderDescriptorSet> conjunto;
    std::shared_ptr<RenderSampler> samplerCarga;
    if(total){const RenderDescriptorRange rangos[]={{RenderDescriptorRangeType::TEXTURE,0,total},{RenderDescriptorRangeType::SAMPLER,0,total}};
        conjunto=pipeline.dispositivo->createDescriptorSet({rangos,2});exigir(bool(conjunto),"Error al crear descriptores");samplerCarga=pipeline.dispositivo->createSampler(RenderSamplerDesc{});}
    uint32_t base=0;
    for(const auto& etapa:pipeline.etapas) {
        uint32_t n=0;for(const auto& b:etapa.reflejo.bindings)if(b.kind==swdk::SlotBinding::Kind::Sampler)n=std::max(n,uint32_t(b.slot)+1);
        exigir(n<=32,"Demasiadas texturas nativas");TexturasEtapa t;
        if(n) {
            t.conjunto=conjunto;
            t.handles.resize(n);t.samplerCarga=samplerCarga;
            for(const auto& b:etapa.reflejo.bindings)if(b.kind==swdk::SlotBinding::Kind::Sampler) {
                const auto& e=buscar(estado,b.group,b.binding);exigir(bool(e.textureView),"Binding has no texture");
                t.conjunto->setTexture(base+b.slot,e.textureView.p->textura.p->nativo->objeto.get(),RenderTextureLayout::SHADER_READ,e.textureView.p->nativa.get());
                auto* sampler=t.samplerCarga.get();
                if(b.samplerGroup!=1000 && b.samplerGroup!=UINT32_MAX) {const auto& s=buscar(estado,b.samplerGroup,b.samplerBinding);exigir(bool(s.sampler),"Binding has no sampler");sampler=s.sampler.p->nativo->objeto.get();}
                t.conjunto->setSampler(total+base+b.slot,sampler);t.handles[b.slot]=dkMakeTextureHandle(base+b.slot,base+b.slot);
            }
        }
        salida.push_back(std::move(t));base+=n;
    }
    return salida;
#endif
}
void aplicar(RenderCommandList& comando,const PipelineDatos& pipeline,const EstadoPaso& estado,const std::vector<TexturasEtapa>& texturas) {
#ifdef LMNX_PC_VULKAN
    const bool compute=pipeline.etapas.size()==1 && pipeline.etapas[0].etapa==DkStage_Compute;
    if(compute)comando.setComputePipelineLayout(pipeline.layout.get());else comando.setGraphicsPipelineLayout(pipeline.layout.get());
    comando.setPipeline(pipeline.pipeline.get());
    for(size_t k=0;k<pipeline.etapas.size();++k) {
        for(const auto& b:pipeline.etapas[k].reflejo.bindings) {
            if(b.kind==swdk::SlotBinding::Kind::Sampler)barrera(comando,buscar(estado,b.group,b.binding).textureView.p->textura,RenderTextureLayout::SHADER_READ);
            else if(b.kind!=swdk::SlotBinding::Kind::Immediates)comando.barriers(RenderBarrierStage::ALL,RenderBufferBarrier(buscar(estado,b.group,b.binding).buffer.p->nativo->objeto.get(),RenderBufferAccess::READ));
        }
        if(compute)comando.setComputeDescriptorSet(texturas[k].conjunto.get(),k);else comando.setGraphicsDescriptorSet(texturas[k].conjunto.get(),k);
    }
#else
    auto& nativo=static_cast<Deko3DCommandList&>(comando);
    const bool compute=pipeline.etapas.size()==1 && pipeline.etapas[0].etapa==DkStage_Compute;
    if(compute)comando.setComputePipelineLayout(pipeline.layout.get());else comando.setGraphicsPipelineLayout(pipeline.layout.get());
    comando.setPipeline(pipeline.pipeline.get());
    if(estado.bytesInmediatos) {
        if(compute)comando.setComputePushConstants(0,estado.inmediatos.data(),0,64);
        else comando.setGraphicsPushConstants(0,estado.inmediatos.data(),0,64);
    }
    for(size_t k=0;k<pipeline.etapas.size();++k) {
        const auto& etapa=pipeline.etapas[k];
        for(const auto& b:etapa.reflejo.bindings) {
            using K=swdk::SlotBinding::Kind;
            if(b.kind==K::Uniform || b.kind==K::Storage) {
                const auto& e=buscar(estado,b.group,b.binding);auto offset=e.offset+dinamico(estado,b.group,b.binding);
                uint64_t size=e.size==UINT64_MAX?e.buffer.GetSize()-offset:e.size;limite(e.buffer,offset,size);exigir(size<=UINT32_MAX,"Binding demasiado grande");
                auto* buffer=static_cast<Deko3DBuffer*>(e.buffer.p->nativo->objeto.get());
                if(compute) {
                    auto& ext=b.kind==K::Uniform?nativo.computeUbo[b.slot]:nativo.computeSsbo[b.slot];ext={buffer->gpu+offset,uint32_t(size)};
                    if(b.kind==K::Uniform){nativo.computeUboValid|=1u<<b.slot;nativo.computeUboDirty=true;}else{nativo.computeSsboValid|=1u<<b.slot;nativo.computeSsboDirty=true;}
                } else {
                    // Sequential slots are stage-specific; prevent plume's shared state from overwriting them.
                    if(b.kind==K::Uniform){nativo.graphicsUboValid&=~(1u<<b.slot);dkCmdBufBindUniformBuffer(nativo.cmd,etapa.etapa,b.slot,buffer->gpu+offset,uint32_t(size));}
                    else{nativo.graphicsSsboValid&=~(1u<<b.slot);dkCmdBufBindStorageBuffer(nativo.cmd,etapa.etapa,b.slot,buffer->gpu+offset,uint32_t(size));}
                }
            } else if(b.kind==K::Sampler) {
                const auto& t=buscar(estado,b.group,b.binding).textureView.p->textura;
                barrera(comando,t,RenderTextureLayout::SHADER_READ);
            }
        }
        const auto& t=texturas[k];if(t.conjunto) {
            if(compute)comando.setComputeDescriptorSet(t.conjunto.get(),0);else comando.setGraphicsDescriptorSet(t.conjunto.get(),0);
            // Global tables with separate indices for each stage.
            dkCmdBufBindTextures(nativo.cmd,etapa.etapa,0,t.handles.data(),t.handles.size());
        }
    }
#endif
}
void actualizarGrupo(EstadoPaso& estado,uint32_t i,const BindGroup& g,size_t n,const uint32_t* offsets) {
    exigir(!estado.terminado && i<estado.grupos.size() && (!n || offsets),"Invalid dynamic group");estado.grupos[i].grupo=g;
    if(n)estado.grupos[i].dinamicos.assign(offsets,offsets+n);else estado.grupos[i].dinamicos.clear();
}
void actualizarInmediatos(EstadoPaso& estado,uint32_t o,const void* datos,size_t n) {
    exigir(!estado.terminado && o<=64 && n<=64-o && !(o%4) && !(n%4),"Immediate data out of range");
    std::memcpy(estado.inmediatos.data()+o,datos,n);estado.bytesInmediatos=std::max(estado.bytesInmediatos,uint32_t(o+n));
}
}
struct RenderPassData : EstadoPaso {
#ifndef LMNX_PC_VULKAN
    using ClaveTexturas=std::vector<uintptr_t>;
    // Shared by pass snapshots; does not reference the encoder.
    std::shared_ptr<std::map<ClaveTexturas,std::vector<TexturasEtapa>>> tablas=
        std::make_shared<std::map<ClaveTexturas,std::vector<TexturasEtapa>>>();
#endif
    std::vector<RenderPassColorAttachment> colores;
    RenderPassDepthStencilAttachment profundidad;
    std::shared_ptr<RenderFramebuffer> framebuffer;
    RenderPipeline pipeline;
    struct BufferVista {Buffer buffer;uint64_t offset=0,size=0;};
    BufferVista indices;
    IndexFormat formatoIndices=IndexFormat::Undefined;
    std::array<BufferVista,16> vertices;
    std::array<float,4> mezcla{};
};
struct ComputePassData : EstadoPaso { ComputePipeline pipeline; };
RenderPassEncoder CommandEncoder::BeginRenderPass(const RenderPassDescriptor* d) const {
    exigir(p && !p->terminado && !p->paso && d,"Invalid render pass");auto v=std::make_shared<RenderPassData>();v->encoder=p;
    v->colores.assign(d->colorAttachments,d->colorAttachments+d->colorAttachmentCount);if(d->depthStencilAttachment)v->profundidad=*d->depthStencilAttachment;
    // Fix surfaces before creating the framebuffer; startup and End retain these same views.
    for(auto& a:v->colores){a.view=blend6VistaActual(a.view);a.resolveTarget=blend6VistaActual(a.resolveTarget);}
    v->profundidad.view=blend6VistaActual(v->profundidad.view);
    // WebGPU permits null-view gaps. Remove trailing unused slots without changing other output indices. Report
    // internal gaps with their label because plume has no equivalent.
    while(!v->colores.empty() && !v->colores.back().view)v->colores.pop_back();
    for(const auto& a:v->colores)if(!a.view)throw std::runtime_error(std::string("Null attachment inside pass ")+(d->label.data?std::string(d->label.data,d->label.length==SIZE_MAX?std::strlen(d->label.data):d->label.length):"unlabeled"));
    std::vector<const RenderTexture*> colores;std::vector<const RenderTextureView*> vistas;
    for(const auto& a:v->colores){exigir(bool(a.view),"Attachment nulo");colores.push_back(a.view.p->textura.p->nativo->objeto.get());vistas.push_back(a.view.p->nativa.get());}
    RenderFramebufferDesc n;n.colorAttachments=colores.data();n.colorAttachmentViews=vistas.data();n.colorAttachmentsCount=colores.size();
    if(v->profundidad.view){n.depthAttachment=v->profundidad.view.p->textura.p->nativo->objeto.get();n.depthAttachmentView=v->profundidad.view.p->nativa.get();n.depthAttachmentReadOnly=v->profundidad.depthReadOnly;}
    v->framebuffer=p->dispositivo->contexto->dispositivo()->createFramebuffer(n);exigir(bool(v->framebuffer),"Error al crear framebuffer");p->paso=true;
    auto inicio=std::make_shared<RenderPassData>(*v);inicio->encoder.reset();
    std::string marca=d->label.data?std::string(d->label.data,d->label.length==SIZE_MAX?std::strlen(d->label.data):d->label.length):"pase";
    marca+=" "+std::to_string(v->framebuffer->getWidth())+"x"+std::to_string(v->framebuffer->getHeight());
    grabar(p,[v=inicio,marca](auto& c){
        lmnx::gfx::marcaGpu(c,marca);
        for(const auto& a:v->colores)barrera(c,a.view.p->textura,RenderTextureLayout::COLOR_WRITE,RenderBarrierStage::GRAPHICS);
        if(v->profundidad.view)barrera(c,v->profundidad.view.p->textura,v->profundidad.depthReadOnly?RenderTextureLayout::DEPTH_READ:RenderTextureLayout::DEPTH_WRITE,RenderBarrierStage::GRAPHICS);
        c.setFramebuffer(v->framebuffer.get());auto w=v->framebuffer->getWidth(),h=v->framebuffer->getHeight();c.setViewports(viewportNativo(0,0,w,h));c.setScissors(RenderRect(0,0,w,h));
        for(uint32_t k=0;k<v->colores.size();++k)if(v->colores[k].loadOp==LoadOp::Clear){auto a=v->colores[k].clearValue;c.clearColor(k,{float(a.r),float(a.g),float(a.b),float(a.a)});}
        if(v->profundidad.view && (v->profundidad.depthLoadOp==LoadOp::Clear || v->profundidad.stencilLoadOp==LoadOp::Clear))
            c.clearDepthStencil(v->profundidad.depthLoadOp==LoadOp::Clear,v->profundidad.stencilLoadOp==LoadOp::Clear,v->profundidad.depthClearValue,v->profundidad.stencilClearValue);
    });return RenderPassEncoder(std::move(v));
}
void RenderPassEncoder::SetPipeline(const RenderPipeline& pipeline) const {exigir(p && !p->terminado && pipeline,"Pipeline nula");p->pipeline=pipeline;}
void RenderPassEncoder::SetBindGroup(uint32_t i,const BindGroup& g,size_t n,const uint32_t* o) const {actualizarGrupo(*p,i,g,n,o);}
void RenderPassEncoder::SetImmediates(uint32_t o,const void* d,size_t n) const {actualizarInmediatos(*p,o,d,n);}
void RenderPassEncoder::SetViewport(float x,float y,float w,float h,float zn,float zf) const {exigir(p && !p->terminado,"Pass finished");grabar(p->encoder,[=](auto& c){c.setViewports(viewportNativo(x,y,w,h,zn,zf));});}
void RenderPassEncoder::SetScissorRect(uint32_t x,uint32_t y,uint32_t w,uint32_t h) const {exigir(p && !p->terminado,"Pass finished");grabar(p->encoder,[=](auto& c){c.setScissors(RenderRect(x,y,x+w,y+h));});}
void RenderPassEncoder::SetIndexBuffer(const Buffer& b,IndexFormat f,uint64_t o,uint64_t n) const {if(n==UINT64_MAX)n=b.GetSize()-o;limite(b,o,n);p->indices={b,o,n};p->formatoIndices=f;}
void RenderPassEncoder::SetVertexBuffer(uint32_t s,const Buffer& b,uint64_t o,uint64_t n) const {exigir(s<16,"Invalid vertex slot");if(n==UINT64_MAX)n=b.GetSize()-o;limite(b,o,n);p->vertices[s]={b,o,n};}
void RenderPassEncoder::SetBlendConstant(const Color* c) const {exigir(c,"Color nulo");p->mezcla={float(c->r),float(c->g),float(c->b),float(c->a)};}
namespace {
void dibujo(const std::shared_ptr<RenderPassData>& p,bool indexado,uint32_t n,uint32_t inst,uint32_t inicio,int32_t base,uint32_t primera) {
    struct Cronometro { std::chrono::steady_clock::time_point t0=std::chrono::steady_clock::now();
        ~Cronometro(){lmnx::medidas::grabarNs.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count()),std::memory_order_relaxed);
            lmnx::medidas::dibujosGrabados.fetch_add(1,std::memory_order_relaxed);} } cronometro;
    exigir(p && !p->terminado && p->pipeline,"Draw has no pipeline");
#ifdef LMNX_PC_VULKAN
    lmnx::diagnostico::registro("draw pipeline="+p->pipeline.p->identificador+" elementos="+std::to_string(n)+" instancias="+std::to_string(inst));
#endif
    auto estado=std::make_shared<RenderPassData>(*p);estado->encoder.reset();std::vector<TexturasEtapa> texturas;
    congelarGrupos(*estado); // Table keys, descriptors and barriers use the same physical version.
#ifdef LMNX_PC_VULKAN
    texturas=prepararTexturas(*estado,*estado->pipeline.p);
#else
    RenderPassData::ClaveTexturas clave{reinterpret_cast<uintptr_t>(estado->pipeline.p.get())};
    for(const auto& etapa:estado->pipeline.p->etapas)for(const auto& b:etapa.reflejo.bindings)
        if(b.kind==swdk::SlotBinding::Kind::Sampler) {
            clave.push_back(reinterpret_cast<uintptr_t>(buscar(*estado,b.group,b.binding).textureView.p.get()));
            clave.push_back(b.samplerGroup==1000 || b.samplerGroup==UINT32_MAX?0:
                reinterpret_cast<uintptr_t>(buscar(*estado,b.samplerGroup,b.samplerBinding).sampler.p.get()));
        }
    const auto encontrado=p->tablas->find(clave);
    if(encontrado!=p->tablas->end()) {
        texturas=encontrado->second;GetDeko3DStats().tablasReusadas.fetch_add(1,std::memory_order_relaxed);
    } else {
        texturas=prepararTexturas(*estado,*estado->pipeline.p);
        GetDeko3DStats().tablasNuevas.fetch_add(1,std::memory_order_relaxed);
        if(p->tablas->size()<1024)p->tablas->emplace(std::move(clave),texturas);
    }
#endif
    grabar(p->encoder,[=](auto& c){aplicar(c,*estado->pipeline.p,*estado,texturas);
#ifdef LMNX_PC_VULKAN
        vkCmdSetBlendConstants(static_cast<VulkanCommandList&>(c).vk,estado->mezcla.data());lmnx::pc::contarDibujo();
#else
        auto& native=static_cast<Deko3DCommandList&>(c);native.setBlendConstant(estado->mezcla.data());
#endif
        for(uint32_t s=0;s<estado->vertices.size();++s)if(estado->vertices[s].buffer){const auto& b=estado->vertices[s];RenderVertexBufferView v{b.buffer.p->nativo->objeto->at(b.offset),uint32_t(b.size)};c.setVertexBuffers(s,&v,1,estado->pipeline.p->slots.data());}
        if(indexado){const auto& b=estado->indices;exigir(bool(b.buffer),"Indexed draw has no indices");RenderIndexBufferView v{b.buffer.p->nativo->objeto->at(b.offset),uint32_t(b.size),estado->formatoIndices==IndexFormat::Uint32?RenderFormat::R32_UINT:RenderFormat::R16_UINT};c.setIndexBuffer(&v);c.drawIndexedInstanced(n,inst,inicio,base,primera);}
        else c.drawInstanced(n,inst,inicio,primera);
    });
}
}
void RenderPassEncoder::Draw(uint32_t n,uint32_t i,uint32_t inicio,uint32_t primera) const {dibujo(p,false,n,i,inicio,0,primera);}
void RenderPassEncoder::DrawIndexed(uint32_t n,uint32_t i,uint32_t inicio,int32_t base,uint32_t primera) const {dibujo(p,true,n,i,inicio,base,primera);}
void RenderPassEncoder::End() const {
    exigir(p && !p->terminado,"Pass finished");auto colores=p->colores;grabar(p->encoder,[colores](auto& c){for(const auto& a:colores)if(a.resolveTarget){auto src=a.view.p->textura,dst=a.resolveTarget.p->textura;barrera(c,src,RenderTextureLayout::RESOLVE_SOURCE);barrera(c,dst,RenderTextureLayout::RESOLVE_DEST);c.resolveTexture(dst.p->nativo->objeto.get(),src.p->nativo->objeto.get());}});
#ifdef LMNX_PC_VULKAN
    for(const auto& a:p->colores){
        lmnx::diagnostico::registro("attachment clear="+std::to_string(uint32_t(a.loadOp))+" rgba="+std::to_string(a.clearValue.r)+","+std::to_string(a.clearValue.g)+","+std::to_string(a.clearValue.b)+","+std::to_string(a.clearValue.a));
        lmnx::diagnostico::textura(Device(p->encoder->dispositivo),CommandEncoder(p->encoder),a.view.p->textura,"fin-pase-color");
    }
    if(p->profundidad.view)lmnx::diagnostico::textura(Device(p->encoder->dispositivo),CommandEncoder(p->encoder),p->profundidad.view.p->textura,"fin-pase-profundidad");
#endif
    p->terminado=true;p->encoder->paso=false;
}
ComputePassEncoder CommandEncoder::BeginComputePass(const ComputePassDescriptor*) const {
    exigir(p && !p->terminado && !p->paso,"Codificador ocupado");auto v=std::make_shared<ComputePassData>();v->encoder=p;p->paso=true;return ComputePassEncoder(std::move(v));
}
void ComputePassEncoder::SetPipeline(const ComputePipeline& pipeline) const {exigir(p && !p->terminado && pipeline,"Pipeline compute nula");p->pipeline=pipeline;}
void ComputePassEncoder::SetBindGroup(uint32_t i,const BindGroup& g,size_t n,const uint32_t* o) const {actualizarGrupo(*p,i,g,n,o);}
void ComputePassEncoder::SetImmediates(uint32_t o,const void* d,size_t n) const {actualizarInmediatos(*p,o,d,n);}
void ComputePassEncoder::DispatchWorkgroups(uint32_t x,uint32_t y,uint32_t z) const {
    exigir(p && !p->terminado && p->pipeline,"Dispatch has no pipeline");auto estado=std::make_shared<ComputePassData>(*p);estado->encoder.reset();congelarGrupos(*estado);auto texturas=prepararTexturas(*estado,*estado->pipeline.p);
    grabar(p->encoder,[=](auto& c){aplicar(c,*estado->pipeline.p,*estado,texturas);c.dispatch(x,y,z);
#ifdef LMNX_PC_VULKAN
        // plume/Vulkan dereferences the barrier buffer; send a global bufferless barrier directly to Vulkan.
        VkMemoryBarrier global{VK_STRUCTURE_TYPE_MEMORY_BARRIER,nullptr,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_SHADER_READ_BIT};
        vkCmdPipelineBarrier(static_cast<VulkanCommandList&>(c).vk,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&global,0,nullptr,0,nullptr);
#else
        c.barriers(RenderBarrierStage::ALL,RenderBufferBarrier(nullptr,RenderBufferAccess::READ));
#endif
    });
}
void ComputePassEncoder::End() const {exigir(p && !p->terminado,"Compute pass finished");p->terminado=true;p->encoder->paso=false;}
bool Device::HasFeature(FeatureName f) const {
    return p && (f==FeatureName::DepthClipControl || f==FeatureName::Float32Filterable || f==FeatureName::TextureCompressionBC || f==FeatureName::DualSourceBlending);
}
bool Device::GetLimits(Limits* l) const {
    if(!p || !l)return false;
    *l={};l->maxTextureDimension1D=8192;l->maxTextureDimension2D=8192;l->maxTextureDimension3D=2048;l->maxTextureArrayLayers=2048;
    l->maxBindGroups=4;l->maxBindingsPerBindGroup=32;l->maxSampledTexturesPerShaderStage=32;l->maxSamplersPerShaderStage=32;
    l->maxStorageBuffersPerShaderStage=14;l->maxUniformBuffersPerShaderStage=15;l->maxUniformBufferBindingSize=65536;l->maxStorageBufferBindingSize=128*1024*1024;
    l->minUniformBufferOffsetAlignment=256;l->minStorageBufferOffsetAlignment=16;l->maxVertexBuffers=16;l->maxVertexAttributes=32;l->maxBufferSize=UINT32_MAX;
    l->maxColorAttachments=8;l->maxComputeWorkgroupSizeX=1024;l->maxComputeWorkgroupSizeY=1024;l->maxComputeWorkgroupSizeZ=64;l->maxComputeInvocationsPerWorkgroup=1024;l->maxComputeWorkgroupsPerDimension=65535;
    return true;
}
// Optional instrumentation: Aurora checks HasFeature first. Do not advertise Switch GPU timestamps or fabricate
// query results.
struct QuerySetData {};
QuerySet Device::CreateQuerySet(const QuerySetDescriptor*) const {throw std::runtime_error("GPU profiling disabled in LM-NX");}
void QuerySet::Destroy() const {}
void CommandEncoder::ResolveQuerySet(const QuerySet&,uint32_t,uint32_t,const Buffer&,uint64_t) const {throw std::runtime_error("GPU profiling disabled in LM-NX");}
struct SurfaceData { Device dispositivo; Texture destino; };
Surface crearSuperficie(const Device& device) {exigir(bool(device),"Surface has no device");auto s=std::make_shared<SurfaceData>();s->dispositivo=device;return Surface(std::move(s));}
void Surface::Configure(const SurfaceConfiguration* config) const {
    exigir(p && config && config->format==TextureFormat::RGBA8Unorm,"Superficie nativa requiere RGBA8");
    TextureDescriptor d{.usage=TextureUsage::RenderAttachment|TextureUsage::CopySrc,.size={config->width,config->height,1},.format=config->format};
    p->destino=p->dispositivo.CreateTexture(&d);
    p->dispositivo.p->contexto->configurarVsync(config->presentMode!=PresentMode::Immediate);
}
void Surface::GetCurrentTexture(SurfaceTexture* texture) const {
    exigir(p && texture,"Invalid surface");texture->texture=p->destino;texture->status=p->destino?SurfaceGetCurrentTextureStatus::SuccessOptimal:SurfaceGetCurrentTextureStatus::Outdated;
}
// Cross-frame texture-table cache for direct batches (grabarLote), used only by the submission-recording thread
// in lmnx::gfx::Contexto::ejecutar under its lock.
struct TablaGlobal {
    std::vector<TexturasEtapa> texturas;
    std::vector<std::shared_ptr<TextureViewData>> vistas;  // Retain the key: view and sampler addresses.
    std::vector<std::shared_ptr<SamplerData>> samplers;
    uint64_t usado=0;
};
std::map<std::vector<uintptr_t>,TablaGlobal> g_tablasGlobales;
std::atomic<uint64_t> g_cuadroTablas{0};
uint64_t g_limpiezaTablas=0;
constexpr uint64_t kVidaTablas=120;  // Unused frames before releasing a table.
#ifndef LMNX_PC_VULKAN
void purgarPoolDescriptores();
#endif
void limpiarTablasGlobales() {
    const uint64_t ahora=g_cuadroTablas.load(std::memory_order_relaxed);
    if(ahora-g_limpiezaTablas<32)return;
    g_limpiezaTablas=ahora;
    for(auto it=g_tablasGlobales.begin();it!=g_tablasGlobales.end();)
        if(ahora-it->second.usado>kVidaTablas)it=g_tablasGlobales.erase(it);else ++it;
#ifndef LMNX_PC_VULKAN
    purgarPoolDescriptores();
#endif
}
#ifndef LMNX_PC_VULKAN
// Global direct-batch descriptor set, like SwitchWaker switch/deko/descriptors.cpp. Give each texture view and
// sampler one fixed index written once; tables store only handles. This avoids rebinding image/sampler sets on
// texture changes, which discards Maxwell's texture-header cache. Reuse an index only after all tables release
// it and kEsperaIndice frames pass so GPU reads have ended.
struct RanuraDescriptor {
    uint32_t indice=0;
    bool imagen=true;
    std::shared_ptr<void> vivo;  // Retain the view or sampler so its address key cannot be reused by another object.
    ~RanuraDescriptor();
};
struct PoolDescriptores {
    // A deko3d handle holds 20 image bits and 12 sampler bits.
    static constexpr uint32_t kCapacidad=16384, kSamplers=4096, kEsperaIndice=8;
    std::shared_ptr<RenderDescriptorSet> conjunto;
    std::shared_ptr<RenderSampler> samplerPorDefecto;   // Sampler index zero for slots without their own sampler.
    std::vector<uint32_t> libresImagen, libresSampler;
    std::deque<std::pair<uint64_t,uint32_t>> retiradasImagen, retiradasSampler;  // (frame, index).
    std::unordered_map<const void*,std::weak_ptr<RanuraDescriptor>> imagenes, samplers;
    uint32_t tomar(bool imagen) {
        auto& retiradas=imagen?retiradasImagen:retiradasSampler; auto& libres=imagen?libresImagen:libresSampler;
        const uint64_t ahora=g_cuadroTablas.load(std::memory_order_relaxed);
        while(!retiradas.empty() && retiradas.front().first+kEsperaIndice<=ahora){libres.push_back(retiradas.front().second);retiradas.pop_front();}
        if(libres.empty())return UINT32_MAX;
        const uint32_t i=libres.back();libres.pop_back();return i;
    }
};
PoolDescriptores* g_poolDescriptores=nullptr;  // Do not destroy on exit; slots use it in their destructors.
RanuraDescriptor::~RanuraDescriptor() {
    if(!g_poolDescriptores)return;
    auto& r=imagen?g_poolDescriptores->retiradasImagen:g_poolDescriptores->retiradasSampler;
    r.emplace_back(g_cuadroTablas.load(std::memory_order_relaxed),indice);
}
void purgarPoolDescriptores() {
    if(!g_poolDescriptores)return;
    for(auto* m:{&g_poolDescriptores->imagenes,&g_poolDescriptores->samplers})
        for(auto it=m->begin();it!=m->end();)if(it->second.expired())it=m->erase(it);else ++it;
}
// Batch tables hold handles into the global set; empty if indices run out, falling back to individual sets.
std::vector<TexturasEtapa> prepararTexturasGlobales(const EstadoPaso& estado,const PipelineDatos& pipeline) {
    using K=swdk::SlotBinding::Kind;
    if(!g_poolDescriptores) {
        auto* p=new PoolDescriptores;
        const RenderDescriptorRange rangos[]={{RenderDescriptorRangeType::TEXTURE,0,PoolDescriptores::kCapacidad},{RenderDescriptorRangeType::SAMPLER,0,PoolDescriptores::kSamplers}};
        p->conjunto=pipeline.dispositivo->createDescriptorSet({rangos,2});exigir(bool(p->conjunto),"Could not create the global descriptor set");
        p->samplerPorDefecto=pipeline.dispositivo->createSampler(RenderSamplerDesc{});
        p->conjunto->setSampler(PoolDescriptores::kCapacidad+0,p->samplerPorDefecto.get());
        for(uint32_t i=PoolDescriptores::kCapacidad;i-->0;)p->libresImagen.push_back(i);
        for(uint32_t i=PoolDescriptores::kSamplers;i-->1;)p->libresSampler.push_back(i);
        g_poolDescriptores=p;
    }
    auto& pool=*g_poolDescriptores;
    const auto ranura=[&](const std::shared_ptr<void>& objeto,bool imagen,const auto& escribir)->std::shared_ptr<RanuraDescriptor> {
        auto& mapa=imagen?pool.imagenes:pool.samplers;
        if(const auto it=mapa.find(objeto.get());it!=mapa.end())if(auto r=it->second.lock())return r;
        const uint32_t i=pool.tomar(imagen);
        if(i==UINT32_MAX)return nullptr;
        escribir(i);
        auto r=std::make_shared<RanuraDescriptor>();r->indice=i;r->imagen=imagen;r->vivo=objeto;
        mapa[objeto.get()]=r;return r;
    };
    std::vector<TexturasEtapa> salida;
    for(const auto& etapa:pipeline.etapas) {
        uint32_t n=0;for(const auto& b:etapa.reflejo.bindings)if(b.kind==K::Sampler)n=std::max(n,uint32_t(b.slot)+1);
        exigir(n<=32,"Demasiadas texturas nativas");TexturasEtapa t;
        if(n) {
            t.conjunto=pool.conjunto;t.handles.assign(n,dkMakeTextureHandle(0,0));
            for(const auto& b:etapa.reflejo.bindings)if(b.kind==K::Sampler) {
                const auto& e=buscar(estado,b.group,b.binding);exigir(bool(e.textureView),"Binding has no texture");
                const auto vista=e.textureView.p;
                auto img=ranura(vista,true,[&](uint32_t i){pool.conjunto->setTexture(i,vista->textura.p->nativo->objeto.get(),RenderTextureLayout::SHADER_READ,vista->nativa.get());});
                if(!img)return {};
                uint32_t smp=0;
                if(b.samplerGroup!=1000 && b.samplerGroup!=UINT32_MAX) {
                    const auto& s=buscar(estado,b.samplerGroup,b.samplerBinding);exigir(bool(s.sampler),"Binding has no sampler");
                    const auto sampler=s.sampler.p;
                    auto r=ranura(sampler,false,[&](uint32_t i){pool.conjunto->setSampler(PoolDescriptores::kCapacidad+i,sampler->nativo->objeto.get());});
                    if(!r)return {};
                    smp=r->indice;t.ranuras.push_back(std::move(r));
                }
                t.handles[b.slot]=dkMakeTextureHandle(img->indice,smp);t.ranuras.push_back(std::move(img));
            }
        }
        salida.push_back(std::move(t));
    }
    return salida;
}
#endif
bool Surface::Present() const {
    g_cuadroTablas.fetch_add(1,std::memory_order_relaxed);
    exigir(p && p->destino,"Superficie no configurada");std::lock_guard lock(p->dispositivo.p->envio);
    gfx::Paquete paquete;paquete.presentar=blend6TexturaActual(p->destino).p->nativo;p->dispositivo.p->contexto->ejecutar(paquete);return true;
}
}

// Direct batch (include/lmnx/directo.hpp): encode a whole GX pass to deko3d with state caching, like SwitchWaker
// dk_encode. Outside shader identity.
#include <lmnx/directo.hpp>
namespace lmnx::gpu::directo {
void grabarLote(const RenderPassEncoder& pase,const BindGroup& estatico,const BindGroup& uniformes,const Buffer& indices,std::vector<Orden>&& lote) {
    exigir(pase.p && !pase.p->terminado,"Batch in a finished pass");
    if(lote.empty())return;
#ifdef LMNX_PC_VULKAN
    // PC sends the same commands individually to support image comparison.
    pase.SetBindGroup(0,estatico);
    for(const auto& o:lote) {
        if(o.tipo==Orden::Tipo::Viewport){pase.SetViewport(o.viewport[0],o.viewport[1],o.viewport[2],o.viewport[3],o.viewport[4],o.viewport[5]);continue;}
        if(o.tipo==Orden::Tipo::Scissor){pase.SetScissorRect(o.scissor[0],o.scissor[1],o.scissor[2],o.scissor[3]);continue;}
        pase.SetImmediates(0,o.inmediatos.data(),o.inmediatos.size());
        pase.SetBindGroup(1,uniformes,1,&o.offsetUniformes);
        if(o.texturas)pase.SetBindGroup(2,o.texturas);
        if(o.indices)pase.SetIndexBuffer(indices,IndexFormat::Uint16,0,UINT64_MAX);
        if(o.dstAlpha!=UINT32_MAX){const Color m{0.,0.,0.,o.dstAlpha/255.};pase.SetBlendConstant(&m);}
        const auto dibujar=[&](const RenderPipeline& r){pase.SetPipeline(r);if(o.indices)pase.DrawIndexed(o.indices,o.instancias,o.primerIndice);else pase.Draw(o.vertices,o.instancias);};
        if(o.previo)dibujar(o.previo);
        dibujar(o.principal);
    }
#else
    uint64_t dibujos=0;for(const auto& o:lote)if(o.tipo==Orden::Tipo::Dibujo)dibujos+=o.previo?2:1;
    lmnx::medidas::dibujosGrabados.fetch_add(dibujos,std::memory_order_relaxed);
    // The batch executes later. Resolve now in Aurora order, before other colors are published. Global tables retain
    // physical views and their identities.
    std::unordered_map<const BindGroupData*,BindGroup> gruposFisicos;
    const auto fijar=[&](const BindGroup& grupo) {
        if(!grupo)return grupo;
        auto [it,nuevo]=gruposFisicos.try_emplace(grupo.p.get());
        if(nuevo)it->second=congelarGrupo(grupo);
        return it->second;
    };
    const auto estaticoFisico=fijar(estatico),uniformesFisicos=fijar(uniformes);
    for(auto& o:lote)o.texturas=fijar(o.texturas);
    grabar(pase.p->encoder,[estatico=estaticoFisico,uniformes=uniformesFisicos,indices,lote=std::move(lote),tablas=pase.p->tablas](RenderCommandList& c) {
        using K=swdk::SlotBinding::Kind;
        auto& nativo=static_cast<Deko3DCommandList&>(c);lmnx::medidas::lotes.fetch_add(1,std::memory_order_relaxed);
        EstadoPaso e;e.grupos[0].grupo=estatico;e.grupos[1].grupo=uniformes;e.grupos[1].dinamicos.assign(1,0u);e.bytesInmediatos=64;
        // Prepare textures read by the batch once here, rather than per draw; plume skips already ready textures.
        {std::vector<const TextureData*> hechas;
         for(const auto& o:lote)if(o.tipo==Orden::Tipo::Dibujo && o.texturas)for(const auto& en:o.texturas.p->entradas)
             if(en.textureView && std::find(hechas.begin(),hechas.end(),en.textureView.p->textura.p.get())==hechas.end()) {
                 hechas.push_back(en.textureView.p->textura.p.get());barrera(c,en.textureView.p->textura,RenderTextureLayout::SHADER_READ);
             }}
        const PipelineDatos* actual=nullptr;uint32_t uboHecho=UINT32_MAX;const BindGroupData* texHecho=nullptr;
        std::array<uint8_t,64> inmHecho{};bool inmValido=false,idxPuesto=false;uint32_t alfa=UINT32_MAX;
        const auto enlazarBuffers=[&](const PipelineDatos& P,bool soloUniformes) {
            for(const auto& etapa:P.etapas)for(const auto& b:etapa.reflejo.bindings) {
                if(b.kind!=K::Uniform && b.kind!=K::Storage)continue;
                if(soloUniformes && b.group!=1)continue;
                const auto& en=buscar(e,b.group,b.binding);const uint64_t offset=en.offset+dinamico(e,b.group,b.binding);
                uint64_t size=en.size==UINT64_MAX?en.buffer.GetSize()-offset:en.size;limite(en.buffer,offset,size);
                auto* buffer=static_cast<Deko3DBuffer*>(en.buffer.p->nativo->objeto.get());
                if(b.kind==K::Uniform){nativo.graphicsUboValid&=~(1u<<b.slot);dkCmdBufBindUniformBuffer(nativo.cmd,etapa.etapa,b.slot,buffer->gpu+offset,uint32_t(size));}
                else{nativo.graphicsSsboValid&=~(1u<<b.slot);dkCmdBufBindStorageBuffer(nativo.cmd,etapa.etapa,b.slot,buffer->gpu+offset,uint32_t(size));}
            }
        };
        const auto claveDe=[&](const PipelineDatos& P) {
            RenderPassData::ClaveTexturas clave{reinterpret_cast<uintptr_t>(&P)};
            for(const auto& etapa:P.etapas)for(const auto& b:etapa.reflejo.bindings)if(b.kind==K::Sampler) {
                clave.push_back(reinterpret_cast<uintptr_t>(buscar(e,b.group,b.binding).textureView.p.get()));
                clave.push_back(b.samplerGroup==1000 || b.samplerGroup==UINT32_MAX?0:reinterpret_cast<uintptr_t>(buscar(e,b.samplerGroup,b.samplerBinding).sampler.p.get()));
            }
            return clave;
        };
        // Cache texture tables across frames in tablasGlobales to avoid rebuilding hundreds per frame. Entries retain
        // views and samplers so their address keys cannot be reused. Release after kVidaTablas unused frames, longer
        // than the in-flight submission lifetime.
        limpiarTablasGlobales();
        const auto tablaDe=[&](const PipelineDatos& P) -> const std::vector<TexturasEtapa>& {
            auto clave=claveDe(P);
            if(const auto it=g_tablasGlobales.find(clave);it!=g_tablasGlobales.end()) {
                it->second.usado=g_cuadroTablas;GetDeko3DStats().tablasReusadas.fetch_add(1,std::memory_order_relaxed);return it->second.texturas;
            }
            TablaGlobal t;t.usado=g_cuadroTablas;t.texturas=prepararTexturasGlobales(e,P);if(t.texturas.empty())t.texturas=prepararTexturas(e,P);
            for(const auto& etapa:P.etapas)for(const auto& b:etapa.reflejo.bindings)if(b.kind==K::Sampler) {
                t.vistas.push_back(buscar(e,b.group,b.binding).textureView.p);
                if(b.samplerGroup!=1000 && b.samplerGroup!=UINT32_MAX)t.samplers.push_back(buscar(e,b.samplerGroup,b.samplerBinding).sampler.p);
            }
            GetDeko3DStats().tablasNuevas.fetch_add(1,std::memory_order_relaxed);
            return g_tablasGlobales.emplace(std::move(clave),std::move(t)).first->second.texturas;
        };
        // Write new batch tables before the first draw. Writing between draws forces repeated GPU descriptor-cache
        // invalidation; this limits invalidation to once per batch.
        {
            for(const auto& o:lote) {
                if(o.tipo!=Orden::Tipo::Dibujo)continue;
                if(o.texturas)e.grupos[2].grupo=o.texturas;
                for(const RenderPipeline* r:{&o.previo,&o.principal})if(*r)tablaDe(*r->p);
            }
            e.grupos[2].grupo={};
        }
        const auto enlazarTexturas=[&](const PipelineDatos& P) {
            const auto& texturas=tablaDe(P);
            for(size_t k=0;k<P.etapas.size();++k)if(const auto& t=texturas[k];t.conjunto) {
                c.setGraphicsDescriptorSet(t.conjunto.get(),0);
                dkCmdBufBindTextures(nativo.cmd,P.etapas[k].etapa,0,t.handles.data(),t.handles.size());
            }
        };
        for(const auto& o:lote) {
            if(o.tipo==Orden::Tipo::Viewport){c.setViewports(viewportNativo(o.viewport[0],o.viewport[1],o.viewport[2],o.viewport[3],o.viewport[4],o.viewport[5]));continue;}
            if(o.tipo==Orden::Tipo::Scissor){c.setScissors(RenderRect(int32_t(o.scissor[0]),int32_t(o.scissor[1]),int32_t(o.scissor[0]+o.scissor[2]),int32_t(o.scissor[1]+o.scissor[3])));continue;}
            if(o.texturas)e.grupos[2].grupo=o.texturas;
            e.grupos[1].dinamicos[0]=o.offsetUniformes;
            std::memcpy(e.inmediatos.data(),o.inmediatos.data(),64);
            if(o.indices && !idxPuesto){RenderIndexBufferView v{indices.p->nativo->objeto->at(0),uint32_t(indices.GetSize()),RenderFormat::R16_UINT};c.setIndexBuffer(&v);idxPuesto=true;}
            if(o.dstAlpha!=UINT32_MAX && o.dstAlpha!=alfa){const float m[4]={0.f,0.f,0.f,float(o.dstAlpha)/255.f};nativo.setBlendConstant(m);alfa=o.dstAlpha;}
            const auto dibujar=[&](const PipelineDatos& P) {
                const bool nuevo=&P!=actual;
                if(nuevo){c.setGraphicsPipelineLayout(P.layout.get());c.setPipeline(P.pipeline.get());actual=&P;lmnx::medidas::cambiosPipeline.fetch_add(1,std::memory_order_relaxed);}
                if(nuevo || !inmValido || std::memcmp(inmHecho.data(),e.inmediatos.data(),64)!=0) {
                    c.setGraphicsPushConstants(0,e.inmediatos.data(),0,64);std::memcpy(inmHecho.data(),e.inmediatos.data(),64);inmValido=true;
                }
                if(nuevo)enlazarBuffers(P,false);else if(o.offsetUniformes!=uboHecho){enlazarBuffers(P,true);lmnx::medidas::enlacesUbo.fetch_add(1,std::memory_order_relaxed);}
                uboHecho=o.offsetUniformes;
                if(nuevo || e.grupos[2].grupo.p.get()!=texHecho){enlazarTexturas(P);texHecho=e.grupos[2].grupo.p.get();lmnx::medidas::enlacesTexturas.fetch_add(1,std::memory_order_relaxed);}
                if(o.indices)c.drawIndexedInstanced(o.indices,o.instancias,o.primerIndice,0,0);else c.drawInstanced(o.vertices,o.instancias,0,0);
            };
            if(o.previo)dibujar(*o.previo.p);
            dibujar(*o.principal.p);
        }
    });
#endif
}
}
