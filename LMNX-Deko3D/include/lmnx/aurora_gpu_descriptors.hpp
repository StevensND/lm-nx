// Data descriptors with LMNX-owned resources; no Dawn ABI or objects.
#pragma once
namespace lmnx::gpu {
struct AdapterInfo;
struct BindGroupDescriptor;
struct BindGroupEntry;
struct BindGroupLayoutDescriptor;
struct BindGroupLayoutEntry;
struct BlendComponent;
struct BlendState;
struct BufferBindingLayout;
struct BufferDescriptor;
struct Color;
struct ColorTargetState;
struct CommandBufferDescriptor;
struct CommandEncoderDescriptor;
struct ComputePassDescriptor;
struct ComputePipelineDescriptor;
struct ComputeState;
struct ConstantEntry;
struct DepthStencilState;
struct Extent3D;
struct FragmentState;
struct Limits;
struct MultisampleState;
struct Origin3D;
struct PassTimestampWrites;
struct PipelineLayoutDescriptor;
struct PrimitiveState;
struct QuerySetDescriptor;
struct RenderPassColorAttachment;
struct RenderPassDepthStencilAttachment;
struct RenderPassDescriptor;
struct RenderPipelineDescriptor;
struct SamplerBindingLayout;
struct SamplerDescriptor;
struct ShaderModuleDescriptor;
struct ShaderSourceWGSL;
struct StencilFaceState;
struct StorageTextureBindingLayout;
struct SurfaceCapabilities;
struct SurfaceConfiguration;
struct TexelCopyBufferInfo;
struct TexelCopyBufferLayout;
struct TexelCopyTextureInfo;
struct TextureBindingLayout;
struct TextureComponentSwizzle;
struct TextureComponentSwizzleDescriptor;
struct TextureDescriptor;
struct TextureViewDescriptor;
struct VertexAttribute;
struct VertexBufferLayout;
struct VertexState;
struct AdapterInfo {
    const ChainedStruct* nextInChain = nullptr;
    StringView vendor = {};
    StringView architecture = {};
    StringView device = {};
    StringView description = {};
    BackendType backendType = {};
    AdapterType adapterType = {};
    uint32_t vendorID = {};
    uint32_t deviceID = {};
    uint32_t subgroupMinSize = {};
    uint32_t subgroupMaxSize = {};
};
struct BindGroupDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    BindGroupLayout layout = {};
    size_t entryCount = 0;
    const BindGroupEntry* entries = nullptr;
};
struct BindGroupEntry {
    const ChainedStruct* nextInChain = nullptr;
    uint32_t binding = {};
    Buffer buffer = {};
    uint64_t offset = 0;
    uint64_t size = UINT64_MAX;
    Sampler sampler = {};
    TextureView textureView = {};
};
struct BindGroupLayoutDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    size_t entryCount = 0;
    const BindGroupLayoutEntry* entries = nullptr;
};
struct BlendComponent {
    BlendOperation operation = BlendOperation::Add;
    BlendFactor srcFactor = BlendFactor::One;
    BlendFactor dstFactor = BlendFactor::Zero;
};
struct BufferBindingLayout {
    const ChainedStruct* nextInChain = nullptr;
    BufferBindingType type = BufferBindingType::Uniform;
    bool hasDynamicOffset = false;
    uint64_t minBindingSize = 0;
};
struct BufferDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    BufferUsage usage = {};
    uint64_t size = {};
    bool mappedAtCreation = false;
};
struct Color {
    double r = {};
    double g = {};
    double b = {};
    double a = {};
};
struct ColorTargetState {
    const ChainedStruct* nextInChain = nullptr;
    TextureFormat format = {};
    const BlendState* blend = nullptr;
    ColorWriteMask writeMask = ColorWriteMask::All;
};
struct CommandBufferDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
};
struct CommandEncoderDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
};
struct ComputePassDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    const PassTimestampWrites* timestampWrites = nullptr;
};
struct ComputeState {
    const ChainedStruct* nextInChain = nullptr;
    ShaderModule module = {};
    StringView entryPoint = {};
    size_t constantCount = 0;
    const ConstantEntry* constants = nullptr;
};
struct ConstantEntry {
    const ChainedStruct* nextInChain = nullptr;
    StringView key = {};
    double value = {};
};
struct Extent3D {
    uint32_t width = {};
    uint32_t height = 1;
    uint32_t depthOrArrayLayers = 1;
};
struct FragmentState {
    const ChainedStruct* nextInChain = nullptr;
    ShaderModule module = {};
    StringView entryPoint = {};
    size_t constantCount = 0;
    const ConstantEntry* constants = nullptr;
    size_t targetCount = {};
    const ColorTargetState* targets = nullptr;
};
struct Limits {
    const ChainedStruct* nextInChain = nullptr;
    uint32_t maxTextureDimension1D = UINT32_MAX;
    uint32_t maxTextureDimension2D = UINT32_MAX;
    uint32_t maxTextureDimension3D = UINT32_MAX;
    uint32_t maxTextureArrayLayers = UINT32_MAX;
    uint32_t maxBindGroups = UINT32_MAX;
    uint32_t maxBindGroupsPlusVertexBuffers = UINT32_MAX;
    uint32_t maxBindingsPerBindGroup = UINT32_MAX;
    uint32_t maxDynamicUniformBuffersPerPipelineLayout = UINT32_MAX;
    uint32_t maxDynamicStorageBuffersPerPipelineLayout = UINT32_MAX;
    uint32_t maxSampledTexturesPerShaderStage = UINT32_MAX;
    uint32_t maxSamplersPerShaderStage = UINT32_MAX;
    uint32_t maxStorageBuffersPerShaderStage = UINT32_MAX;
    uint32_t maxStorageTexturesPerShaderStage = UINT32_MAX;
    uint32_t maxUniformBuffersPerShaderStage = UINT32_MAX;
    uint64_t maxUniformBufferBindingSize = UINT64_MAX;
    uint64_t maxStorageBufferBindingSize = UINT64_MAX;
    uint32_t minUniformBufferOffsetAlignment = UINT32_MAX;
    uint32_t minStorageBufferOffsetAlignment = UINT32_MAX;
    uint32_t maxVertexBuffers = UINT32_MAX;
    uint64_t maxBufferSize = UINT64_MAX;
    uint32_t maxVertexAttributes = UINT32_MAX;
    uint32_t maxVertexBufferArrayStride = UINT32_MAX;
    uint32_t maxInterStageShaderVariables = UINT32_MAX;
    uint32_t maxColorAttachments = UINT32_MAX;
    uint32_t maxColorAttachmentBytesPerSample = UINT32_MAX;
    uint32_t maxComputeWorkgroupStorageSize = UINT32_MAX;
    uint32_t maxComputeInvocationsPerWorkgroup = UINT32_MAX;
    uint32_t maxComputeWorkgroupSizeX = UINT32_MAX;
    uint32_t maxComputeWorkgroupSizeY = UINT32_MAX;
    uint32_t maxComputeWorkgroupSizeZ = UINT32_MAX;
    uint32_t maxComputeWorkgroupsPerDimension = UINT32_MAX;
    uint32_t maxImmediateSize = UINT32_MAX;
};
struct MultisampleState {
    const ChainedStruct* nextInChain = nullptr;
    uint32_t count = 1;
    uint32_t mask = 0xFFFFFFFF;
    bool alphaToCoverageEnabled = false;
};
struct Origin3D {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
};
struct PassTimestampWrites {
    const ChainedStruct* nextInChain = nullptr;
    QuerySet querySet = {};
    uint32_t beginningOfPassWriteIndex = UINT32_MAX;
    uint32_t endOfPassWriteIndex = UINT32_MAX;
};
struct PipelineLayoutDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    size_t bindGroupLayoutCount = {};
    const BindGroupLayout* bindGroupLayouts = nullptr;
    uint32_t immediateSize = 0;
};
struct PrimitiveState {
    const ChainedStruct* nextInChain = nullptr;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    IndexFormat stripIndexFormat = {};
    FrontFace frontFace = FrontFace::CCW;
    CullMode cullMode = CullMode::None;
    bool unclippedDepth = false;
};
struct QuerySetDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    QueryType type = {};
    uint32_t count = {};
};
struct RenderPassDepthStencilAttachment {
    const ChainedStruct* nextInChain = nullptr;
    TextureView view = {};
    LoadOp depthLoadOp = {};
    StoreOp depthStoreOp = {};
    float depthClearValue = {};
    bool depthReadOnly = false;
    LoadOp stencilLoadOp = {};
    StoreOp stencilStoreOp = {};
    uint32_t stencilClearValue = 0;
    bool stencilReadOnly = false;
};
struct RenderPassDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    size_t colorAttachmentCount = {};
    const RenderPassColorAttachment* colorAttachments = nullptr;
    const RenderPassDepthStencilAttachment* depthStencilAttachment = nullptr;
    QuerySet occlusionQuerySet = {};
    const PassTimestampWrites* timestampWrites = nullptr;
};
struct SamplerBindingLayout {
    const ChainedStruct* nextInChain = nullptr;
    SamplerBindingType type = SamplerBindingType::Filtering;
};
struct SamplerDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    AddressMode addressModeU = AddressMode::ClampToEdge;
    AddressMode addressModeV = AddressMode::ClampToEdge;
    AddressMode addressModeW = AddressMode::ClampToEdge;
    FilterMode magFilter = FilterMode::Nearest;
    FilterMode minFilter = FilterMode::Nearest;
    MipmapFilterMode mipmapFilter = MipmapFilterMode::Nearest;
    float lodMinClamp = 0.f;
    float lodMaxClamp = 32.f;
    CompareFunction compare = {};
    uint16_t maxAnisotropy = 1;
};
struct ShaderModuleDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
};
struct ShaderSourceWGSL : ChainedStruct {
    struct Init { StringView code; };
    ShaderSourceWGSL() = default;
    ShaderSourceWGSL(Init init) : code(init.code) {}
    StringView code = {};
};
struct StencilFaceState {
    CompareFunction compare = CompareFunction::Always;
    StencilOperation failOp = StencilOperation::Keep;
    StencilOperation depthFailOp = StencilOperation::Keep;
    StencilOperation passOp = StencilOperation::Keep;
};
struct StorageTextureBindingLayout {
    const ChainedStruct* nextInChain = nullptr;
    StorageTextureAccess access = StorageTextureAccess::WriteOnly;
    TextureFormat format = {};
    TextureViewDimension viewDimension = TextureViewDimension::e2D;
};
struct SurfaceCapabilities {
    const ChainedStruct* nextInChain = nullptr;
    TextureUsage usages = {};
    size_t formatCount = {};
    const TextureFormat* formats = nullptr;
    size_t presentModeCount = {};
    const PresentMode* presentModes = nullptr;
    size_t alphaModeCount = {};
    const CompositeAlphaMode* alphaModes = nullptr;
};
struct SurfaceConfiguration {
    const ChainedStruct* nextInChain = nullptr;
    Device device = {};
    TextureFormat format = {};
    TextureUsage usage = TextureUsage::RenderAttachment;
    uint32_t width = {};
    uint32_t height = {};
    size_t viewFormatCount = 0;
    const TextureFormat* viewFormats = nullptr;
    CompositeAlphaMode alphaMode = CompositeAlphaMode::Auto;
    PresentMode presentMode = PresentMode::Fifo;
};
struct TexelCopyBufferLayout {
    uint64_t offset = 0;
    uint32_t bytesPerRow = UINT32_MAX;
    uint32_t rowsPerImage = UINT32_MAX;
};
struct TextureBindingLayout {
    const ChainedStruct* nextInChain = nullptr;
    TextureSampleType sampleType = TextureSampleType::Float;
    TextureViewDimension viewDimension = TextureViewDimension::e2D;
    bool multisampled = false;
};
struct TextureComponentSwizzle {
    ComponentSwizzle r = ComponentSwizzle::R;
    ComponentSwizzle g = ComponentSwizzle::G;
    ComponentSwizzle b = ComponentSwizzle::B;
    ComponentSwizzle a = ComponentSwizzle::A;
};
struct TextureViewDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    TextureFormat format = {};
    TextureViewDimension dimension = {};
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = UINT32_MAX;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = UINT32_MAX;
    TextureAspect aspect = TextureAspect::All;
    TextureUsage usage = TextureUsage::None;
};
struct VertexAttribute {
    const ChainedStruct* nextInChain = nullptr;
    VertexFormat format = {};
    uint64_t offset = {};
    uint32_t shaderLocation = {};
};
struct VertexBufferLayout {
    const ChainedStruct* nextInChain = nullptr;
    VertexStepMode stepMode = {};
    uint64_t arrayStride = {};
    size_t attributeCount = {};
    const VertexAttribute* attributes = nullptr;
};
struct VertexState {
    const ChainedStruct* nextInChain = nullptr;
    ShaderModule module = {};
    StringView entryPoint = {};
    size_t constantCount = 0;
    const ConstantEntry* constants = nullptr;
    size_t bufferCount = 0;
    const VertexBufferLayout* buffers = nullptr;
};
struct BindGroupLayoutEntry {
    const ChainedStruct* nextInChain = nullptr;
    uint32_t binding = {};
    ShaderStage visibility = {};
    uint32_t bindingArraySize = 0;
    BufferBindingLayout buffer = {};
    SamplerBindingLayout sampler = {};
    TextureBindingLayout texture = {};
    StorageTextureBindingLayout storageTexture = {};
};
struct BlendState {
    BlendComponent color = {};
    BlendComponent alpha = {};
};
struct ComputePipelineDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    PipelineLayout layout = {};
    ComputeState compute = {};
};
struct DepthStencilState {
    const ChainedStruct* nextInChain = nullptr;
    TextureFormat format = {};
    OptionalBool depthWriteEnabled = {};
    CompareFunction depthCompare = {};
    StencilFaceState stencilFront = {};
    StencilFaceState stencilBack = {};
    uint32_t stencilReadMask = 0xFFFFFFFF;
    uint32_t stencilWriteMask = 0xFFFFFFFF;
    int32_t depthBias = 0;
    float depthBiasSlopeScale = 0.f;
    float depthBiasClamp = 0.f;
};
struct RenderPassColorAttachment {
    const ChainedStruct* nextInChain = nullptr;
    TextureView view = {};
    uint32_t depthSlice = UINT32_MAX;
    TextureView resolveTarget = {};
    LoadOp loadOp = {};
    StoreOp storeOp = {};
    Color clearValue = {};
};
struct RenderPipelineDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    PipelineLayout layout = {};
    VertexState vertex = {};
    PrimitiveState primitive = {};
    const DepthStencilState* depthStencil = nullptr;
    MultisampleState multisample = {};
    const FragmentState* fragment = nullptr;
};
struct TexelCopyBufferInfo {
    TexelCopyBufferLayout layout = {};
    Buffer buffer = {};
};
struct TexelCopyTextureInfo {
    Texture texture = {};
    uint32_t mipLevel = 0;
    Origin3D origin = {};
    TextureAspect aspect = TextureAspect::All;
};
struct TextureComponentSwizzleDescriptor : ChainedStruct {
    TextureComponentSwizzle swizzle = {};
};
struct TextureDescriptor {
    const ChainedStruct* nextInChain = nullptr;
    StringView label = {};
    TextureUsage usage = {};
    TextureDimension dimension = TextureDimension::e2D;
    Extent3D size = {};
    TextureFormat format = {};
    uint32_t mipLevelCount = 1;
    uint32_t sampleCount = 1;
    size_t viewFormatCount = 0;
    const TextureFormat* viewFormats = nullptr;
};
}
