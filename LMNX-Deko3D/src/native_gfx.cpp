#include <functional>
#include <atomic>
#include <chrono>
#include <cstdint>
// Per-frame CPU measurements; inline variables match aurora_gpu.cpp because the graphics test links only this
// file.
#ifdef __SWITCH__
// The screen size chosen at start-up (platforms/switch/sdl3_shim: 1920x1080 docked, 1280x720 handheld); weak: without
// it the window keeps its default 1280x720.
extern "C" __attribute__((weak)) void lmnx_pantalla_tam(int* w, int* h);
namespace lmnx::saltynx { __attribute__((weak)) void Latir(uint32_t, uint32_t); __attribute__((weak)) void Actualizar(double, double, uint32_t, uint32_t, uint64_t); }
#endif
namespace lmnx::medidas { inline std::atomic<uint64_t> grabarNs{0}, dibujosGrabados{0}, ejecutarNs{0}, enviarNs{0}, presentarNs{0}, cambiosPipeline{0}, enlacesTexturas{0}, enlacesUbo{0}, lotes{0}; }
static uint64_t nsDesdeMedida(std::chrono::steady_clock::time_point t0){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t0).count());}
#include <lmnx/native_gfx.hpp>
#include <lmnx/shader_source.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstdio>

namespace plume { extern std::atomic<uint64_t> g_invalidacionesDescriptores, g_invalidacionesShader; }
#include <lmnx/perfil_vivo.hpp>
// Performance panel: latest snapshot published by Contexto::Impl::publicarPerfil on the render thread, read by
// the menu on the main thread.
static std::mutex g_perfilMutex;
static lmnx::perfil::Detalle g_perfil;
void lmnx::perfil::leer(Detalle& destino) { std::lock_guard bloqueo(g_perfilMutex); destino = g_perfil; }
namespace lmnx::gfx {
using namespace plume;
namespace {
void exigir(bool valor, const char* error) { if (!valor) throw std::runtime_error(error); }
void barrera(RenderCommandList& cmd, const RefTextura& textura, RenderTextureLayout layout) {
    exigir(textura && textura->dispositivo.get() == static_cast<Deko3DCommandList&>(cmd).device, "Textura de otro dispositivo o nula");
    cmd.barriers(RenderBarrierStage::ALL, RenderTextureBarrier(textura->objeto.get(), layout));
}
void region(const RefTextura& t, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    exigir(t && t->objeto && w && h && x <= t->descripcion.width && y <= t->descripcion.height &&
        w <= t->descripcion.width - x && h <= t->descripcion.height - y, "Texture region out of bounds");
    exigir(t->descripcion.multisampling.sampleCount == 1, "Native copying requires an explicit MSAA resolve");
}
std::unique_ptr<RenderShader> shader(RenderDevice& d, DkStage etapa, std::string fuente, uint32_t ubo, uint32_t ssbo) {
    fuente = prepararGLSL(std::move(fuente), ubo, ssbo);
    Deko3DShaderBlob cabecera;
    cabecera.stage = uint32_t(etapa); cabecera.glslSize = uint32_t(fuente.size());
    std::vector<uint8_t> blob(sizeof cabecera + fuente.size());
    std::memcpy(blob.data(), &cabecera, sizeof cabecera);
    std::memcpy(blob.data() + sizeof cabecera, fuente.data(), fuente.size());
    auto salida = d.createShader(blob.data(), blob.size(), "main", RenderShaderFormat::DEKO3D);
    exigir(bool(salida), "Could not create the native GLSL module");
    return salida;
}
}
Pipeline::~Pipeline() { objeto.reset(); }
Scissor recortar(Scissor r, uint32_t w, uint32_t h) {
    const int64_t x0 = std::clamp<int64_t>(r.x, 0, w), y0 = std::clamp<int64_t>(r.y, 0, h);
    const int64_t x1 = std::clamp<int64_t>(int64_t(r.x) + std::max(r.ancho, 0), x0, w);
    const int64_t y1 = std::clamp<int64_t>(int64_t(r.y) + std::max(r.alto, 0), y0, h);
    return {int32_t(x0), int32_t(y0), int32_t(x1 - x0), int32_t(y1 - y0)};
}
// Global GPU wait for aurora_gpu.cpp readbacks and work-done requests. Recursive because a read may complete
// inside ejecutar().
static std::function<void()> g_esperarTodo;
static std::vector<std::string>* g_marcaRanura = nullptr;  // Labels for the submission being recorded (marcaGpu).
static DkGpuAddr g_marcaBase = 0;
// Asynchronous readbacks (aurora_gpu.cpp, Buffer::map): number each submission, query completion without
// blocking or wait only through that submission. Avoid waiting for the whole GPU on each Aurora EFB readback.
static std::function<uint64_t()> g_enviados;
static std::function<bool(uint64_t)> g_sondear;
static std::function<void(uint64_t)> g_esperarHasta;
static std::recursive_mutex g_implMutex;
struct Contexto::Impl {
    std::unique_ptr<RenderInterface> interfaz = crearBackend();
    std::shared_ptr<RenderDevice> dispositivo = interfaz->createDevice();
    std::unique_ptr<RenderCommandQueue> cola;
    std::unique_ptr<RenderSwapChain> ventana;
    std::unique_ptr<RenderCommandList> cmd;
    std::unique_ptr<RenderCommandFence> valla;
    // In-flight submissions, as in SwitchWaker. Each slot retains its resources until its fence, allowing CPU and
    // GPU to overlap.
    struct Ranura {
        std::unique_ptr<RenderCommandList> cmd; std::unique_ptr<RenderCommandFence> valla; bool pendiente = false;
        std::vector<std::unique_ptr<RenderFramebuffer>> framebuffers; std::vector<std::unique_ptr<RenderBuffer>> temporales;
        std::vector<std::unique_ptr<RenderDescriptorSet>> descriptores; Paquete retenido; uint64_t secuencia = 0; std::vector<std::string> etiquetas;
        uint64_t cuadro = 0;  // Owning frame for the GPU Frame panel.
        uint32_t generacion = 0;  // Panel generation at submission time.
        void liberar() { framebuffers.clear(); temporales.clear(); descriptores.clear(); retenido = {}; }
    };
    // Sixteen slots: each Aurora WriteBuffer/WriteTexture is a separate submission. A four-slot ring can fill within
    // one frame and make the main submission wait for the GPU.
    std::array<Ranura, 16> ranuras; uint32_t siguiente = 0;
    uint64_t enviosMedida = 0, esperaMedidaNs = 0;  // Per-frame values logged every five seconds. One ordered queue: completing submission N means all earlier
// submissions completed.
    uint64_t enviados = 0, terminados = 0;
    // Actual GPU time: 16-byte timestamps at each submission's start and end per slot. GPU ticks are 625/384 ns. Add
    // a labeled marcaGpu before each render pass, copy or upload from aurora_gpu.cpp; intervals measure each
    // operation, summarized by label every five seconds.
    static constexpr size_t kMarcas = 128, kBytesMarcas = kMarcas * 16;
    std::unique_ptr<RenderBuffer> marcas; uint64_t gpuOcupadaNs = 0;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> porEtiqueta;  // ns y veces
    inline static const std::string kInicioEnvio = "(submission start)";
    // Performance panel (lmnx/perfil_vivo.hpp), only while enabled. Record Present, GPU Frame and Render thread
    // series each presentation, like banjo-nx RecordHostPresent/SetGpuFrameMs/SetRenderThreadMs. Refresh extra
    // render-thread, draw and costly-pass details every 0.5 seconds. Read lmnx::medidas by differences from
    // panelPrevio; the five-second log resets counters and adjusts that baseline. For GPU Frame, tag each submission
    // with cuadroEnvio and the panel generation. Each frame accumulates time and outstanding submissions; publish
    // once presented and fully collected, regardless of slot collection order.
    struct CuadroGpu { uint64_t cuadro = UINT64_MAX, ns = 0; uint32_t pendientes = 0, generacion = 0; bool presentado = false; };
    std::array<CuadroGpu, 8> gpuCuadros{};
    uint64_t cuadroEnvio = 0;
    uint32_t panelGeneracion = 0;
    uint64_t esperaPanelNs = 0, enviosPanel = 0;  // CPU fence-wait time and submissions since the last publication.
    void anotarEnvioGpu(Ranura& r) {
        r.cuadro = cuadroEnvio; r.generacion = lmnx::perfil::generacion.load(std::memory_order_relaxed);
        CuadroGpu& c = gpuCuadros[cuadroEnvio % gpuCuadros.size()];
        if (c.cuadro != cuadroEnvio) c = CuadroGpu{cuadroEnvio, 0, 0, r.generacion, false};
        ++c.pendientes;
    }
    void cerrarCuadroGpu(CuadroGpu& c) {  // Publish the frame if complete.
        if (c.cuadro == UINT64_MAX || !c.presentado || c.pendientes != 0) return;
        if (lmnx::perfil::activo.load(std::memory_order_relaxed) &&
            c.generacion == lmnx::perfil::generacion.load(std::memory_order_relaxed) && c.ns != 0)
            lmnx::perfil::guardar(lmnx::perfil::Gpu, double(c.ns) * 1e-6);
        c.cuadro = UINT64_MAX;
    }
    void presentadoGpu() {
        CuadroGpu& c = gpuCuadros[cuadroEnvio % gpuCuadros.size()];
        if (c.cuadro == cuadroEnvio) { c.presentado = true; cerrarCuadroGpu(c); }
        ++cuadroEnvio;
    }
    std::unordered_map<std::string, uint64_t> porEtiquetaPanel;
    std::chrono::steady_clock::time_point panelUltimo{}, panelInicio{};
    uint32_t panelCuadros = 0;
    // Difference counters: 0 record, 1 execute, 2 submit, 3 present (ns), 4 draws, 5 batches, 6 pipeline changes, 7
    // texture bindings, 8 uniform bindings (lmnx::medidas, reset by logging), 9 full barriers and 10 submitted
    // segments (plume totals, never reset).
    static constexpr int kContadores = 11;
    int64_t panelPrevio[kContadores] = {};
    double panelAcum[kContadores] = {};
    lmnx::perfil::Detalle panel;
    static void leerContadores(uint64_t (&v)[kContadores]) {
        using namespace lmnx::medidas;
        auto& st = GetDeko3DStats();
        v[0] = grabarNs.load(); v[1] = ejecutarNs.load(); v[2] = enviarNs.load(); v[3] = presentarNs.load();
        v[4] = dibujosGrabados.load(); v[5] = lotes.load(); v[6] = cambiosPipeline.load(); v[7] = enlacesTexturas.load();
        v[8] = enlacesUbo.load(); v[9] = st.barriersFull.load(); v[10] = st.segmentosEnviados.load();
    }
    void publicarPerfil() {
        using namespace lmnx::perfil;
        const auto ahora = std::chrono::steady_clock::now();
        uint64_t valores[kContadores];
        leerContadores(valores);
        // Start from zero when enabled, including reenabling without an intervening disabled presentation.
        const uint32_t generacionActual = lmnx::perfil::generacion.load(std::memory_order_relaxed);
        if (generacionActual != panelGeneracion) { panelGeneracion = generacionActual; panelUltimo = {}; }
        if (panelUltimo == std::chrono::steady_clock::time_point{}) {
            for (int k = 0; k < kContadores; ++k) { panelPrevio[k] = int64_t(valores[k]); panelAcum[k] = 0; }
            porEtiquetaPanel.clear(); panelCuadros = 0; esperaPanelNs = 0; enviosPanel = 0;
            panel = {};  // Extra lines show measuring rather than previous-session values.
            { std::lock_guard bloqueo(g_perfilMutex); g_perfil = panel; }
            panelUltimo = panelInicio = ahora; return;
        }
        double d[kContadores];
        for (int k = 0; k < kContadores; ++k) {
            d[k] = double(std::max<int64_t>(int64_t(valores[k]) - panelPrevio[k], 0));
            panelPrevio[k] = int64_t(valores[k]);
            panelAcum[k] += d[k];
        }
        guardar(Render, (d[0] + d[1] + d[2] + d[3]) * 1e-6);
        guardar(Presentar, std::chrono::duration<double, std::milli>(ahora - panelUltimo).count()); panelUltimo = ahora;
        ++panelCuadros;
        const double segundos = std::chrono::duration<double>(ahora - panelInicio).count();
        if (segundos < 0.5) return;
        const double c = double(panelCuadros);
        panel.grabar = float(panelAcum[0] / c * 1e-6); panel.ejecutar = float(panelAcum[1] / c * 1e-6);
        panel.enviar = float(panelAcum[2] / c * 1e-6); panel.presentar = float(panelAcum[3] / c * 1e-6);
        panel.dibujos = float(panelAcum[4] / c); panel.lotes = float(panelAcum[5] / c);
        panel.pipelines = float(panelAcum[6] / c); panel.texturas = float(panelAcum[7] / c);
        panel.uniformes = float(panelAcum[8] / c); panel.barreras = float(panelAcum[9] / c);
        panel.segmentos = float(panelAcum[10] / c);
        panel.esperaGpu = float(double(esperaPanelNs) / c * 1e-6); panel.envios = float(double(enviosPanel) / c);
        esperaPanelNs = 0; enviosPanel = 0;
        {
            auto& st = GetDeko3DStats();
            panel.gpuMemMiB = float(double(st.bloquesVivosBytes.load()) / (1024.0 * 1024.0));
            panel.gpuMemPicoMiB = float(double(st.bloquesPicoBytes.load()) / (1024.0 * 1024.0));
        }
        for (auto& a : panelAcum) a = 0;
        std::vector<std::pair<uint64_t, const std::string*>> orden;
        for (const auto& [nombre, ns] : porEtiquetaPanel) orden.push_back({ns, &nombre});
        const size_t n = std::min<size_t>(4, orden.size());
        std::partial_sort(orden.begin(), orden.begin() + n, orden.end(),
                          [](const auto& a, const auto& b) { return a.first > b.first; });
        for (size_t k = 0; k < 4; ++k) {
            std::snprintf(panel.pasada[k], sizeof(panel.pasada[k]), "%s", k < n ? orden[k].second->c_str() : "");
            panel.pasadaMs[k] = k < n ? float(double(orden[k].first) / c * 1e-6) : 0.0f;
        }
        porEtiquetaPanel.clear();
        panelCuadros = 0; panelInicio = ahora; ++panel.version;
        std::lock_guard bloqueo(g_perfilMutex); g_perfil = panel;
    }
    void leerMarcas(Ranura& r) {
        if (!marcas) return;
        auto* b = static_cast<Deko3DBuffer*>(marcas.get());
        auto* p = reinterpret_cast<uint64_t*>(b->cpu + size_t(&r - ranuras.data()) * kBytesMarcas);
        const size_t n = std::min(r.etiquetas.size() + 2, kMarcas);
        armDCacheFlush(p, n * 16);
        auto t = [&](size_t k) { return p[2 * k + 1] ? p[2 * k + 1] : p[2 * k]; };
        const bool panelActivo = lmnx::perfil::activo.load(std::memory_order_relaxed);
        const uint64_t ocupadaNs = t(0) && t(n - 1) > t(0) ? (t(n - 1) - t(0)) * 625 / 384 : 0;
        gpuOcupadaNs += ocupadaNs;
        {
            CuadroGpu& c = gpuCuadros[r.cuadro % gpuCuadros.size()];
            if (c.cuadro == r.cuadro && c.pendientes != 0) {
                c.ns += ocupadaNs; --c.pendientes;
                cerrarCuadroGpu(c);
            }
        }
        for (size_t k = 0; k + 1 < n; ++k) {
            if (!t(k) || t(k + 1) < t(k)) continue;
            const std::string& nombre = k == 0 ? kInicioEnvio : r.etiquetas[k - 1];
            auto& e = porEtiqueta[nombre];
            e.first += (t(k + 1) - t(k)) * 625 / 384; ++e.second;
            if (panelActivo) porEtiquetaPanel[nombre] += (t(k + 1) - t(k)) * 625 / 384;
        }
        std::memset(p, 0, n * 16); armDCacheFlush(p, n * 16);
        r.etiquetas.clear();
    }
    void retirar(Ranura& r) {
        const auto t0 = std::chrono::steady_clock::now();
        cola->waitForCommandFence(r.valla.get()); r.pendiente = false;
        const uint64_t espera = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
        esperaMedidaNs += espera; esperaPanelNs += espera;
        leerMarcas(r);
        terminados = std::max(terminados, r.secuencia); r.liberar();
    }
    Ranura& tomar() {
        auto& r = ranuras[siguiente]; siguiente = (siguiente + 1) % ranuras.size();
        if (r.pendiente) retirar(r); else r.liberar();
        return r;
    }
    void esperarTodo() {
        for (auto& r : ranuras) if (r.pendiente) retirar(r);
    }
    // Nonblocking: retire slots whose fences have signaled.
    bool sondear(uint64_t s) {
        if (s <= terminados) return true;
        for (auto& r : ranuras)
            if (r.pendiente && dkFenceWait(&static_cast<Deko3DCommandFence*>(r.valla.get())->fence, 0) == DkResult_Success) retirar(r);
        return s <= terminados;
    }
    void esperarHasta(uint64_t s) {
        while (s > terminados) {
            Ranura* menor = nullptr;
            for (auto& r : ranuras) if (r.pendiente && (!menor || r.secuencia < menor->secuencia)) menor = &r;
            if (!menor) { terminados = std::max(terminados, s); break; }
            retirar(*menor);
        }
    }
    std::mutex mutex;
    std::condition_variable despertar;
    std::deque<std::packaged_task<RefPipeline()>> trabajos;
    bool detener = false;
    std::thread trabajador;
    ViDisplay pantalla{};
    Event vblank{};
    bool tienePantalla = false, tieneVblank = false;
    Impl() {
        exigir(bool(dispositivo), "Could not create the deko3d device");
        cola = dispositivo->createCommandQueue(RenderCommandListType::DIRECT);
        exigir(bool(cola), "Could not create the native queue");
        // The swapchain takes the window's size (plume Deko3DSwapChain::resize): 1920x1080 when started docked.
        if (lmnx_pantalla_tam) {
            int w = 0, h = 0;
            lmnx_pantalla_tam(&w, &h);
            if (w > 0 && h > 0) nwindowSetDimensions(nwindowGetDefault(), u32(w), u32(h));
        }
        ventana = cola->createSwapChain({nwindowGetDefault(), RenderFormat::R8G8B8A8_UNORM, 2});
        exigir(ventana && !ventana->isEmpty(), "Could not create the native swapchain");
        ventana->setVsyncEnabled(true);
        cmd = cola->createCommandList(); valla = dispositivo->createCommandFence();
        for (auto& r : ranuras) { r.cmd = cola->createCommandList(); r.valla = dispositivo->createCommandFence(); exigir(r.cmd && r.valla, "Could not create a submission slot"); }
        // CPU-cached memory: leerMarcas invalidates before reading and flushes after zeroing.
        marcas = dispositivo->createBuffer(RenderBufferDesc::UploadBuffer(kBytesMarcas * ranuras.size(), RenderBufferFlag::HOST_CACHED));
        if (marcas && !static_cast<Deko3DBuffer*>(marcas.get())->cpu) marcas.reset();
        if (marcas) { std::memset(static_cast<Deko3DBuffer*>(marcas.get())->cpu, 0, kBytesMarcas * ranuras.size()); armDCacheFlush(static_cast<Deko3DBuffer*>(marcas.get())->cpu, kBytesMarcas * ranuras.size()); }
        g_esperarTodo = [this] { esperarTodo(); };
        g_enviados = [this] { return enviados; };
        g_sondear = [this](uint64_t s) { return sondear(s); };
        g_esperarHasta = [this](uint64_t s) { esperarHasta(s); };
        exigir(cmd && valla, "Could not create the encoder or its fence");
        // NWindow already owns VI initialization; this reference has its own lifetime.
        tienePantalla = R_SUCCEEDED(viOpenDefaultDisplay(&pantalla));
        if (tienePantalla) tieneVblank = R_SUCCEEDED(viGetDisplayVsyncEvent(&pantalla, &vblank));
        trabajador = std::thread([this] {
            svcSetThreadPriority(CUR_THREAD_HANDLE, 0x3b);
            for (;;) {
                std::packaged_task<RefPipeline()> tarea;
                {
                    std::unique_lock lock(mutex);
                    despertar.wait(lock, [&] { return detener || !trabajos.empty(); });
                    if (trabajos.empty() && detener) return;
                    tarea = std::move(trabajos.front()); trabajos.pop_front();
                }
                tarea();
            }
        });
    }
    ~Impl() {
        { std::lock_guard lock(g_implMutex); esperarTodo(); g_esperarTodo = nullptr; g_enviados = nullptr; g_sondear = nullptr; g_esperarHasta = nullptr; }
        { std::lock_guard lock(mutex); detener = true; }
        despertar.notify_one();
        if (trabajador.joinable()) trabajador.join();
        esperarColaAcotada(static_cast<Deko3DDevice*>(dispositivo.get())->queue);
        if (tieneVblank) eventClose(&vblank);
        if (tienePantalla) viCloseDisplay(&pantalla);
    }
    void enviar() {
        cmd->end(); cola->executeCommandLists(cmd.get(), valla.get());
        cola->waitForCommandFence(valla.get());
    }
};
Contexto::Contexto() : impl(std::make_unique<Impl>()) {}
Contexto::~Contexto() = default;
std::shared_ptr<RenderDevice> Contexto::dispositivo() const { return impl->dispositivo; }
void Contexto::configurarVsync(bool activo) { impl->ventana->setVsyncEnabled(activo); }
uint32_t Contexto::ancho() const { return impl->ventana->getWidth(); }
uint32_t Contexto::alto() const { return impl->ventana->getHeight(); }
void Contexto::esperarVblank() {
    exigir(impl->tieneVblank, "Could not obtain the VI vblank event");
    exigir(R_SUCCEEDED(eventWait(&impl->vblank, 1000000000ull)), "vblank wait failed or timed out");
}
RefTextura Contexto::crearTextura(const RenderTextureDesc& d) {
    exigir(d.width && d.height && d.width <= 8192 && d.height <= 8192, "Invalid texture dimensions");
    auto t = std::make_shared<Textura>(); t->dispositivo = impl->dispositivo; t->descripcion = d;
    t->objeto = t->dispositivo->createTexture(d);
    exigir(t->objeto && static_cast<Deko3DTexture*>(t->objeto.get())->valid, "Could not allocate the texture");
    return t;
}
RefBuffer Contexto::crearBuffer(uint64_t bytes, RenderBufferFlags flags) {
    exigir(bytes && bytes <= 128 * 1024 * 1024, "Invalid buffer size");
    auto b = std::make_shared<Buffer>(); b->dispositivo = impl->dispositivo; b->bytes = bytes;
    b->objeto = b->dispositivo->createBuffer(RenderBufferDesc::DefaultBuffer(bytes, flags));
    exigir(b->objeto && static_cast<Deko3DBuffer*>(b->objeto.get())->block, "Could not allocate the buffer");
    return b;
}
RefSampler Contexto::crearSampler(const RenderSamplerDesc& d) {
    auto s = std::make_shared<Sampler>(); s->dispositivo = impl->dispositivo;
    s->objeto = s->dispositivo->createSampler(d);
    exigir(bool(s->objeto), "Could not create the sampler");
    return s;
}
std::future<RefPipeline> Contexto::compilar(SolicitudPipeline s) {
    exigir(!s.estado.geometryShader && !s.estado.pipelineLayout && !s.estado.vertexShader && !s.estado.pixelShader &&
        !s.estado.inputSlots && !s.estado.inputElements && !s.estado.specConstants,
        "The request must own its data without external pointers");
    exigir(s.atributos.empty(), "El contrato GX nativo usa vertex pulling; atributos tradicionales pendientes");
    exigir(!s.estado.dynamicDepthBiasEnabled && !s.estado.dynamicStencilReferenceEnabled, "Dynamic bias/stencil state is not implemented");
    exigir(s.estado.renderTargetCount <= 8, "Demasiados attachments de pipeline");
    exigir(!(s.texturasVertice & ~255u) && !(s.texturasFragmento & ~255u), "GX admite ocho slots de textura");
    auto dispositivo = impl->dispositivo;
    std::packaged_task<RefPipeline()> tarea([dispositivo, s = std::move(s)]() mutable {
        auto p = std::make_shared<Pipeline>(); p->dispositivo = dispositivo;
        p->texturasVertice = s.texturasVertice; p->texturasFragmento = s.texturasFragmento;
        p->uniformes = s.uboVertice | s.uboFragmento; p->almacenamiento = s.ssboVertice | s.ssboFragmento;
        p->colores = s.estado.renderTargetCount; p->muestras = s.estado.multisampling.sampleCount;
        p->formatoProfundidad = s.estado.depthTargetFormat;
        std::copy_n(s.estado.renderTargetFormat, 8, p->formatos.begin());
        const RenderPushConstantRange push{15, 0, 0, 64, RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL};
        p->layout = dispositivo->createPipelineLayout({&push, 1, nullptr, 0});
        p->vertice = shader(*dispositivo, DkStage_Vertex, s.vertice, s.uboVertice, s.ssboVertice);
        if (!s.fragmento.empty()) p->fragmento = shader(*dispositivo, DkStage_Fragment, s.fragmento, s.uboFragmento, s.ssboFragmento);
        s.estado.pipelineLayout = p->layout.get(); s.estado.vertexShader = p->vertice.get(); s.estado.pixelShader = p->fragmento.get();
        s.estado.inputSlots = s.slots.data(); s.estado.inputSlotsCount = s.slots.size();
        s.estado.specConstants = s.constantes.data(); s.estado.specConstantsCount = s.constantes.size();
        p->objeto = dispositivo->createGraphicsPipeline(s.estado);
        exigir(p->objeto && static_cast<Deko3DGraphicsPipeline*>(p->objeto.get())->valid, "Pipeline compilation failed; see the uam log");
        return p;
    });
    auto futuro = tarea.get_future();
    { std::lock_guard lock(impl->mutex); impl->trabajos.push_back(std::move(tarea)); }
    impl->despertar.notify_one();
    return futuro;
}
// Labeled GPU timestamp from a native operation recorded inside ejecutar, on the same thread under g_implMutex.
// Outside ejecutar this does nothing.
void marcaGpu(plume::RenderCommandList& c, const std::string& etiqueta) {
    if (!g_marcaRanura || g_marcaRanura->size() + 2 >= 128) return;
    dkCmdBufReportCounter(static_cast<Deko3DCommandList&>(c).cmd, DkCounter_Timestamp, g_marcaBase + DkGpuAddr(1 + g_marcaRanura->size()) * 16);
    g_marcaRanura->push_back(etiqueta);
}
void esperarGpuGlobal() { std::lock_guard lock(g_implMutex); if (g_esperarTodo) g_esperarTodo(); }
uint64_t envioActual() { std::lock_guard lock(g_implMutex); return g_enviados ? g_enviados() : 0; }
bool envioTerminado(uint64_t s) { std::lock_guard lock(g_implMutex); return !g_sondear || g_sondear(s); }
void esperarEnvio(uint64_t s) { std::lock_guard lock(g_implMutex); if (g_esperarHasta) g_esperarHasta(s); }
void Contexto::ejecutar(const Paquete& paquete) {
    const auto t0Ejecutar=std::chrono::steady_clock::now();
    auto& i = *impl; std::lock_guard bloqueo(g_implMutex); auto& ranura = i.tomar(); auto& cmd = *ranura.cmd;
    // Submission resources remain in their slot until GPU completion.
    auto& framebuffers = ranura.framebuffers; auto& temporales = ranura.temporales; auto& descriptores = ranura.descriptores;
    struct Pendiente { const Lectura* peticion; RenderBuffer* buffer; size_t bytes; };
    std::vector<Pendiente> lecturas;
    cmd.begin();
    const DkGpuAddr marca = i.marcas ? static_cast<Deko3DBuffer*>(i.marcas.get())->gpu + DkGpuAddr(&ranura - i.ranuras.data()) * Impl::kBytesMarcas : 0;
    ranura.etiquetas.clear();
    if (marca) { dkCmdBufReportCounter(static_cast<Deko3DCommandList&>(cmd).cmd, DkCounter_Timestamp, marca); g_marcaRanura = &ranura.etiquetas; g_marcaBase = marca; }
    for (const auto& operacion : paquete.operaciones) { std::visit([&](const auto& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, Paso>) {
            exigir((!op.colores.empty() || op.profundidad) && op.colores.size() <= 8 && op.limpiarColores.size() <= op.colores.size(), "Invalid attachments");
            std::vector<const RenderTexture*> colores;
            const auto base = op.colores.empty() ? op.profundidad : op.colores.front();
            exigir(bool(base), "Attachment nulo");
            const auto w = base->descripcion.width, h = base->descripcion.height;
            for (const auto& t : op.colores) {
                exigir(t && t->descripcion.width == w && t->descripcion.height == h &&
                    t->descripcion.multisampling.sampleCount == base->descripcion.multisampling.sampleCount &&
                    (t->descripcion.flags & RenderTextureFlag::RENDER_TARGET), "Attachments incompatibles");
                barrera(cmd, t, RenderTextureLayout::COLOR_WRITE); colores.push_back(t->objeto.get());
            }
            if (op.profundidad) {
                exigir(op.profundidad->descripcion.width == w && op.profundidad->descripcion.height == h &&
                    op.profundidad->descripcion.multisampling.sampleCount == base->descripcion.multisampling.sampleCount &&
                    (op.profundidad->descripcion.flags & RenderTextureFlag::DEPTH_TARGET), "Profundidad incompatible");
                barrera(cmd, op.profundidad, RenderTextureLayout::DEPTH_WRITE);
            }
            framebuffers.push_back(i.dispositivo->createFramebuffer({colores.data(), uint32_t(colores.size()), op.profundidad ? op.profundidad->objeto.get() : nullptr}));
            exigir(bool(framebuffers.back()), "Could not create the framebuffer");
            cmd.setFramebuffer(framebuffers.back().get());
            cmd.setViewports(RenderViewport(0, 0, float(w), float(h)));
            cmd.setScissors(RenderRect(0, 0, w, h));
            for (size_t c = 0; c < op.limpiarColores.size(); ++c) cmd.clearColor(c, op.limpiarColores[c]);
            if (op.limpiarProfundidad) { exigir(bool(op.profundidad), "Depth clear without an attachment"); cmd.clearDepth(true, op.z); }
            for (const auto& comando : op.comandos) std::visit([&](const auto& c) {
                using C = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<C, RenderViewport>) cmd.setViewports(c);
                else if constexpr (std::is_same_v<C, Scissor>) {
                    auto r = recortar(c, w, h); cmd.setScissors(RenderRect(r.x, r.y, r.x + r.ancho, r.y + r.alto));
                } else {
                    exigir(c.pipeline && c.pipeline->objeto && c.cantidad && c.instancias, "Draw has no pipeline or geometry");
                    exigir(c.pipeline->dispositivo == i.dispositivo && c.pipeline->colores == op.colores.size() &&
                        c.pipeline->muestras == base->descripcion.multisampling.sampleCount &&
                        c.pipeline->formatoProfundidad == (op.profundidad ? op.profundidad->descripcion.format : RenderFormat::UNKNOWN), "Pipeline is incompatible with the pass or device");
                    for (size_t f = 0; f < op.colores.size(); ++f)
                        exigir(c.pipeline->formatos[f] == op.colores[f]->descripcion.format, "Incompatible pipeline format");
                    cmd.setGraphicsPipelineLayout(c.pipeline->layout.get()); cmd.setPipeline(c.pipeline->objeto.get());
                    uint32_t mascaraUbo = 0, mascaraSsbo = 0;
                    auto enlace = [&](const EnlaceBuffer& b, bool uniforme) {
                        exigir(b.buffer && b.buffer->dispositivo == i.dispositivo && b.bytes && b.offset <= b.buffer->bytes && b.bytes <= b.buffer->bytes - b.offset &&
                            b.slot < (uniforme ? 15u : 14u) && (!uniforme || !(b.offset & 255u)), "Invalid buffer binding");
                        const auto direccion = b.buffer->objeto->getDeviceAddress() + b.offset;
                        if (uniforme) mascaraUbo |= 1u << b.slot; else mascaraSsbo |= 1u << b.slot;
                        exigir(uniforme ? Deko3DBindUniform(&cmd, b.slot, direccion, b.bytes) : Deko3DBindStorage(&cmd, b.slot, direccion, b.bytes), "Could not bind the buffer");
                    };
                    for (auto& b : c.uniformes) enlace(b, true);
                    for (auto& b : c.almacenamiento) enlace(b, false);
                    exigir((mascaraUbo & (c.pipeline->uniformes & 0x7fffu)) == (c.pipeline->uniformes & 0x7fffu) &&
                        (mascaraSsbo & c.pipeline->almacenamiento) == c.pipeline->almacenamiento, "Buffers read by the shader are missing");
                    exigir(c.bytesInmediatos <= 64 && !(c.bytesInmediatos & 3u), "Invalid immediate data");
                    exigir(!(c.pipeline->uniformes & 0x8000u) || c.bytesInmediatos == 64, "The ABI requires all 64 bytes of immediate data");
                    if (c.bytesInmediatos) {
                        cmd.setGraphicsPushConstants(0, c.inmediatos.data(), 0, 64);
                        // plume uses UBO 5 for Renut; GX reserves 15.
                        const auto* d = static_cast<Deko3DDevice*>(i.dispositivo.get());
                        exigir(Deko3DBindUniform(&cmd, 15, d->pushAddr, Deko3DDevice::PushSize), "Failed to bind GX immediate data");
                    }
                    static_cast<Deko3DCommandList&>(cmd).setBlendConstant(c.colorMezcla.data());
                    const uint32_t mascaraTex = c.pipeline->texturasVertice | c.pipeline->texturasFragmento;
                    if (mascaraTex) {
                        const RenderDescriptorRange rangos[] = {{RenderDescriptorRangeType::TEXTURE, 0, 8}, {RenderDescriptorRangeType::SAMPLER, 0, 8}};
                        descriptores.push_back(i.dispositivo->createDescriptorSet({rangos, 2}));
                        auto* grupo = descriptores.back().get(); exigir(bool(grupo), "Could not allocate descriptors");
                        DkResHandle handles[8]{};
                        for (uint32_t k = 0; k < 8; ++k) if (mascaraTex & (1u << k)) {
                            const auto& m = c.texturas[k];
                            exigir(m.textura && m.sampler && m.sampler->dispositivo == i.dispositivo, "Texture/sampler missing or belongs to another device");
                            exigir(std::find(op.colores.begin(), op.colores.end(), m.textura) == op.colores.end() && op.profundidad != m.textura, "Feedback sobre un attachment; requiere snapshot");
                            barrera(cmd, m.textura, RenderTextureLayout::SHADER_READ);
                            grupo->setTexture(k, m.textura->objeto.get(), RenderTextureLayout::SHADER_READ);
                            grupo->setSampler(8 + k, m.sampler->objeto.get());
                            handles[k] = dkMakeTextureHandle(k, k);
                        }
                        cmd.setGraphicsDescriptorSet(grupo, 0);
                        auto& nativo = static_cast<Deko3DCommandList&>(cmd);
                        // This entry uses direct GX slots, without plume's Xenos ABI.
                        nativo.flushDescriptorInvalidate(); nativo.flushBarriers();
                        if (c.pipeline->texturasVertice) dkCmdBufBindTextures(nativo.cmd, DkStage_Vertex, 0, handles, 8);
                        if (c.pipeline->texturasFragmento) dkCmdBufBindTextures(nativo.cmd, DkStage_Fragment, 0, handles, 8);
                    }
                    if (c.indices) {
                        exigir(c.indices->dispositivo == i.dispositivo && !(c.offsetIndices & 1u) && c.offsetIndices <= c.indices->bytes && uint64_t(c.cantidad) * 2 <= c.indices->bytes - c.offsetIndices, "Indices are out of bounds or belong to another device");
                        const RenderIndexBufferView vista{c.indices->objeto->at(c.offsetIndices), c.cantidad * 2, RenderFormat::R16_UINT};
                        cmd.setIndexBuffer(&vista); cmd.drawIndexedInstanced(c.cantidad, c.instancias, 0, c.baseVertice, 0);
                    } else cmd.drawInstanced(c.cantidad, c.instancias, 0, 0);
                }
            }, comando);
        } else if constexpr (std::is_same_v<T, Copia> || std::is_same_v<T, Lectura>) {
            region(op.origen, op.x, op.y, op.ancho, op.alto);
            barrera(cmd, op.origen, RenderTextureLayout::COPY_SOURCE);
            const RenderBox caja{int32_t(op.x), int32_t(op.y), int32_t(op.x + op.ancho), int32_t(op.y + op.alto), 0, 1};
            if constexpr (std::is_same_v<T, Copia>) {
                region(op.destino, op.destinoX, op.destinoY, op.ancho, op.alto);
                exigir(op.origen != op.destino && op.origen->descripcion.format == op.destino->descripcion.format, "Overlapping copy or unsupported conversion");
                barrera(cmd, op.destino, RenderTextureLayout::COPY_DEST);
                cmd.copyTextureRegion(RenderTextureCopyLocation::Subresource(op.destino->objeto.get()), RenderTextureCopyLocation::Subresource(op.origen->objeto.get()), op.destinoX, op.destinoY, 0, &caja);
            } else {
                exigir(op.origen->descripcion.format == RenderFormat::R8G8B8A8_UNORM || op.origen->descripcion.format == RenderFormat::D32_FLOAT, "Readback supports only RGBA8 or D32; GX conversion is not implemented");
                exigir(op.formato == FormatoLectura::Lineal || op.origen->descripcion.format == RenderFormat::R8G8B8A8_UNORM, "GX repacking requires RGBA8 color; Z conversion is not implemented");
                const size_t bytes = size_t(op.ancho) * op.alto * 4;
                // CPU-cached memory, supplied by plume through HOST_CACHED uploads. Invalidate the range with armDCacheFlush
                // after the fence and before reading; the CPU never writes to it. This avoids slow uncached reads.
                temporales.push_back(i.dispositivo->createBuffer(RenderBufferDesc::UploadBuffer(bytes, RenderBufferFlag::HOST_CACHED)));
                auto* b = temporales.back().get(); exigir(bool(b), "Failed to allocate readback");
                // Clear old dirty cache lines so they cannot overwrite the GPU's copy.
                if (auto* cpuLectura = static_cast<Deko3DBuffer*>(b)->cpu) armDCacheFlush(cpuLectura, bytes);
                cmd.copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(b, op.origen->descripcion.format, op.ancho, op.alto, 1, op.ancho), RenderTextureCopyLocation::Subresource(op.origen->objeto.get()), 0, 0, 0, &caja);
                lecturas.push_back({&op, b, bytes});
            }
        } else if constexpr (std::is_same_v<T, OperacionNativa>) {
            exigir(bool(op.codificar), "Native operation has no encoder");
            op.codificar(cmd);
        } else {
            exigir(op.destino && op.destino->dispositivo == i.dispositivo && op.offset <= op.destino->bytes && op.datos.size() <= op.destino->bytes - op.offset &&
                !(op.offset & 3u) && !(op.datos.size() & 3u), "Buffer upload is out of bounds or unaligned");
            if (op.datos.empty()) return;
            temporales.push_back(i.dispositivo->createBuffer(RenderBufferDesc::UploadBuffer(op.datos.size())));
            auto* b = temporales.back().get(); exigir(bool(b), "Staging allocation failed");
            auto* cpu = b->map(); exigir(cpu, "Failed to map staging");
            std::memcpy(cpu, op.datos.data(), op.datos.size()); b->unmap();
            cmd.copyBufferRegion(op.destino->objeto->at(op.offset), b->at(0), op.datos.size());
            cmd.barriers(RenderBarrierStage::ALL, RenderBufferBarrier(op.destino->objeto.get(), RenderBufferAccess::READ));
        }
    }, operacion);
    static_cast<Deko3DCommandList&>(cmd).limitarOperaciones();
    }
    uint32_t imagen = 0;
    if (paquete.presentar) {
        const auto& t = paquete.presentar;
        region(t, 0, 0, t->descripcion.width, t->descripcion.height);
        exigir(t->descripcion.format == RenderFormat::R8G8B8A8_UNORM, "Presentation requires RGBA8");
        exigir(i.ventana->acquireTexture(nullptr, &imagen), "Could not acquire the image");
        barrera(cmd, t, RenderTextureLayout::COPY_SOURCE);
        auto* destino = i.ventana->getTexture(imagen);
        static bool superficieInformada=false;
        if(!superficieInformada){superficieInformada=true;std::fprintf(stderr,"[superficie] RGBA8 %ux%u -> swapchain %ux%u, ruta=%s\n",t->descripcion.width,t->descripcion.height,ancho(),alto(),t->descripcion.width==ancho() && t->descripcion.height==alto()?"copia":"blit");std::fflush(stderr);}
        if (t->descripcion.width == ancho() && t->descripcion.height == alto()) {
            cmd.barriers(RenderBarrierStage::ALL, RenderTextureBarrier(destino, RenderTextureLayout::COPY_DEST));
            cmd.copyTexture(destino, t->objeto.get());
        } else {
            cmd.barriers(RenderBarrierStage::ALL, RenderTextureBarrier(destino, RenderTextureLayout::COLOR_WRITE));
            const RenderTexture* attachment = destino;
            framebuffers.push_back(i.dispositivo->createFramebuffer({&attachment, 1}));
            cmd.setFramebuffer(framebuffers.back().get());
            cmd.setScissors(RenderRect(0, 0, ancho(), alto())); cmd.clearColor(0, RenderColor(0, 0, 0, 1));
            cmd.barriers(RenderBarrierStage::ALL, RenderTextureBarrier(destino, RenderTextureLayout::COPY_DEST));
            const uint64_t sw = t->descripcion.width, sh = t->descripcion.height;
            const bool ajustarAncho = uint64_t(ancho()) * sh <= uint64_t(alto()) * sw;
            const uint32_t dw = std::max(1u, ajustarAncho ? ancho() : uint32_t(uint64_t(alto()) * sw / sh));
            const uint32_t dh = std::max(1u, ajustarAncho ? uint32_t(uint64_t(ancho()) * sh / sw) : alto());
            auto& n = static_cast<Deko3DCommandList&>(cmd); n.flushBarriers();
            auto sv = static_cast<Deko3DTexture*>(t->objeto.get())->defaultView();
            auto dv = static_cast<Deko3DTexture*>(destino)->defaultView();
            const DkImageRect sr{0, 0, 0, uint32_t(sw), uint32_t(sh), 1};
            const DkImageRect dr{(ancho() - dw) / 2, (alto() - dh) / 2, 0, dw, dh, 1};
            dkCmdBufBlitImage(n.cmd, &sv, &sr, &dv, &dr, DkBlitFlag_FilterLinear, 0);
        }
        cmd.barriers(RenderBarrierStage::ALL, RenderTextureBarrier(destino, RenderTextureLayout::PRESENT));
    }
    lmnx::medidas::ejecutarNs.fetch_add(nsDesdeMedida(t0Ejecutar),std::memory_order_relaxed);
    const auto t0Enviar=std::chrono::steady_clock::now();
    if (marca) dkCmdBufReportCounter(static_cast<Deko3DCommandList&>(cmd).cmd, DkCounter_Timestamp, marca + DkGpuAddr(1 + ranura.etiquetas.size()) * 16);
    g_marcaRanura = nullptr;
    cmd.end(); i.cola->executeCommandLists(&cmd, ranura.valla.get()); ranura.pendiente = true; ranura.retenido = paquete;
    ranura.secuencia = ++i.enviados; ++i.enviosMedida; ++i.enviosPanel; i.anotarEnvioGpu(ranura);
    if (!lecturas.empty()) {  // Readbacks need the result immediately.
        { const auto t0 = std::chrono::steady_clock::now();
          i.cola->waitForCommandFence(ranura.valla.get());
          const uint64_t espera = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
          i.esperaMedidaNs += espera; i.esperaPanelNs += espera; }
        ranura.pendiente = false;
        i.leerMarcas(ranura);  // Count this slot's timestamps too; release resources when the slot is reused.
        i.terminados = std::max(i.terminados, ranura.secuencia);
    }
    lmnx::medidas::enviarNs.fetch_add(nsDesdeMedida(t0Enviar),std::memory_order_relaxed);
    if (paquete.presentar) {
        const auto t0Presentar=std::chrono::steady_clock::now();
        exigir(i.ventana->present(imagen, nullptr, 0), "Could not present");
        i.presentadoGpu();
        lmnx::medidas::presentarNs.fetch_add(nsDesdeMedida(t0Presentar),std::memory_order_relaxed);
        if (lmnx::perfil::activo.load(std::memory_order_relaxed)) i.publicarPerfil();
        else i.panelUltimo = {};  // al volver a encenderlo, empieza de cero
#ifdef __SWITCH__
        // Per-presentation heartbeat and once-per-second FPS for the overlay (platforms/switch/switch_saltynx.cpp). Weak
        // symbols because the graphics test omits it.
        if(lmnx::saltynx::Latir)lmnx::saltynx::Latir(ancho(),alto());
        {static auto segundo=std::chrono::steady_clock::now();static uint64_t enSegundo=0,total=0;++enSegundo;++total;
         const double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-segundo).count();
         if(s>=1.0){if(lmnx::saltynx::Actualizar)lmnx::saltynx::Actualizar(enSegundo/s,enSegundo/s,ancho(),alto(),total);segundo=std::chrono::steady_clock::now();enSegundo=0;}}
