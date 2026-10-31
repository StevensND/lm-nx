// Console profiling without a debugger, adapted from NFSMW-NX switch_perf.cpp. Every ten seconds log exact
// thread CPU time using svcGetInfo InfoType_ThreadTickCount and entry addresses as image+0x..., resolved with
// addr2line and the matching ELF. An empty sdmc:/switch/LM-NX/perfil_pilas.flag enables 1 kHz stack sampling: a
// high-priority thread pauses busy threads with svcSetThreadActivity, reads pc, lr and the x29 chain through
// svcGetThreadContext3, then resumes them. Frequent stacks go to logs/perfil.log every ten seconds. Wrap libnx
// threadCreate/threadClose (--wrap in tools/audit_switch_link.py) to track all threads, including pthreads.
// While a thread is paused, never acquire locks, allocate memory or call stdio; use only system calls and writes
// to preallocated memory.
#include <switch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" char __start__[];

extern "C" {
Result __real_threadCreate(Thread* t, ThreadFunc entry, void* arg, void* stack_mem, size_t stack_sz, int prio, int cpuid);
Result __real_threadClose(Thread* t);
void lmnx_perfil_nombrar_hilo(const char* nombre);  // Defined below.
}

namespace {

constexpr size_t kMaxHilos = 64;
constexpr size_t kProfundidad = 8;  // pc, lr y 6 marcos de x29

struct Hilo {
    std::atomic<Handle> handle{INVALID_HANDLE};
    uintptr_t entrada = 0;
    uint64_t ticksPrevios = 0;
    // Performance panel Thread CPU use, as in banjo-nx: thread name from lmnx_perfil_nombrar_hilo and previous-query
    // ticks, total and per core.
    char nombre[40] = {};
    uint64_t usoTicks = 0;
    uint64_t nucleoTicks[4] = {};
    uint32_t generacion = 0;  // Changes on registration to keep samples from an earlier occupant separate.
};
std::array<Hilo, kMaxHilos> g_hilos;
std::atomic<uint32_t> g_numHilos{0};
// One registry lock covers registration, removal, names, panel queries and sampling. The sampler holds it across
// pause/resume, so a paused thread cannot hold it. Acquire no other locks while paused. The panel tries without
// waiting, as in banjo-nx.
std::mutex g_registro;

struct Muestra {
    uint16_t hilo = 0;
    uint16_t profundidad = 0;
    uint32_t generacion = 0;
    uintptr_t marcos[kProfundidad]{};
};
// Sampler-written ring; no malloc while a thread is paused.
constexpr size_t kMaxMuestras = 16384;
std::array<Muestra, kMaxMuestras> g_muestras;
size_t g_numMuestras = 0;
size_t g_muestrasPerdidas = 0;      // The ring filled before the report.
size_t g_reanudacionesFallidas = 0;  // svcSetThreadActivity(Runnable) returned an error.

uintptr_t relativa(uintptr_t a) {
    const auto base = reinterpret_cast<uintptr_t>(__start__);
    return a >= base ? a - base : a;
}

// Hold g_registro. CPU baselines start from ticks already consumed; the main thread registers late.
Hilo* registrarConCerrojo(Handle h, uintptr_t entrada) {
    for (auto& hilo : g_hilos) {
        if (hilo.handle.load() != INVALID_HANDLE) continue;
        hilo.entrada = entrada;
        hilo.nombre[0] = '\0';
        uint64_t ticks = 0;
        if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount, h, TickCountInfo_Total))) ticks = 0;
        hilo.ticksPrevios = ticks;
        hilo.usoTicks = ticks;
        for (uint32_t c = 0; c < 4; ++c)
            if (R_FAILED(svcGetInfo(&hilo.nucleoTicks[c], InfoType_ThreadTickCount, h, c))) hilo.nucleoTicks[c] = 0;
        ++hilo.generacion;
        hilo.handle.store(h);
        g_numHilos.fetch_add(1);
        return &hilo;
    }
    return nullptr;
}

void registrar(Handle h, uintptr_t entrada) {
    std::lock_guard bloqueo(g_registro);
    registrarConCerrojo(h, entrada);
}

void olvidar(Handle h) {
    std::lock_guard bloqueo(g_registro);
    for (auto& hilo : g_hilos) {
        if (hilo.handle.load() == h) {
            hilo.handle.store(INVALID_HANDLE);
            g_numHilos.fetch_sub(1);
            return;
        }
    }
}

bool muestrearPilas() {
    static const bool activo = [] {
        FILE* f = std::fopen("sdmc:/switch/LM-NX/perfil_pilas.flag", "rb");
        if (f) std::fclose(f);
        return f != nullptr;
    }();
    return activo;
}

