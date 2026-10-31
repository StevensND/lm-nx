#pragma once
// Native Aurora resource boundary. Each resource owns LMNX-Deko3D memory/pipelines; no Dawn/WebGPU device,
// validator or library.
#include "aurora_gpu_enums.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <limits>
#include <filesystem>

namespace lmnx::gpu {
struct StringView {
    const char* data = nullptr;
    size_t length = SIZE_MAX;
    constexpr StringView() = default;
    constexpr StringView(const char* p) : data(p) {}
    constexpr StringView(const char* p, size_t n) : data(p), length(n) {}
    StringView(const std::string& s) : data(s.data()), length(s.size()) {}
    constexpr StringView(std::string_view s) : data(s.data()), length(s.size()) {}
    operator std::string_view() const { return data ? std::string_view(data, length == SIZE_MAX ? std::char_traits<char>::length(data) : length) : std::string_view{}; }
};
inline std::string_view format_as(StringView s) { return s; }
struct OptionalBool {
    enum Value : uint32_t { False=0, True=1, Undefined=2 };
    Value value = Undefined;
    constexpr OptionalBool() = default;
    constexpr OptionalBool(bool b) : value(b ? True : False) {}
    constexpr OptionalBool(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
};
struct ChainedStruct { const ChainedStruct* nextInChain = nullptr; uint32_t sType = 0; };
struct Future { uint64_t id = 0; };
inline constexpr uint32_t kQuerySetIndexUndefined = UINT32_MAX;
inline constexpr uint64_t kWholeSize = UINT64_MAX;

#define LMNX_RECURSO(nombre) struct nombre##Data; \
    template<> struct Datos<nombre##Data>;
template<class T> struct Datos;
template<class T> class Ref {
public:
    std::shared_ptr<T> p;
    Ref() = default;
    Ref(std::nullptr_t) {}
    explicit Ref(std::shared_ptr<T> v) : p(std::move(v)) {}
    T* Get() const { return p.get(); }
    explicit operator bool() const { return bool(p); }
    bool operator==(const Ref&) const = default;
    bool operator==(std::nullptr_t) const { return !p; }
    Ref& operator=(std::nullptr_t) { p.reset(); return *this; }
};
struct TextureDescriptor; struct TextureViewDescriptor; struct SamplerDescriptor;
struct BufferDescriptor; struct BindGroupDescriptor; struct BindGroupLayoutDescriptor;
struct PipelineLayoutDescriptor; struct ShaderModuleDescriptor; struct RenderPipelineDescriptor;
struct ComputePipelineDescriptor; struct CommandEncoderDescriptor; struct CommandBufferDescriptor;
struct RenderPassDescriptor; struct ComputePassDescriptor; struct TexelCopyTextureInfo;
struct TexelCopyBufferInfo; struct TexelCopyBufferLayout; struct Extent3D; struct Color;
struct Limits; struct QuerySetDescriptor;
struct SurfaceConfiguration; struct SurfaceTexture;
struct BufferData; struct TextureData; struct TextureViewData; struct SamplerData;
struct BindGroupData; struct BindGroupLayoutData; struct PipelineLayoutData; struct ShaderModuleData;
struct RenderPipelineData; struct ComputePipelineData; struct EncoderData; struct RenderPassData;
struct ComputePassData; struct CommandBufferData; struct DeviceData; struct QueueData;
struct SurfaceData; struct QuerySetData;

class TextureView : public Ref<TextureViewData> { public: using Ref::Ref; using Ref::operator=; };
class Sampler : public Ref<SamplerData> { public: using Ref::Ref; using Ref::operator=; };
class BindGroupLayout : public Ref<BindGroupLayoutData> { public: using Ref::Ref; using Ref::operator=; };
class PipelineLayout : public Ref<PipelineLayoutData> { public: using Ref::Ref; using Ref::operator=; };
class ShaderModule : public Ref<ShaderModuleData> { public: using Ref::Ref; using Ref::operator=; };
class BindGroup : public Ref<BindGroupData> { public: using Ref::Ref; using Ref::operator=; };
class QuerySet : public Ref<QuerySetData> { public: using Ref::Ref; using Ref::operator=; void Destroy() const; };
class Surface : public Ref<SurfaceData> { public: using Ref::Ref; using Ref::operator=;
    void Configure(const SurfaceConfiguration*) const;
    void GetCurrentTexture(SurfaceTexture*) const;
    bool Present() const;
};
class Buffer : public Ref<BufferData> {
public:
    using Ref::Ref; using Ref::operator=;
    void* GetMappedRange(size_t offset=0, size_t size=SIZE_MAX) const;
    const void* GetConstMappedRange(size_t offset=0, size_t size=SIZE_MAX) const;
    void Unmap() const;
    void Destroy() const;
    uint64_t GetSize() const;
    Future map(MapMode, size_t, size_t, std::function<void(MapAsyncStatus,StringView)>) const;
    template<class F> Future MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode, F&& f) const {
        return map(mode,offset,size,std::forward<F>(f));
    }
};
class Texture : public Ref<TextureData> {
public:
    using Ref::Ref; using Ref::operator=;
    TextureView CreateView(const TextureViewDescriptor* desc=nullptr) const;
    uint32_t GetWidth() const; uint32_t GetHeight() const; uint32_t GetMipLevelCount() const;
    uint32_t GetSampleCount() const; TextureFormat GetFormat() const;
    void Destroy() const;
};
class RenderPipeline : public Ref<RenderPipelineData> {
public: using Ref::Ref; using Ref::operator=;
    BindGroupLayout GetBindGroupLayout(uint32_t) const;
};
class ComputePipeline : public Ref<ComputePipelineData> {
public: using Ref::Ref; using Ref::operator=;
    BindGroupLayout GetBindGroupLayout(uint32_t) const;
};
class CommandBuffer : public Ref<CommandBufferData> { public: using Ref::Ref; using Ref::operator=; };
class RenderPassEncoder : public Ref<RenderPassData> {
public:
    using Ref::Ref; using Ref::operator=;
    void SetPipeline(const RenderPipeline&) const;
    void SetBindGroup(uint32_t,const BindGroup&,size_t=0,const uint32_t* =nullptr) const;
    void SetViewport(float,float,float,float,float,float) const;
    void SetScissorRect(uint32_t,uint32_t,uint32_t,uint32_t) const;
    void SetIndexBuffer(const Buffer&,IndexFormat,uint64_t=0,uint64_t=UINT64_MAX) const;
    void SetVertexBuffer(uint32_t,const Buffer&,uint64_t=0,uint64_t=UINT64_MAX) const;
    void SetImmediates(uint32_t,const void*,size_t) const;
    void SetBlendConstant(const Color*) const;
    void Draw(uint32_t,uint32_t=1,uint32_t=0,uint32_t=0) const;
    void DrawIndexed(uint32_t,uint32_t=1,uint32_t=0,int32_t=0,uint32_t=0) const;
    void End() const;
    void PushDebugGroup(StringView) const {} // Diagnostic labels only.
    void PopDebugGroup() const {}
    void InsertDebugMarker(StringView) const {}
};
class ComputePassEncoder : public Ref<ComputePassData> {
public:
    using Ref::Ref; using Ref::operator=;
    void SetPipeline(const ComputePipeline&) const;
    void SetBindGroup(uint32_t,const BindGroup&,size_t=0,const uint32_t* =nullptr) const;
    void DispatchWorkgroups(uint32_t,uint32_t=1,uint32_t=1) const;
    void SetImmediates(uint32_t,const void*,size_t) const;
    void End() const;
};
class CommandEncoder : public Ref<EncoderData> {
public:
    using Ref::Ref; using Ref::operator=;
    RenderPassEncoder BeginRenderPass(const RenderPassDescriptor*) const;
    ComputePassEncoder BeginComputePass(const ComputePassDescriptor* =nullptr) const;
    void CopyBufferToBuffer(const Buffer&,uint64_t,const Buffer&,uint64_t,uint64_t) const;
    void CopyBufferToTexture(const TexelCopyBufferInfo*,const TexelCopyTextureInfo*,const Extent3D*) const;
    void CopyTextureToBuffer(const TexelCopyTextureInfo*,const TexelCopyBufferInfo*,const Extent3D*) const;
    void CopyTextureToTexture(const TexelCopyTextureInfo*,const TexelCopyTextureInfo*,const Extent3D*) const;
    void ClearBuffer(const Buffer&,uint64_t=0,uint64_t=UINT64_MAX) const;
    CommandBuffer Finish(const CommandBufferDescriptor* =nullptr) const;
    void ResolveQuerySet(const QuerySet&,uint32_t,uint32_t,const Buffer&,uint64_t) const;
    void PushDebugGroup(StringView) const {}
    void PopDebugGroup() const {}
    void InsertDebugMarker(StringView) const {}
};
class Queue : public Ref<QueueData> {
public:
    using Ref::Ref; using Ref::operator=;
    void Submit(size_t,const CommandBuffer*) const;
    void WriteBuffer(const Buffer&,uint64_t,const void*,size_t) const;
    void WriteTexture(const TexelCopyTextureInfo*,const void*,size_t,const TexelCopyBufferLayout*,const Extent3D*) const;
    Future completado(std::function<void(QueueWorkDoneStatus,StringView)>) const;
    template<class F> Future OnSubmittedWorkDone(CallbackMode,F&& f) const { return completado(std::forward<F>(f)); }
};
class Instance {
public:
    bool activo = false;
    explicit operator bool() const { return activo; }
    // Readbacks and work-done requests complete when their submission fence signals (aurora_gpu.cpp).
    void ProcessEvents() const;
    WaitStatus WaitAny(Future future, uint64_t timeoutNs) const;
};
class Device : public Ref<DeviceData> {
public:
    using Ref::Ref; using Ref::operator=;
    Buffer CreateBuffer(const BufferDescriptor*) const;
    Texture CreateTexture(const TextureDescriptor*) const;
    Sampler CreateSampler(const SamplerDescriptor*) const;
    BindGroupLayout CreateBindGroupLayout(const BindGroupLayoutDescriptor*) const;
    BindGroup CreateBindGroup(const BindGroupDescriptor*) const;
    PipelineLayout CreatePipelineLayout(const PipelineLayoutDescriptor*) const;
    ShaderModule CreateShaderModule(const ShaderModuleDescriptor*) const;
    RenderPipeline CreateRenderPipeline(const RenderPipelineDescriptor*) const;
    ComputePipeline CreateComputePipeline(const ComputePipelineDescriptor*) const;
    CommandEncoder CreateCommandEncoder(const CommandEncoderDescriptor* =nullptr) const;
    QuerySet CreateQuerySet(const QuerySetDescriptor*) const;
    bool HasFeature(FeatureName) const;
    bool GetLimits(Limits*) const;
    Queue GetQueue() const;
    void PushErrorScope(ErrorFilter) const;
    void pop(std::function<void(PopErrorScopeStatus,ErrorType,StringView)>) const;
    template<class F> Future PopErrorScope(CallbackMode,F&& f) const { pop(std::forward<F>(f)); return {}; }
};
Device crearDispositivo();
std::string nombreDispositivo(const Device&);
uint32_t muestrasSoportadas(const Device&);
void configurarCacheTraduccion(const std::filesystem::path&);
Surface crearSuperficie(const Device&);
void presentar(const Texture&);
void cerrar();
}

#include "aurora_gpu_descriptors.hpp"
namespace lmnx::gpu {
enum class SurfaceGetCurrentTextureStatus { SuccessOptimal, SuccessSuboptimal, Timeout, Outdated, Lost, Error };
struct SurfaceTexture { Texture texture; SurfaceGetCurrentTextureStatus status=SurfaceGetCurrentTextureStatus::Error; };
inline bool operator==(const Extent3D& a, const Extent3D& b) {
    return a.width == b.width && a.height == b.height && a.depthOrArrayLayers == b.depthOrArrayLayers;
}
}
namespace wgpu = lmnx::gpu; // Legacy name aliases without WebGPU objects or ABI.
using WGPURenderPipeline = lmnx::gpu::RenderPipelineData*;
using WGPUTextureView = lmnx::gpu::TextureViewData*;
using WGPUBindGroupDescriptor = lmnx::gpu::BindGroupDescriptor;
using WGPUBindGroupEntry = lmnx::gpu::BindGroupEntry;
#define WGPU_STRLEN SIZE_MAX
#define WGPU_MIP_LEVEL_COUNT_UNDEFINED UINT32_MAX
#define WGPU_ARRAY_LAYER_COUNT_UNDEFINED UINT32_MAX
