// Serialized Aurora enums from Dawn's BSD-3-Clause schema; data only.
#pragma once
#include <cstdint>
namespace lmnx::gpu {
enum class AdapterType : uint32_t {
    DiscreteGPU = 1,
    IntegratedGPU = 2,
    CPU = 3,
    Unknown = 4,
};
enum class AddressMode : uint32_t {
    Undefined = 0,
    ClampToEdge = 1,
    Repeat = 2,
    MirrorRepeat = 3,
};
enum class BackendType : uint32_t {
    Undefined = 0,
    Null = 1,
    WebGPU = 2,
    D3D11 = 3,
    D3D12 = 4,
    Metal = 5,
    Vulkan = 6,
    OpenGL = 7,
    OpenGLES = 8,
};
enum class BlendFactor : uint32_t {
    Undefined = 0,
    Zero = 1,
    One = 2,
    Src = 3,
    OneMinusSrc = 4,
    SrcAlpha = 5,
    OneMinusSrcAlpha = 6,
    Dst = 7,
    OneMinusDst = 8,
    DstAlpha = 9,
    OneMinusDstAlpha = 10,
    SrcAlphaSaturated = 11,
    Constant = 12,
    OneMinusConstant = 13,
    Src1 = 14,
    OneMinusSrc1 = 15,
    Src1Alpha = 16,
    OneMinusSrc1Alpha = 17,
};
enum class BlendOperation : uint32_t {
    Undefined = 0,
    Add = 1,
    Subtract = 2,
    ReverseSubtract = 3,
    Min = 4,
    Max = 5,
};
enum class BufferBindingType : uint32_t {
    BindingNotUsed = 0,
    Undefined = 1,
    Uniform = 2,
    Storage = 3,
    ReadOnlyStorage = 4,
};
enum class BufferUsage : uint64_t {
    None = 0,
    MapRead = 1,
    MapWrite = 2,
    CopySrc = 4,
    CopyDst = 8,
    Index = 16,
    Vertex = 32,
    Uniform = 64,
    Storage = 128,
    Indirect = 256,
    QueryResolve = 512,
    TexelBuffer = 328704,
};
constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return BufferUsage(uint64_t(a) | uint64_t(b)); }
constexpr BufferUsage& operator|=(BufferUsage& a, BufferUsage b) { return a = a | b; }
constexpr BufferUsage operator&(BufferUsage a, BufferUsage b) { return BufferUsage(uint64_t(a) & uint64_t(b)); }
constexpr BufferUsage& operator&=(BufferUsage& a, BufferUsage b) { return a = a & b; }
constexpr BufferUsage operator^(BufferUsage a, BufferUsage b) { return BufferUsage(uint64_t(a) ^ uint64_t(b)); }
constexpr BufferUsage& operator^=(BufferUsage& a, BufferUsage b) { return a = a ^ b; }
constexpr bool operator!(BufferUsage a) { return uint64_t(a)==0; }
enum class CallbackMode : uint32_t {
    WaitAnyOnly = 1,
    AllowProcessEvents = 2,
    AllowSpontaneous = 3,
};
enum class ColorWriteMask : uint64_t {
    None = 0,
    Red = 1,
    Green = 2,
    Blue = 4,
    Alpha = 8,
    All = 15,
};
constexpr ColorWriteMask operator|(ColorWriteMask a, ColorWriteMask b) { return ColorWriteMask(uint64_t(a) | uint64_t(b)); }
constexpr ColorWriteMask& operator|=(ColorWriteMask& a, ColorWriteMask b) { return a = a | b; }
constexpr ColorWriteMask operator&(ColorWriteMask a, ColorWriteMask b) { return ColorWriteMask(uint64_t(a) & uint64_t(b)); }
constexpr ColorWriteMask& operator&=(ColorWriteMask& a, ColorWriteMask b) { return a = a & b; }
constexpr ColorWriteMask operator^(ColorWriteMask a, ColorWriteMask b) { return ColorWriteMask(uint64_t(a) ^ uint64_t(b)); }
constexpr ColorWriteMask& operator^=(ColorWriteMask& a, ColorWriteMask b) { return a = a ^ b; }
constexpr bool operator!(ColorWriteMask a) { return uint64_t(a)==0; }
enum class CompareFunction : uint32_t {
    Undefined = 0,
    Never = 1,
    Less = 2,
    Equal = 3,
    LessEqual = 4,
    Greater = 5,
    NotEqual = 6,
    GreaterEqual = 7,
    Always = 8,
};
enum class ComponentSwizzle : uint32_t {
    Undefined = 0,
    Zero = 1,
    One = 2,
    R = 3,
    G = 4,
    B = 5,
    A = 6,
};
enum class CompositeAlphaMode : uint32_t {
    Auto = 0,
    Opaque = 1,
    Premultiplied = 2,
    Unpremultiplied = 3,
    Inherit = 4,
};
enum class CullMode : uint32_t {
    Undefined = 0,
    None = 1,
    Front = 2,
    Back = 3,
};
enum class ErrorFilter : uint32_t {
    Validation = 1,
    OutOfMemory = 2,
    Internal = 3,
};
enum class ErrorType : uint32_t {
    NoError = 1,
    Validation = 2,
    OutOfMemory = 3,
    Internal = 4,
    Unknown = 5,
};
enum class FeatureName : uint32_t {
    CoreFeaturesAndLimits = 1,
    DepthClipControl = 2,
    Depth32FloatStencil8 = 3,
    TextureCompressionBC = 4,
    TextureCompressionBCSliced3D = 5,
    TextureCompressionETC2 = 6,
    TextureCompressionASTC = 7,
    TextureCompressionASTCSliced3D = 8,
    TimestampQuery = 9,
    IndirectFirstInstance = 10,
    ShaderF16 = 11,
    RG11B10UfloatRenderable = 12,
    BGRA8UnormStorage = 13,
    Float32Filterable = 14,
    Float32Blendable = 15,
    ClipDistances = 16,
    DualSourceBlending = 17,
    Subgroups = 18,
    TextureFormatsTier1 = 19,
    TextureFormatsTier2 = 20,
    PrimitiveIndex = 21,
    TextureComponentSwizzle = 22,
    SubgroupSizeControl = 23,
    DawnInternalUsages = 327680,
    DawnMultiPlanarFormats = 327681,
    DawnNative = 327682,
    ChromiumExperimentalTimestampQueryInsidePasses = 327683,
    ImplicitDeviceSynchronization = 327684,
    TransientAttachments = 327686,
    MSAARenderToSingleSampled = 327687,
    D3D11MultithreadProtected = 327688,
    ANGLETextureSharing = 327689,
    PixelLocalStorageCoherent = 327690,
    PixelLocalStorageNonCoherent = 327691,
    Unorm16TextureFormats = 327692,
    MultiPlanarFormatExtendedUsages = 327693,
    MultiPlanarFormatP010 = 327694,
    HostMappedPointer = 327695,
    MultiPlanarRenderTargets = 327696,
    MultiPlanarFormatNv12a = 327697,
    FramebufferFetch = 327698,
    BufferMapExtendedUsages = 327699,
    AdapterPropertiesMemoryHeaps = 327700,
    AdapterPropertiesD3D = 327701,
    AdapterPropertiesVk = 327702,
    DawnFormatCapabilities = 327703,
    DawnDrmFormatCapabilities = 327704,
    MultiPlanarFormatNv16 = 327705,
    MultiPlanarFormatNv24 = 327706,
    MultiPlanarFormatP210 = 327707,
    MultiPlanarFormatP410 = 327708,
    SharedTextureMemoryVkDedicatedAllocation = 327709,
    SharedTextureMemoryAHardwareBuffer = 327710,
    SharedTextureMemoryDmaBuf = 327711,
    SharedTextureMemoryOpaqueFD = 327712,
    SharedTextureMemoryZirconHandle = 327713,
    SharedTextureMemoryDXGISharedHandle = 327714,
    SharedTextureMemoryD3D11Texture2D = 327715,
    SharedTextureMemoryIOSurface = 327716,
    SharedTextureMemoryEGLImage = 327717,
    SharedFenceVkSemaphoreOpaqueFD = 327718,
    SharedFenceSyncFD = 327719,
    SharedFenceVkSemaphoreZirconHandle = 327720,
    SharedFenceDXGISharedHandle = 327721,
    SharedFenceMTLSharedEvent = 327722,
    SharedBufferMemoryD3D12Resource = 327723,
    StaticSamplers = 327724,
    YCbCrVulkanSamplers = 327725,
    ShaderModuleCompilationOptions = 327726,
    DawnLoadResolveTexture = 327727,
    DawnPartialLoadResolveTexture = 327728,
    MultiDrawIndirect = 327729,
    DawnTexelCopyBufferRowAlignment = 327730,
    FlexibleTextureViews = 327731,
    ChromiumExperimentalSubgroupMatrix = 327732,
    SharedFenceEGLSync = 327733,
    DawnDeviceAllocatorControl = 327734,
    AdapterPropertiesWGPU = 327735,
    SharedBufferMemoryFromWindowsHandle = 327736,
    SharedTextureMemoryD3D12Resource = 327737,
    ChromiumExperimentalSamplingResourceTable = 327738,
    AtomicVec2uMinMax = 327739,
    Unorm16FormatsForExternalTexture = 327740,
    OpaqueYCbCrAndroidForExternalTexture = 327741,
    Unorm16Filterable = 327742,
    RenderPassRenderArea = 327743,
    AdapterPropertiesDrm = 327744,
    TextureCompressionUnaligned = 327745,
    DawnAllowUndefinedLoadStoreOp = 327746,
};
enum class FilterMode : uint32_t {
    Undefined = 0,
    Nearest = 1,
    Linear = 2,
};
enum class FrontFace : uint32_t {
    Undefined = 0,
    CCW = 1,
    CW = 2,
};
enum class IndexFormat : uint32_t {
    Undefined = 0,
    Uint16 = 1,
    Uint32 = 2,
};
enum class LoadOp : uint32_t {
    Undefined = 0,
    Load = 1,
    Clear = 2,
    ExpandResolveTexture = 327683,
};
enum class MapAsyncStatus : uint32_t {
    Success = 1,
    CallbackCancelled = 2,
    Error = 3,
    Aborted = 4,
};
enum class MapMode : uint64_t {
    None = 0,
    Read = 1,
    Write = 2,
};
constexpr MapMode operator|(MapMode a, MapMode b) { return MapMode(uint64_t(a) | uint64_t(b)); }
constexpr MapMode& operator|=(MapMode& a, MapMode b) { return a = a | b; }
constexpr MapMode operator&(MapMode a, MapMode b) { return MapMode(uint64_t(a) & uint64_t(b)); }
constexpr MapMode& operator&=(MapMode& a, MapMode b) { return a = a & b; }
constexpr MapMode operator^(MapMode a, MapMode b) { return MapMode(uint64_t(a) ^ uint64_t(b)); }
constexpr MapMode& operator^=(MapMode& a, MapMode b) { return a = a ^ b; }
constexpr bool operator!(MapMode a) { return uint64_t(a)==0; }
enum class MipmapFilterMode : uint32_t {
    Undefined = 0,
    Nearest = 1,
    Linear = 2,
};
enum class PopErrorScopeStatus : uint32_t {
    Success = 1,
    CallbackCancelled = 2,
    Error = 3,
};
enum class PresentMode : uint32_t {
    Undefined = 0,
    Fifo = 1,
    FifoRelaxed = 2,
    Immediate = 3,
    Mailbox = 4,
};
enum class PrimitiveTopology : uint32_t {
    Undefined = 0,
    PointList = 1,
    LineList = 2,
    LineStrip = 3,
    TriangleList = 4,
    TriangleStrip = 5,
};
enum class QueryType : uint32_t {
    Occlusion = 1,
    Timestamp = 2,
};
enum class QueueWorkDoneStatus : uint32_t {
    Success = 1,
    CallbackCancelled = 2,
    Error = 3,
};
enum class SamplerBindingType : uint32_t {
    BindingNotUsed = 0,
    Undefined = 1,
    Filtering = 2,
    NonFiltering = 3,
    Comparison = 4,
};
enum class ShaderStage : uint64_t {
    None = 0,
    Vertex = 1,
    Fragment = 2,
    Compute = 4,
};
constexpr ShaderStage operator|(ShaderStage a, ShaderStage b) { return ShaderStage(uint64_t(a) | uint64_t(b)); }
constexpr ShaderStage& operator|=(ShaderStage& a, ShaderStage b) { return a = a | b; }
constexpr ShaderStage operator&(ShaderStage a, ShaderStage b) { return ShaderStage(uint64_t(a) & uint64_t(b)); }
constexpr ShaderStage& operator&=(ShaderStage& a, ShaderStage b) { return a = a & b; }
constexpr ShaderStage operator^(ShaderStage a, ShaderStage b) { return ShaderStage(uint64_t(a) ^ uint64_t(b)); }
constexpr ShaderStage& operator^=(ShaderStage& a, ShaderStage b) { return a = a ^ b; }
constexpr bool operator!(ShaderStage a) { return uint64_t(a)==0; }
enum class StencilOperation : uint32_t {
    Undefined = 0,
    Keep = 1,
    Zero = 2,
    Replace = 3,
    Invert = 4,
    IncrementClamp = 5,
    DecrementClamp = 6,
    IncrementWrap = 7,
    DecrementWrap = 8,
};
enum class StorageTextureAccess : uint32_t {
    BindingNotUsed = 0,
    Undefined = 1,
    WriteOnly = 2,
    ReadOnly = 3,
    ReadWrite = 4,
};
enum class StoreOp : uint32_t {
    Undefined = 0,
    Store = 1,
    Discard = 2,
};
enum class TextureAspect : uint32_t {
    Undefined = 0,
    All = 1,
    StencilOnly = 2,
    DepthOnly = 3,
    Plane0Only = 327680,
    Plane1Only = 327681,
    Plane2Only = 327682,
};
enum class TextureDimension : uint32_t {
    Undefined = 0,
    e1D = 1,
    e2D = 2,
    e3D = 3,
};
enum class TextureFormat : uint32_t {
    Undefined = 0,
    R8Unorm = 1,
    R8Snorm = 2,
    R8Uint = 3,
    R8Sint = 4,
    R16Unorm = 5,
    R16Snorm = 6,
    R16Uint = 7,
    R16Sint = 8,
    R16Float = 9,
    RG8Unorm = 10,
    RG8Snorm = 11,
    RG8Uint = 12,
    RG8Sint = 13,
    R32Float = 14,
    R32Uint = 15,
    R32Sint = 16,
    RG16Unorm = 17,
    RG16Snorm = 18,
    RG16Uint = 19,
    RG16Sint = 20,
    RG16Float = 21,
    RGBA8Unorm = 22,
    RGBA8UnormSrgb = 23,
    RGBA8Snorm = 24,
    RGBA8Uint = 25,
    RGBA8Sint = 26,
    BGRA8Unorm = 27,
    BGRA8UnormSrgb = 28,
    RGB10A2Uint = 29,
    RGB10A2Unorm = 30,
    RG11B10Ufloat = 31,
    RGB9E5Ufloat = 32,
    RG32Float = 33,
    RG32Uint = 34,
    RG32Sint = 35,
    RGBA16Unorm = 36,
    RGBA16Snorm = 37,
    RGBA16Uint = 38,
    RGBA16Sint = 39,
    RGBA16Float = 40,
    RGBA32Float = 41,
    RGBA32Uint = 42,
    RGBA32Sint = 43,
    Stencil8 = 44,
    Depth16Unorm = 45,
    Depth24Plus = 46,
    Depth24PlusStencil8 = 47,
    Depth32Float = 48,
    Depth32FloatStencil8 = 49,
    BC1RGBAUnorm = 50,
    BC1RGBAUnormSrgb = 51,
    BC2RGBAUnorm = 52,
    BC2RGBAUnormSrgb = 53,
    BC3RGBAUnorm = 54,
    BC3RGBAUnormSrgb = 55,
    BC4RUnorm = 56,
    BC4RSnorm = 57,
    BC5RGUnorm = 58,
    BC5RGSnorm = 59,
    BC6HRGBUfloat = 60,
    BC6HRGBFloat = 61,
    BC7RGBAUnorm = 62,
    BC7RGBAUnormSrgb = 63,
    ETC2RGB8Unorm = 64,
    ETC2RGB8UnormSrgb = 65,
    ETC2RGB8A1Unorm = 66,
    ETC2RGB8A1UnormSrgb = 67,
    ETC2RGBA8Unorm = 68,
    ETC2RGBA8UnormSrgb = 69,
    EACR11Unorm = 70,
    EACR11Snorm = 71,
    EACRG11Unorm = 72,
    EACRG11Snorm = 73,
    ASTC4x4Unorm = 74,
    ASTC4x4UnormSrgb = 75,
    ASTC5x4Unorm = 76,
    ASTC5x4UnormSrgb = 77,
    ASTC5x5Unorm = 78,
    ASTC5x5UnormSrgb = 79,
    ASTC6x5Unorm = 80,
    ASTC6x5UnormSrgb = 81,
    ASTC6x6Unorm = 82,
    ASTC6x6UnormSrgb = 83,
    ASTC8x5Unorm = 84,
    ASTC8x5UnormSrgb = 85,
    ASTC8x6Unorm = 86,
    ASTC8x6UnormSrgb = 87,
    ASTC8x8Unorm = 88,
    ASTC8x8UnormSrgb = 89,
    ASTC10x5Unorm = 90,
    ASTC10x5UnormSrgb = 91,
    ASTC10x6Unorm = 92,
    ASTC10x6UnormSrgb = 93,
    ASTC10x8Unorm = 94,
    ASTC10x8UnormSrgb = 95,
    ASTC10x10Unorm = 96,
    ASTC10x10UnormSrgb = 97,
    ASTC12x10Unorm = 98,
    ASTC12x10UnormSrgb = 99,
    ASTC12x12Unorm = 100,
    ASTC12x12UnormSrgb = 101,
    R8BG8Biplanar420Unorm = 327680,
    R10X6BG10X6Biplanar420Unorm = 327681,
    R8BG8A8Triplanar420Unorm = 327682,
    R8BG8Biplanar422Unorm = 327683,
    R8BG8Biplanar444Unorm = 327684,
    R10X6BG10X6Biplanar422Unorm = 327685,
    R10X6BG10X6Biplanar444Unorm = 327686,
    OpaqueYCbCrAndroid = 327687,
};
enum class TextureSampleType : uint32_t {
    BindingNotUsed = 0,
    Undefined = 1,
    Float = 2,
    UnfilterableFloat = 3,
    Depth = 4,
    Sint = 5,
    Uint = 6,
};
enum class TextureUsage : uint64_t {
    None = 0,
    CopySrc = 1,
    CopyDst = 2,
    TextureBinding = 4,
    StorageBinding = 8,
    RenderAttachment = 16,
    TransientAttachment = 32,
    StorageAttachment = 327744,
};
constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) { return TextureUsage(uint64_t(a) | uint64_t(b)); }
constexpr TextureUsage& operator|=(TextureUsage& a, TextureUsage b) { return a = a | b; }
constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) { return TextureUsage(uint64_t(a) & uint64_t(b)); }
constexpr TextureUsage& operator&=(TextureUsage& a, TextureUsage b) { return a = a & b; }
constexpr TextureUsage operator^(TextureUsage a, TextureUsage b) { return TextureUsage(uint64_t(a) ^ uint64_t(b)); }
constexpr TextureUsage& operator^=(TextureUsage& a, TextureUsage b) { return a = a ^ b; }
constexpr bool operator!(TextureUsage a) { return uint64_t(a)==0; }
enum class TextureViewDimension : uint32_t {
    Undefined = 0,
    e1D = 1,
    e2D = 2,
    e2DArray = 3,
    Cube = 4,
    CubeArray = 5,
    e3D = 6,
};
enum class VertexFormat : uint32_t {
    Uint8 = 1,
    Uint8x2 = 2,
    Uint8x4 = 3,
    Sint8 = 4,
    Sint8x2 = 5,
    Sint8x4 = 6,
    Unorm8 = 7,
    Unorm8x2 = 8,
    Unorm8x4 = 9,
    Snorm8 = 10,
    Snorm8x2 = 11,
    Snorm8x4 = 12,
    Uint16 = 13,
    Uint16x2 = 14,
    Uint16x4 = 15,
    Sint16 = 16,
    Sint16x2 = 17,
    Sint16x4 = 18,
    Unorm16 = 19,
    Unorm16x2 = 20,
    Unorm16x4 = 21,
    Snorm16 = 22,
    Snorm16x2 = 23,
    Snorm16x4 = 24,
    Float16 = 25,
    Float16x2 = 26,
    Float16x4 = 27,
    Float32 = 28,
    Float32x2 = 29,
    Float32x3 = 30,
    Float32x4 = 31,
    Uint32 = 32,
    Uint32x2 = 33,
    Uint32x3 = 34,
    Uint32x4 = 35,
    Sint32 = 36,
    Sint32x2 = 37,
    Sint32x3 = 38,
    Sint32x4 = 39,
    Unorm10_10_10_2 = 40,
    Unorm8x4BGRA = 41,
    Snorm10_10_10_2 = 42,
};
enum class VertexStepMode : uint32_t {
    Undefined = 0,
    Vertex = 1,
    Instance = 2,
};
enum class WaitStatus : uint32_t {
    Success = 1,
    TimedOut = 2,
    Error = 3,
};
}