// Pause the thread, copy its stack and resume it. No locks or allocations between pause and resume.
void muestrear(size_t indice, Handle h) {
    if (g_numMuestras >= kMaxMuestras) {
        ++g_muestrasPerdidas;
        return;
    }
    if (R_FAILED(svcSetThreadActivity(h, ThreadActivity_Paused))) return;  // Already suspended by another caller; leave untouched.
    ThreadContext ctx;
    if (R_SUCCEEDED(svcGetThreadContext3(&ctx, h))) {
        Muestra& m = g_muestras[g_numMuestras];
        m.hilo = uint16_t(indice);
        m.generacion = g_hilos[indice].generacion;
        m.marcos[0] = ctx.pc.x;
        m.marcos[1] = ctx.lr;
        size_t n = 2;
        uintptr_t fp = ctx.fp;
        const uintptr_t sp = ctx.sp;
        // Only increasing, readable frames inside the thread stack (sp through sp + 1 MiB). Check the region before
        // reading both values so a corrupt frame cannot fault while paused. svcQueryMemory uses no locks or allocations.
        uintptr_t regionFin = 0;
        while (n < kProfundidad && fp >= sp && fp < sp + 0x100000 && !(fp & 15)) {
            if (fp + 2 * sizeof(uintptr_t) > regionFin) {
                MemoryInfo info{};
                u32 pagina = 0;
                if (R_FAILED(svcQueryMemory(&info, &pagina, fp)) || !(info.perm & Perm_R) ||
                    fp + 2 * sizeof(uintptr_t) > info.addr + info.size)
                    break;
                regionFin = info.addr + info.size;
            }
            const auto* marco = reinterpret_cast<const uintptr_t*>(fp);
            const uintptr_t siguiente = marco[0], retorno = marco[1];
            if (!retorno) break;
            m.marcos[n++] = retorno;
            if (siguiente <= fp) break;
            fp = siguiente;
        }
        m.profundidad = uint16_t(n);
        ++g_numMuestras;
    }
    if (R_FAILED(svcSetThreadActivity(h, ThreadActivity_Runnable))) ++g_reanudacionesFallidas;
}

void informe(FILE* f, double segundos) {
    std::fprintf(f, "\n==== perfil (%.1f s) ====\n", segundos);
    std::fprintf(f, "CPU per thread (100 %% = one full core):\n");
    std::array<double, kMaxHilos> porcentaje{};
    for (size_t i = 0; i < g_hilos.size(); ++i) {
        const Handle h = g_hilos[i].handle.load();
        if (h == INVALID_HANDLE) continue;
        u64 ticks = 0;
        if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount, h, UINT64_MAX))) continue;
        const u64 delta = g_hilos[i].ticksPrevios ? ticks - g_hilos[i].ticksPrevios : 0;
        g_hilos[i].ticksPrevios = ticks;
        porcentaje[i] = double(armTicksToNs(delta)) / (segundos * 1e7);
        s32 prioridad = 0;
        svcGetThreadPriority(&prioridad, h);
        std::fprintf(f, "  hilo %2zu  handle 0x%08X  entrada imagen+0x%lx  prio 0x%02X  CPU %5.1f %%\n", i, unsigned(h),
                     (unsigned long)relativa(g_hilos[i].entrada), unsigned(prioridad), porcentaje[i]);
    }
    if (!muestrearPilas()) {
        std::fprintf(f, "Stack sampling disabled (create sdmc:/switch/LM-NX/perfil_pilas.flag to enable)\n");
        return;
    }
    std::fprintf(f, "Stack sampling active (1 ms), %zu samples (%zu lost to full ring, %zu resumes "
                    "failed). Most frequent stacks per thread (image+0x...):\n",
                 g_numMuestras, g_muestrasPerdidas, g_reanudacionesFallidas);
    g_muestrasPerdidas = 0;
    for (size_t i = 0; i < g_hilos.size(); ++i) {
        size_t total = 0;
        std::unordered_map<uint64_t, std::pair<size_t, size_t>> cuenta;  // Stack hash to (count, example index).
        for (size_t k = 0; k < g_numMuestras; ++k) {
            const auto& m = g_muestras[k];
            if (m.hilo != i || m.generacion != g_hilos[i].generacion) continue;
            ++total;
            uint64_t hash = 1469598103934665603ull;
            for (size_t d = 0; d < std::min<size_t>(m.profundidad, 5); ++d) hash = (hash ^ m.marcos[d]) * 1099511628211ull;
            auto& c = cuenta[hash];
            if (c.first++ == 0) c.second = k;
        }
        if (total < 20) continue;
        // Frequent exact pc addresses and their lr callers can be grouped by function using the ELF, showing CPU
        // distribution even when complete stacks differ.
        {
            std::unordered_map<uintptr_t, size_t> hojas;
            for (size_t k = 0; k < g_numMuestras; ++k)
                if (g_muestras[k].hilo == i && g_muestras[k].generacion == g_hilos[i].generacion)
                    ++hojas[g_muestras[k].marcos[0]];
            std::vector<std::pair<size_t, uintptr_t>> top;
            for (const auto& [pc, n] : hojas) top.push_back({n, pc});
            std::sort(top.begin(), top.end(), [](auto a, auto b) { return a.first > b.first; });
            std::fprintf(f, " hojas hilo %zu:", i);
            for (size_t j = 0; j < std::min<size_t>(top.size(), 80); ++j)
                std::fprintf(f, " %zu@0x%lx", top[j].first, (unsigned long)relativa(top[j].second));
            std::fputc('\n', f);
        }
        std::vector<std::pair<size_t, size_t>> orden;
        for (const auto& [h, c] : cuenta) orden.push_back(c);
        std::sort(orden.begin(), orden.end(), [](auto a, auto b) { return a.first > b.first; });
        std::fprintf(f, " thread %zu (%zu samples):\n", i, total);
        for (size_t j = 0; j < std::min<size_t>(orden.size(), 12); ++j) {
            const auto& m = g_muestras[orden[j].second];
            std::fprintf(f, "  %5.1f %% ", 100.0 * double(orden[j].first) / double(total));
            for (size_t d = 0; d < m.profundidad; ++d) std::fprintf(f, " 0x%lx", (unsigned long)relativa(m.marcos[d]));
            std::fputc('\n', f);
        }
    }
    g_numMuestras = 0;
}