#endif
        static auto inicio=std::chrono::steady_clock::now();static uint64_t cuadros=0;
        ++cuadros;const auto ahora=std::chrono::steady_clock::now();const double segundos=std::chrono::duration<double>(ahora-inicio).count();
        if(segundos>=5){std::printf("[rendimiento] %.2f presentations/s (%llu in %.2f s)\n",cuadros/segundos,(unsigned long long)cuadros,segundos);std::fflush(stdout);inicio=ahora;
            {using namespace lmnx::medidas;const double c=double(cuadros?cuadros:1);
             // Reset with exchange so printed values and panel baseline adjustments match exactly what was removed. load()
             // followed by zeroing would lose concurrent additions.
             const uint64_t vGrabar=grabarNs.exchange(0),vEjecutar=ejecutarNs.exchange(0),vEnviar=enviarNs.exchange(0),
                 vPresentar=presentarNs.exchange(0),vDibujos=dibujosGrabados.exchange(0);
             std::fprintf(stderr,"[cpu por fotograma] draws=%.0f record=%.2f ms execute=%.2f ms submit=%.2f ms present=%.2f ms (us per draw: record %.1f, execute %.1f)\n",
                 vDibujos/c,vGrabar/c/1e6,vEjecutar/c/1e6,vEnviar/c/1e6,vPresentar/c/1e6,
                 vDibujos?vGrabar/1e3/vDibujos:0.,vDibujos?vEjecutar/1e3/vDibujos:0.);
             const uint64_t vLotes=lotes.exchange(0),vPipelines=cambiosPipeline.exchange(0),vTexturas=enlacesTexturas.exchange(0),
                 vUbo=enlacesUbo.exchange(0);
             std::fprintf(stderr,"[estado por fotograma] batches=%.1f pipeline changes=%.0f texture bindings=%.0f uniform bindings=%.0f\n",vLotes/c,vPipelines/c,vTexturas/c,vUbo/c);
             std::fprintf(stderr,"[gpu por fotograma] submissions=%.1f GPU wait=%.2f ms busy=%.2f ms invalidations: descriptors=%.0f shaders=%.0f\n",i.enviosMedida/c,i.esperaMedidaNs/c/1e6,i.gpuOcupadaNs/c/1e6,
                 ::plume::g_invalidacionesDescriptores.exchange(0)/c,::plume::g_invalidacionesShader.exchange(0)/c);
             i.enviosMedida=0;i.esperaMedidaNs=0;i.gpuOcupadaNs=0;
             {std::vector<std::pair<uint64_t,std::string>> orden;for(const auto& [e,v]:i.porEtiqueta)orden.push_back({v.first,e+" (x"+std::to_string(v.second)+")"});
              std::sort(orden.begin(),orden.end(),std::greater<>());
              std::fprintf(stderr,"[gpu por pasada] ms per frame:");
              for(size_t k=0;k<std::min<size_t>(orden.size(),12);++k)std::fprintf(stderr," %.2f %s;",orden[k].first/c/1e6,orden[k].second.c_str());
              std::fputc('\n',stderr);i.porEtiqueta.clear();}
             // The panel reads differences; subtract reset measurements from its baseline so they are not lost.
             i.panelPrevio[0]-=int64_t(vGrabar);i.panelPrevio[1]-=int64_t(vEjecutar);
             i.panelPrevio[2]-=int64_t(vEnviar);i.panelPrevio[3]-=int64_t(vPresentar);
             i.panelPrevio[4]-=int64_t(vDibujos);i.panelPrevio[5]-=int64_t(vLotes);i.panelPrevio[6]-=int64_t(vPipelines);
             i.panelPrevio[7]-=int64_t(vTexturas);i.panelPrevio[8]-=int64_t(vUbo);}
            cuadros=0;
            auto& st=GetDeko3DStats();
            std::fprintf(stderr,"[gpu acumulado] new tables=%llu reused=%llu redundant readbacks skipped=%llu segments=%llu fragment barriers=%llu full=%llu\n",
                (unsigned long long)st.tablasNuevas.load(),(unsigned long long)st.tablasReusadas.load(),
                (unsigned long long)st.lecturasOmitidas.load(),(unsigned long long)st.segmentosEnviados.load(),
                (unsigned long long)st.barriersFragments.load(),(unsigned long long)st.barriersFull.load());
            std::fprintf(stderr,"[memoria LMNX] vivos=%llu KiB pico=%llu KiB acumulados=%llu KiB (cola SDK aparte)\n",
                (unsigned long long)(st.bloquesVivosBytes.load()>>10),(unsigned long long)(st.bloquesPicoBytes.load()>>10),
                (unsigned long long)(st.bloquesAcumuladosBytes.load()>>10));std::fflush(stderr);
        }
    }
    for (auto& l : lecturas) {
        const auto* cpu = static_cast<const uint8_t*>(l.buffer->map()); exigir(cpu, "Failed to map readback");
        armDCacheFlush(const_cast<uint8_t*>(cpu), l.bytes);  // Cached memory: discard stale lines before reading.
        try {
            if (l.peticion->completar) {
                if (l.peticion->formato == FormatoLectura::Lineal) l.peticion->completar({cpu, l.bytes});
                else {
                    auto gx = empaquetarEFB({cpu, l.bytes}, l.peticion->ancho, l.peticion->alto, l.peticion->formato);
                    l.peticion->completar(gx);
                }
            }
        }
        catch (...) { l.buffer->unmap(); throw; }
        l.buffer->unmap();
    }
}
}
