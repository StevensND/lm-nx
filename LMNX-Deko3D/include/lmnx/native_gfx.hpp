#pragma once
#include "backend.hpp"
#include "efb_pack.hpp"
#include <array>
#include <functional>
#include <future>
#include <span>
#include <variant>

// Native contract replacing Aurora graphics objects and callbacks. No dependency on WebGPU headers, objects,
// validators or devices; the Aurora GX adapter uses this boundary.
namespace lmnx::gfx {
template<class T> struct Recurso {
    std::shared_ptr<plume::RenderDevice> dispositivo;
    std::unique_ptr<T> objeto;
};
struct Textura : Recurso<plume::RenderTexture> { plume::RenderTextureDesc descripcion; };
struct Buffer : Recurso<plume::RenderBuffer> { uint64_t bytes = 0; };
using RefTextura = std::shared_ptr<Textura>;
using RefBuffer = std::shared_ptr<Buffer>;
using Sampler = Recurso<plume::RenderSampler>;
using RefSampler = std::shared_ptr<Sampler>;
struct Muestreo { RefTextura textura; RefSampler sampler; };
struct Pipeline : Recurso<plume::RenderPipeline> {
    // The pipeline retains the layout and modules referenced by plume.
    std::unique_ptr<plume::RenderPipelineLayout> layout;
    std::unique_ptr<plume::RenderShader> vertice, fragmento;
    uint32_t texturasVertice = 0, texturasFragmento = 0;
    uint32_t uniformes = 0, almacenamiento = 0;
    uint32_t colores = 0, muestras = 1;
    std::array<plume::RenderFormat, 8> formatos{};
    plume::RenderFormat formatoProfundidad = plume::RenderFormat::UNKNOWN;
    ~Pipeline();
};
using RefPipeline = std::shared_ptr<Pipeline>;
struct SolicitudPipeline {
    std::string vertice, fragmento;
    plume::RenderGraphicsPipelineDesc estado;
    std::vector<plume::RenderInputSlot> slots;
    std::vector<plume::RenderInputElement> atributos;
    std::vector<plume::RenderSpecConstant> constantes;
    // Explicit GLSL; reflect these masks in plume metadata.
    uint32_t uboVertice = 0, ssboVertice = 0, uboFragmento = 0, ssboFragmento = 0;
    uint32_t texturasVertice = 0, texturasFragmento = 0;
};
struct EnlaceBuffer { RefBuffer buffer; uint32_t slot = 0; uint64_t offset = 0; uint32_t bytes = 0; };
struct Dibujo {
    RefPipeline pipeline;
    std::vector<EnlaceBuffer> uniformes, almacenamiento;
    RefBuffer indices;
    uint32_t offsetIndices = 0, cantidad = 0, instancias = 1;
    int32_t baseVertice = 0;
    std::array<uint32_t, 16> inmediatos{};
    uint32_t bytesInmediatos = 0;
    std::array<float, 4> colorMezcla{};
    std::array<Muestreo, 8> texturas;
};
struct Scissor { int32_t x = 0, y = 0, ancho = 0, alto = 0; };
using Comando = std::variant<plume::RenderViewport, Scissor, Dibujo>;
struct Paso {
    std::vector<RefTextura> colores;
    RefTextura profundidad;
    std::vector<plume::RenderColor> limpiarColores;
    bool limpiarProfundidad = false;
    float z = 1.f;
    std::vector<Comando> comandos;
};
struct Copia { RefTextura origen, destino; uint32_t x = 0, y = 0, ancho = 0, alto = 0, destinoX = 0, destinoY = 0; };
struct Lectura { RefTextura origen; uint32_t x = 0, y = 0, ancho = 0, alto = 0;
    std::function<void(std::span<const uint8_t>)> completar;
    FormatoLectura formato = FormatoLectura::Lineal; };
struct CargaBuffer { RefBuffer destino; uint64_t offset = 0; std::vector<uint8_t> datos; };
// Retain captured resources until the fence. The function encodes actual plume/deko3d commands on this same
// queue.
struct OperacionNativa { std::function<void(plume::RenderCommandList&)> codificar; };
using Operacion = std::variant<Paso, Copia, Lectura, CargaBuffer, OperacionNativa>;
struct Paquete { std::vector<Operacion> operaciones; RefTextura presentar; };

class Contexto {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    Contexto();
    ~Contexto();
    Contexto(const Contexto&) = delete;
    Contexto& operator=(const Contexto&) = delete;
    RefTextura crearTextura(const plume::RenderTextureDesc& descripcion);
    RefBuffer crearBuffer(uint64_t bytes, plume::RenderBufferFlags flags = plume::RenderBufferFlag::NONE);
    RefSampler crearSampler(const plume::RenderSamplerDesc& descripcion);
    std::shared_ptr<plume::RenderDevice> dispositivo() const;
    // Resolve on a persistent worker; get() propagates uam errors.
    std::future<RefPipeline> compilar(SolicitudPipeline solicitud);
    // Wait for the submission fence before releasing packets or reusing staging. Notify CPU readbacks after the
    // fence.
    void ejecutar(const Paquete& paquete);
    uint32_t ancho() const;
    uint32_t alto() const;
    void esperarVblank();
    void configurarVsync(bool activo);
};
Scissor recortar(Scissor rectangulo, uint32_t ancho, uint32_t alto);
}