void bucle() {
    lmnx_perfil_nombrar_hilo("profiler");
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x20);  // Higher priority than the game to minimize pause duration.
    FILE* f = std::fopen("sdmc:/switch/LM-NX/logs/perfil.log", "w");
    if (!f) return;
    std::fprintf(f, "ProjectLM-NX perfil. Base de la imagen: %p\n", __start__);
    std::fflush(f);
    const Handle propio = threadGetCurHandle();
    u64 inicio = armGetSystemTick();
    for (;;) {
        if (muestrearPilas()) {
            {
                std::lock_guard bloqueo(g_registro);  // The handle cannot close while paused.
                for (size_t i = 0; i < g_hilos.size(); ++i) {
                    const Handle h = g_hilos[i].handle.load();
                    if (h != INVALID_HANDLE && h != propio) muestrear(i, h);
                }
            }
            svcSleepThread(1000000);  // 1 ms
        } else {
            svcSleepThread(500000000);
        }
        const double segundos = double(armTicksToNs(armGetSystemTick() - inicio)) / 1e9;
        if (segundos >= 10.0) {
            {
                // Capture thread ticks and the time reference together under the registry lock. Write and fflush to SD outside
                // it so thread registration, removal and naming can proceed.
                std::lock_guard bloqueo(g_registro);
                informe(f, segundos);
                inicio = armGetSystemTick();
            }
            std::fflush(f);
        }
    }
}

}  // namespace

extern "C" {
// Performance panel: name the calling thread (Aurora, game, audio, etc.). Register it if needed, including the
// main thread before startup.
void lmnx_perfil_nombrar_hilo(const char* nombre) {
    if (!nombre) return;
    const Handle h = threadGetCurHandle();
    std::lock_guard bloqueo(g_registro);
    Hilo* encontrado = nullptr;
    for (auto& hilo : g_hilos) {
        if (hilo.handle.load() == h) {
            encontrado = &hilo;
            break;
        }
    }
    if (!encontrado) encontrado = registrarConCerrojo(h, 0);
    if (encontrado) std::snprintf(encontrado->nombre, sizeof(encontrado->nombre), "%s", nombre);
}

// Performance panel: CPU use since the previous query, in banjo-nx AppendThreadCpuUsage format: name 12.3%, with
// per-core distribution (c0 40, c3 9) above 5% when consistent with the total. Include threads using at least
// 0.5%; query once per second.
static std::mutex g_usoCerrojo;
static double g_cargaNucleos[4] = {};  // Per-core process CPU percentage in the last lmnx_perfil_uso_cpu window.
static std::atomic<bool> g_reinicioUso{false};
static double g_cpuTotal = 0.0;  // Total thread CPU percentage, measured in units of one core, in the last window.

// Performance panel: whole-process CPU use. Horizon does not provide other threads' per-core distribution, so it
// reports zero there.
double lmnx_perfil_cpu_total() {
    std::lock_guard bloqueo(g_usoCerrojo);
    return g_cpuTotal;
}

// When the panel is enabled again, the next successful registry query resets baselines without reporting.
void lmnx_perfil_reiniciar_uso() { g_reinicioUso.store(true, std::memory_order_relaxed); }

// Performance panel: summed thread load for each core (c0-c3) in the last query.
void lmnx_perfil_carga_nucleos(double* nucleos) {
    if (!nucleos) return;
    std::lock_guard bloqueo(g_usoCerrojo);
    for (int c = 0; c < 4; ++c) nucleos[c] = g_cargaNucleos[c];
}

size_t lmnx_perfil_uso_cpu(char* salida, size_t capacidad) {
    static uint64_t inicioVentana = 0;
    if (!salida || capacidad == 0) return 0;
    salida[0] = '\0';
    // Main thread: never wait for the sampler, as in banjo-nx; keep the panel's previous text.
    std::unique_lock registro(g_registro, std::try_to_lock);
    if (!registro.owns_lock()) return 0;
    std::lock_guard bloqueo(g_usoCerrojo);
    double carga[4] = {};
    double total = 0.0;
    const uint64_t ahora = armGetSystemTick();
    const uint64_t ventana = ahora - inicioVentana;
    const bool reinicio = g_reinicioUso.exchange(false, std::memory_order_relaxed);
    const bool primera = inicioVentana == 0 || reinicio;
    inicioVentana = ahora;
    size_t largo = 0;
    auto anadir = [&](const char* texto) {
        const size_t n = std::strlen(texto);
        if (largo + n + 1 > capacidad) return;
        std::memcpy(salida + largo, texto, n + 1);
        largo += n;
    };
    for (size_t i = 0; i < g_hilos.size(); ++i) {
        Hilo& hilo = g_hilos[i];
        const Handle h = hilo.handle.load();
        if (h == INVALID_HANDLE) continue;
        uint64_t ticks = 0;
        if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount, h, TickCountInfo_Total))) continue;
        const uint64_t usado = ticks >= hilo.usoTicks ? ticks - hilo.usoTicks : 0;
        hilo.usoTicks = ticks;
        double nucleo[4]{};
        double sumaNucleos = 0.0;
        for (uint32_t c = 0; c < 4; ++c) {
            uint64_t t = 0;
            if (R_FAILED(svcGetInfo(&t, InfoType_ThreadTickCount, h, c))) t = hilo.nucleoTicks[c];
            const uint64_t paso = t >= hilo.nucleoTicks[c] ? t - hilo.nucleoTicks[c] : 0;
            nucleo[c] = ventana ? 100.0 * double(paso) / double(ventana) : 0.0;
            sumaNucleos += nucleo[c];
            hilo.nucleoTicks[c] = t;
            carga[c] += nucleo[c];
        }
        if (primera || ventana == 0) continue;
        const double porcentaje = 100.0 * double(usado) / double(ventana);
        total += porcentaje;
        if (porcentaje < 0.5) continue;
        char entrada[96];
        char nombre[48];
        if (hilo.nombre[0]) std::snprintf(nombre, sizeof(nombre), "%s", hilo.nombre);
        else std::snprintf(nombre, sizeof(nombre), "thread #%zu", i);
        std::snprintf(entrada, sizeof(entrada), "%s%s %.1f%%", largo == 0 ? "" : ", ", nombre, porcentaje);
        anadir(entrada);
        if (porcentaje >= 5.0 && std::abs(sumaNucleos - porcentaje) <= std::max(2.0, porcentaje * 0.1)) {
            char nucleos[64];
            int n = 0;
            for (uint32_t c = 0; c < 4; ++c) {
                if (nucleo[c] >= 1.0 && n >= 0 && size_t(n) < sizeof(nucleos) - 12)
                    n += std::snprintf(nucleos + n, sizeof(nucleos) - n, "%sc%u %.0f", n == 0 ? " (" : ", ", c, nucleo[c]);
            }
            if (n > 0) {
                anadir(nucleos);
                anadir(")");
            }
        }
    }
    if (!primera && ventana != 0)
    {
        for (int c = 0; c < 4; ++c) g_cargaNucleos[c] = std::min(carga[c], 100.0);
        g_cpuTotal = total;
    }
    return largo;
}

Result __wrap_threadCreate(Thread* t, ThreadFunc entry, void* arg, void* stack_mem, size_t stack_sz, int prio, int cpuid) {
    const Result r = __real_threadCreate(t, entry, arg, stack_mem, stack_sz, prio, cpuid);
    if (R_SUCCEEDED(r) && t) registrar(t->handle, reinterpret_cast<uintptr_t>(entry));
    return r;
}
Result __wrap_threadClose(Thread* t) {
    if (t) olvidar(t->handle);
    return __real_threadClose(t);
}
}

namespace lmnx::perfil {
// Call once at startup on the main thread.
void Iniciar() {
    lmnx_perfil_nombrar_hilo("main");  // Register if needed.
    new std::thread(bucle);  // Persistent, heap allocated without destruction; no detach() on Horizon.
}
}  // namespace lmnx::perfil
