/* Publish FPS and resolution through SaltyNX shared memory for Status Monitor Overlay and related monitors, and read Reverse-NX mode requests. NX-FPS normally hooks nvnQueuePresentTexture, eglSwapBuffers or vkQueuePresentKHR; this native renderer bypasses those hooks. Write the packed 174-byte NxFpsSharedBlock with signature 0x465053, found by overlays in four-byte steps. Also provide the NXRT block so ReverseNX-RT can request handheld or docked mode. If SaltyNX is absent, its port is unavailable and this does nothing. References: masagrator/SaltyNX saltysd_core, Status-Monitor-Overlay source/Utils.hpp and ReverseNX-RT Overlay/include/SaltyNX.h. */

#include "switch_saltynx.h"

#if defined(__SWITCH__)

#include <switch.h>

#include <atomic>
#include <cstddef>
#include <cstring>

#include <cstdio>

/* This object has no fmt dependency; send diagnostics through stderr to the console's SD log. */

// SD bytes read for the overlay's Read Speed display. Weak zero definition until the disc reader supplies the
// count; NFSMW-NX supplied it through its I/O layer.
extern "C" __attribute__((weak)) uint64_t RexIoBytesLeidos() { return 0; }

namespace lmnx::saltynx {
namespace {

constexpr uint32_t kMagicFps = 0x465053;          // «SPF»: bloque de NX-FPS
constexpr uint32_t kMagicReverseNx = 0x5452584E;  // «NXRT» en little-endian
constexpr size_t kTamanoCompartido = 0x1000;      // SaltyNX maps one page.

/* Resolution calls in the overlay's format: width, height and count. */
struct ResolucionLlamadas {
  uint16_t ancho;
  uint16_t alto;
  uint16_t llamadas;
} __attribute__((packed));

/* SaltyNX NxFpsSharedBlock; the overlay expects 174 bytes. */
struct BloqueFps {
  uint32_t magic;
  uint8_t fps;
  float fps_media;
  bool plugin_activo;
  uint8_t fps_bloqueados;
  uint8_t modo_fps;
  uint8_t zero_sync;
  uint8_t parche_aplicado;
  uint8_t api;
  uint32_t ticks[10];
  uint8_t buffers;
  uint8_t buffers_puestos;
  uint8_t buffers_activos;
  uint8_t buffers_activos_puestos;
  uint8_t display_sync;
  ResolucionLlamadas render[8];
  ResolucionLlamadas viewport[8];
  bool forzar_refresco_original;
  bool no_forzar_60_en_base;
  bool forzar_suspension;
  uint8_t refresco_actual;
  float lectura_por_segundo;
  uint8_t fps_bloqueados_base;
  uint64_t numero_fotograma;
  int8_t buffers_esperados;
} __attribute__((packed));

static_assert(sizeof(BloqueFps) == 174, "the overlay expects 174 bytes");
// SaltyNX readSpeedPerSecond (NX-FPS-Common.hpp) is at byte 160. PerfCounters in the 310-byte version follow
// byte 174 without moving it.
static_assert(offsetof(BloqueFps, lectura_por_segundo) == 160, "Read Speed at byte 160 of the SaltyNX block");

/* struct Shared de ReverseNX-RT (9 bytes). */
struct BloqueReverseNx {
  uint32_t magic;
  bool en_base;
  bool por_defecto;
  bool plugin_activo;
  uint8_t resoluciones;
  bool uso_ddr;
} __attribute__((packed));

static_assert(sizeof(BloqueReverseNx) == 9, "ReverseNX-RT usa 9 bytes");

/* API reported in the block: 0 unknown, 1 NVN, 2 GL, 3 Vulkan. */
constexpr uint8_t kApiVulkan = 3;

/* Send diagnostics through stderr to the console's SD log. */
void Aviso(const char* texto) { std::fprintf(stderr, "%s\n", texto); }
void AvisoNum(const char* texto, long long numero) {
  std::fprintf(stderr, "%s 0x%llX (%lld)\n", texto, (unsigned long long)numero, numero);
}
void AvisoDos(const char* texto, long long a, long long b) {
  std::fprintf(stderr, "%s %lld / %lld\n", texto, a, b);
}

SharedMemory g_memoria{};
bool g_mapeada = false;
uint8_t* g_base = nullptr;   // Start of shared memory, found through IPC or scanning.
size_t g_bytes = 0;
/* Written by the profiling thread (Iniciar/Actualizar) and read on every presentation (Latir) by the render thread; must be atomic. */
std::atomic<BloqueFps*> g_fps{nullptr};
uint64_t g_tick_previo = 0;   // Render thread only.
unsigned g_tick_pos = 0;      // idem
uint64_t g_fotogramas = 0;    // idem
std::atomic<BloqueReverseNx*> g_reverse{nullptr};
std::atomic<bool> g_reverse_en_base{false};
std::atomic<bool> g_reverse_activo{false};
/* Enabled by the game through nfsmw_switch_saltynx; on by default. */
std::atomic<int> g_habilitado{1};
/* Remaining connection attempts. The system module may start later, so retry once per second during the first minute. */
int g_intentos = 0;  // Number of attempts already made.

/* Last published values, owned by the profiling thread (Actualizar/Iniciar). Seed a late shared block immediately, whether delayed by overlay startup, memory availability or allocation, so FPS and resolution appear even during loading without presentations. */
uint8_t g_ultimo_fps = 0;
float g_ultima_media = 0.0f;
uint16_t g_ultimo_ancho = 0;
uint16_t g_ultimo_alto = 0;
uint64_t g_ultimos_fotogramas = 0;
// Read Speed, updated once per second by the profiling thread through Actualizar.
uint64_t g_lectura_bytes_previos = 0;
uint64_t g_lectura_tick_previo = 0;
float g_lectura_por_segundo = 0.0f;

/* First-attempt tick. The profiling thread calls Iniciar() at startup without waiting for kStartDelayNs, providing the time reference for the success message. */
uint64_t g_tick_primer_intento = 0;
/* Log each notice once to avoid repeating it on every retry. */
bool g_avisado_publicado = false;
bool g_avisado_conexion = false;
bool g_avisado_bloque = false;
bool g_avisado_sin_sitio = false;
bool g_avisado_reverse = false;
bool g_avisado_sin_memoria = false;

/* SaltySD IPC: modern libnx lacks the old ipc.h API, but the service uses standard CMIF. serviceDispatch supplies the same SFCI header and body fields. */

Result ReservarMemoria(Service* s, uint64_t tamano, uint64_t* desplazamiento) {
  return serviceDispatchInOut(s, 6, tamano, *desplazamiento, .in_send_pid = true);
}

Result PedirManejador(Service* s, Handle* salida) {
  return serviceDispatch(s, 7, .in_send_pid = true,
                         .out_handle_attrs = {SfOutHandleAttr_HipcCopy}, .out_handles = salida);
}

Result Terminar(Service* s) {
  const uint64_t cero = 0;
  return serviceDispatchIn(s, 0, cero, .in_send_pid = true);
}

/* Find a signature in four-byte steps through the page, as the overlay does. */
void* BuscarMarcaEn(uint8_t* base, size_t bytes, uint32_t marca) {
  if (!base) {
    return nullptr;
  }
  for (size_t offset = 0; offset + sizeof(uint32_t) <= bytes; offset += 4) {
    uint32_t leido = 0;
    std::memcpy(&leido, base + offset, sizeof(leido));
    if (leido == marca) {
      return base + offset;
    }
  }
  return nullptr;
}

void* BuscarMarca(uint32_t marca) { return BuscarMarcaEn(g_base, g_bytes, marca); }

/* A block stays valid only while its signature matches. Multiple clients share the SaltySD page and allocation state persists across launches. Validate the four-byte signature on each use. */
bool BloqueValido(const BloqueFps* bloque) { return bloque != nullptr && bloque->magic == kMagicFps; }

/* Immediately seed heartbeat, API, FPS and resolution. The overlay needs populated fields to show both rows, so seed the block as soon as available without requiring a restart. */
void SembrarBloque(BloqueFps* bloque) {
  bloque->plugin_activo = true;  // The overlay clears this to check that the game is alive.
  bloque->api = kApiVulkan;
  bloque->fps = g_ultimo_fps;
  bloque->fps_media = g_ultima_media;
  bloque->numero_fotograma = g_ultimos_fotogramas;
  if (g_ultimo_ancho && g_ultimo_alto) {
    // calls must not equal 0xFFFF; the overlay uses that sentinel to request resolution.
    const uint16_t cuantas = g_ultimo_fps ? uint16_t(g_ultimo_fps) : uint16_t(1);
    const ResolucionLlamadas r = {g_ultimo_ancho, g_ultimo_alto, cuantas};
    bloque->render[0] = r;
    bloque->viewport[0] = r;
  }
  // The overlay averages ticks[], not fps_media; zeros would display infinity. Seed a new block from the measured
  // rate, then let Latir replace it with real intervals after two presentations.
  if (g_ultimo_fps != 0 && bloque->ticks[0] == 0) {
    // Use indexing, not references: the packed block puts ticks[] at an odd offset, so taking an element address
    // would yield an unaligned pointer (-Waddress-of-packed-member).
    const uint32_t por_fotograma = uint32_t(armGetSystemTickFreq() / g_ultimo_fps);
    for (unsigned i = 0; i < 10; ++i) {
      bloque->ticks[i] = por_fotograma;
    }
  }
}

/* Log publication success once with seconds since startup to help diagnose delayed overlay data. */
void AvisarPublicado(const BloqueFps* bloque) {
  if (g_avisado_publicado) {
    return;
  }
  g_avisado_publicado = true;
  const uint64_t freq = armGetSystemTickFreq();
  const double segundos =
      freq ? double(armGetSystemTick() - g_tick_primer_intento) / double(freq) : 0.0;
  const unsigned desplazamiento =
      g_base ? unsigned(reinterpret_cast<const uint8_t*>(bloque) - static_cast<const uint8_t*>(g_base)) : 0u;
  std::fprintf(stderr,
               "[saltynx] Published %.2f s after startup: FPS=%u and RES=%ux%u at offset 0x%X\n",
               segundos, unsigned(g_ultimo_fps), unsigned(g_ultimo_ancho), unsigned(g_ultimo_alto),
               desplazamiento);
}

/* Search the already mapped page for both signatures: only 1,024 four-byte comparisons and no port session. Repeat every second while a block is missing. Return true if the FPS block exists. */
bool Enganchar() {
  auto* bloque = static_cast<BloqueFps*>(BuscarMarca(kMagicFps));
  if (bloque) {
    g_fps.store(bloque, std::memory_order_release);
    SembrarBloque(bloque);
    AvisarPublicado(bloque);
  }
  if (!g_reverse.load(std::memory_order_acquire)) {
    if (auto* reverse = static_cast<BloqueReverseNx*>(BuscarMarca(kMagicReverseNx))) {
      g_reverse.store(reverse, std::memory_order_release);
    }
  }
  return bloque != nullptr;
}

/* If SaltyNX injected into this process, shared memory is already mapped. Walk readable shared-memory regions with svcQueryMemory. */
bool BuscarMemoriaCompartidaPropia() {
  uint64_t direccion = 0;
  for (int regiones = 0; regiones < 4096; ++regiones) {
    MemoryInfo info{};
    u32 paginas = 0;
    if (R_FAILED(svcQueryMemory(&info, &paginas, direccion))) {
      return false;
    }
    if (info.size == 0) {
      return false;
    }
    if (info.type == MemType_SharedMem && (info.perm & Perm_R) != 0 && info.size >= kTamanoCompartido) {
      auto* base = reinterpret_cast<uint8_t*>(uintptr_t(info.addr));
      const size_t bytes = size_t(info.size) < 0x10000 ? size_t(info.size) : 0x10000;
      if (BuscarMarcaEn(base, bytes, kMagicFps) || BuscarMarcaEn(base, bytes, kMagicReverseNx)) {
        g_base = base;
        g_bytes = bytes;
        return true;
      }
    }
    const uint64_t siguiente = info.addr + info.size;
    if (siguiente <= direccion) {
      return false;
    }
    direccion = siguiente;
  }
  return false;
}

/* Connection failures can mean SaltyNX's single port session is occupied or this process has exhausted sessions (0x10801). Try sm: as a control; it supports many sessions. If it also fails, report the process's used and maximum sessions. */
void Diagnostico(Result rc_saltysd) {
  if (rc_saltysd == 0) {
    Aviso("[saltynx] not attempted: the process has no room for another port session");
  } else {
    AvisoNum("[saltynx] could not connect to SaltyNX ports. Last error", rc_saltysd);
  }

  // Report the process title ID: SaltyNX rejects IDs above 0x01FFFFFFFFFFFFFF as homebrew, a range used by
  // forwarders.
  u64 titulo = 0;
  if (R_SUCCEEDED(svcGetInfo(&titulo, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0))) {
    std::fprintf(stderr, "[saltynx] process TID: %016llX (SaltyNX accepts <= 01FFFFFFFFFFFFFF without 0x1F00)\n",
                 (unsigned long long)titulo);
  }

  // Check each port separately: absent (0xF201) differs from resource exhaustion.
  static const char* const kDosPuertos[2] = {"InjectServ", "SaltySD"};
  for (const char* p : kDosPuertos) {
    Handle h = INVALID_HANDLE;
    const Result rc = svcConnectToNamedPort(&h, p);
    if (R_SUCCEEDED(rc)) {
      svcCloseHandle(h);
      Aviso(p[0] == 'I' ? "[saltynx] InjectServ port: connected" : "[saltynx] SaltySD port: connected");
    } else {
      AvisoNum(p[0] == 'I' ? "[saltynx] InjectServ port: error" : "[saltynx] SaltySD port: error", rc);
    }
  }

  Handle control = INVALID_HANDLE;
  const Result rc_sm = svcConnectToNamedPort(&control, "sm:");
  if (R_SUCCEEDED(rc_sm)) {
    svcCloseHandle(control);
    Aviso("[saltynx] control: sm: provides a new session; the limit is on the SaltyNX port");
  } else {
    AvisoNum("[saltynx] control: sm: also failed; the process session limit was reached. Error", rc_sm);
  }

  u64 bruto = 0;
  Result rc_lim = svcGetInfo(&bruto, InfoType_ResourceLimit, INVALID_HANDLE, 0);
  if (R_FAILED(rc_lim)) {
    rc_lim = svcGetInfo(&bruto, InfoType_ResourceLimit, CUR_PROCESS_HANDLE, 0);
  }
  if (R_FAILED(rc_lim)) {
    AvisoNum("[saltynx] could not read process limits. Error", rc_lim);
    return;
  }
  const Handle limite = static_cast<Handle>(bruto);
  struct Recurso {
    const char* nombre;
    LimitableResource cual;
  };
  const Recurso kRecursos[] = {
      {"[saltynx] sessions used / limit:", LimitableResource_Sessions},
      {"[saltynx] events used / limit:", LimitableResource_Events},
      {"[saltynx] threads used / limit:", LimitableResource_Threads},
      {"[saltynx] transfer memories used / limit:", LimitableResource_TransferMemories},
  };
  for (const Recurso& r : kRecursos) {
    s64 ahora = 0;
    s64 tope = 0;
    if (R_SUCCEEDED(svcGetResourceLimitCurrentValue(&ahora, limite, r.cual)) &&
        R_SUCCEEDED(svcGetResourceLimitLimitValue(&tope, limite, r.cual))) {
      AvisoDos(r.nombre, (long long)ahora, (long long)tope);
    }
  }
  svcCloseHandle(limite);
}

/* Make room for a port session. A forwarder/hbloader process may allow only one, already used by libnx for sm:. First try raising its own resource limit; check envIsSyscallHinted before SVC 0x7E because a forbidden SVC kills the process. The new limit must not be below current usage. Otherwise temporarily close sm: with smExit, reconnect afterward, and retain the shared-memory handle after closing the port session. */

Handle AbrirLimiteDeRecursos() {
  u64 bruto = 0;
  if (R_SUCCEEDED(svcGetInfo(&bruto, InfoType_ResourceLimit, INVALID_HANDLE, 0))) {
    return static_cast<Handle>(bruto);
  }
  if (R_SUCCEEDED(svcGetInfo(&bruto, InfoType_ResourceLimit, CUR_PROCESS_HANDLE, 0))) {
    return static_cast<Handle>(bruto);
  }
  return INVALID_HANDLE;
}

bool HaySitioParaUnaSesion(Handle limite) {
  s64 ahora = 0;
  s64 tope = 0;
  if (R_FAILED(svcGetResourceLimitCurrentValue(&ahora, limite, LimitableResource_Sessions)) ||
      R_FAILED(svcGetResourceLimitLimitValue(&tope, limite, LimitableResource_Sessions))) {
    return true;  // Try anyway if the limit cannot be read.
  }
  return ahora < tope;
}

/* Set *sm_cerrado if sm: was closed and must be restored. Restrict temporary closure to early attempts to avoid repeatedly interrupting name service. Raising the limit is permanent and attempted once. */
bool HacerSitio(bool* sm_cerrado, bool permitir_soltar_sm) {
  *sm_cerrado = false;
  const Handle limite = AbrirLimiteDeRecursos();
  if (limite == INVALID_HANDLE) {
    return true;
  }
  bool sitio = HaySitioParaUnaSesion(limite);

  if (!sitio) {
    if (envIsSyscallHinted(0x7E)) {  // svcSetResourceLimitLimitValue
      s64 tope = 0;
      svcGetResourceLimitLimitValue(&tope, limite, LimitableResource_Sessions);
      const Result rc = svcSetResourceLimitLimitValue(limite, LimitableResource_Sessions,
                                                      static_cast<u64>(tope + 4));
      if (R_SUCCEEDED(rc)) {
        sitio = HaySitioParaUnaSesion(limite);
        AvisoDos("[saltynx] raised process session limit:", (long long)tope, (long long)(tope + 4));
      } else {
        AvisoNum("[saltynx] could not raise session limit. Error", rc);
      }
    } else {
      Aviso("[saltynx] the loader does not allow svcSetResourceLimitLimitValue (SVC 0x7E)");
    }
  }

  if (!sitio && permitir_soltar_sm) {
    smExit();  // libnx reference-counts this; a successful close frees a session.
    if (HaySitioParaUnaSesion(limite)) {
      *sm_cerrado = true;
      sitio = true;
      Aviso("[saltynx] temporarily closed sm: to free a session");
    } else {
      smInitialize();  // It did not close; restore the reference count and leave it unchanged.
      Aviso("[saltynx] no free session even after closing sm:");
    }
  }

  svcCloseHandle(limite);
  return sitio;
}

/* Restore sm: on exit in all cases. */
struct DevolverSm {
  bool activo = false;
  ~DevolverSm() {
    if (activo) {
      smInitialize();
    }
  }
};

}  // namespace

void Iniciar() {
  if (!g_habilitado.load(std::memory_order_relaxed)) {
    return;
  }
  if (g_tick_primer_intento == 0) {
    g_tick_primer_intento = armGetSystemTick();
  }

  /* Success requires a valid block, not just a mapped page. Check the signature and reconnect if allocation failed or another client overwrote it; a mapped page alone must not stop retries. */
  BloqueFps* ya = g_fps.load(std::memory_order_acquire);
  if (!BloqueValido(ya)) {
    g_fps.store(nullptr, std::memory_order_release);
  } else if (g_reverse.load(std::memory_order_acquire) != nullptr) {
    return;  // Both blocks are ready; no work needed.
  }

  // First rescan an already mapped page for signatures. This needs no sessions or IPC and can run every second
  // indefinitely.
  if (g_mapeada && Enganchar()) {
    return;
  }

  // Path 1: IPC. Try both SaltyNX ports, each allowing one session, with retries for delayed startup. First free a
  // session if libnx's sm: uses the process's only slot.
  DevolverSm devolver;
  // Try raising the limit immediately; it is permanent. Temporarily close sm: only from the third attempt after
  // startup opens services. After the initial retry window, try once per minute so a busy port does not require a
  // game restart.
  const bool soltar_sm = g_intentos >= 3 && (g_intentos < 15 || (g_intentos % 60) == 0);
  const bool con_sitio = HacerSitio(&devolver.activo, soltar_sm);
  // With sm: closed, make one quick pass. Retry more during the first startup attempt; two passes suffice on later
  // one-second retries because the overlay also needs the port's single session.
  const int vueltas = devolver.activo ? 1 : (g_intentos == 0 ? 20 : 2);
  bool listo = false;

  /* Try SaltySD first: it serves commands 6 and 7 for shared-memory allocation and handles. InjectServ may connect but then close with 0xF601. If a connected port provides no memory, close it and try the other in the same pass. */
  static const char* const kPuertos[2] = {"SaltySD", "InjectServ"};

  uint64_t desplazamiento = 0;
  bool reservada = false;
  uint64_t desplazamiento_nx = 0;
  bool reservada_nx = false;
  Result rc_puerto = 0;
  Result rc_manejador = 0;
  const char* nombre = nullptr;

  for (int i = 0; i < vueltas && !listo && con_sitio; ++i) {
    for (const char* candidato : kPuertos) {
      Handle puerto = INVALID_HANDLE;
      rc_puerto = svcConnectToNamedPort(&puerto, candidato);
      if (R_FAILED(rc_puerto)) {
        continue;
      }
      Service servicio{};
      servicio.session = puerto;

      // Get and map the shared-memory handle before allocating missing blocks. SaltySD resets its allocation counter
      // only during injection, which does not happen here; reusing existing blocks avoids exhausting the 4 KiB page
      // across launches. If already mapped, skip requesting the handle again and allocate only missing blocks.
      bool tenemos_pagina = g_mapeada;
      if (!tenemos_pagina) {
        Handle memoria = INVALID_HANDLE;
        rc_manejador = PedirManejador(&servicio, &memoria);
        if (R_SUCCEEDED(rc_manejador)) {
          shmemLoadRemote(&g_memoria, memoria, kTamanoCompartido, Perm_Rw);
          if (R_SUCCEEDED(shmemMap(&g_memoria))) {
            g_base = static_cast<uint8_t*>(shmemGetAddr(&g_memoria));
            g_bytes = kTamanoCompartido;
            g_mapeada = true;
            tenemos_pagina = true;
          }
        }
      }
      if (tenemos_pagina) {
        listo = true;
        nombre = candidato;
        if (!BuscarMarca(kMagicFps)) {
          reservada = R_SUCCEEDED(ReservarMemoria(&servicio, sizeof(BloqueFps), &desplazamiento));
        }
        if (!BuscarMarca(kMagicReverseNx)) {
          reservada_nx = R_SUCCEEDED(ReservarMemoria(&servicio, sizeof(BloqueReverseNx), &desplazamiento_nx));
        }
      }

      Terminar(&servicio);  // Command 0 closes the server side. Also close this side because the port permits one session; retaining it
// would block the overlay. The shared-memory handle remains valid independently.
      svcCloseHandle(puerto);
      if (listo) {
        break;
      }
    }
    if (!listo) {
      svcSleepThread(10 * 1000 * 1000);  // 10 ms
    }
  }

  if (devolver.activo) {  // A session is free again; restore sm: immediately.
    smInitialize();
    devolver.activo = false;
  }

  if (nombre && !g_avisado_conexion) {
    g_avisado_conexion = true;  // Log once; this retries every second.
    Aviso(nombre[0] == 'S' ? "[saltynx] connected through SaltySD" : "[saltynx] connected through InjectServ");
  }
  if (!g_mapeada) {
    if (R_FAILED(rc_manejador) && !g_avisado_sin_memoria) {
      // A port connected without supplying memory; the other has also been tried. Log once because this retries
      // throughout the session.
      g_avisado_sin_memoria = true;
      AvisoNum("[saltynx] a port connected but did not provide shared memory; error", rc_manejador);
    }
    // Path 2: no free session. If SaltyNX injected into this process, its shared memory is already mapped.
    if (!BuscarMemoriaCompartidaPropia()) {
      // Retry throughout the session. 0x10801 (LimitReached) means the port exists but its session is occupied and may
      // become free later.
      ++g_intentos;
      // Diagnose first on attempt five, after trying to close sm:, since the first connection attempt happens
      // immediately at startup.
      if (g_intentos == 5 || g_intentos == 30) {
        Diagnostico(rc_puerto);
      } else if (g_intentos % 600 == 0) {
        AvisoNum("[saltynx] connection retries still failing. Error", rc_puerto);
      }
      return;
    }
    g_mapeada = true;
    Aviso("[saltynx] no port session, but shared memory was already mapped");
  }

  // Reuse an existing FPS block, whose fields may be zero because SaltyNX has no presentation hook here; otherwise
  // use the IPC allocation.
  auto* existente = static_cast<BloqueFps*>(BuscarMarca(kMagicFps));
  if (existente) {
    g_fps.store(existente, std::memory_order_release);
    if (!g_avisado_bloque) {
      g_avisado_bloque = true;
      Aviso("[saltynx] bloque de FPS ya presente: se escribe encima");
    }
  } else if (reservada) {
    auto* bloque = reinterpret_cast<BloqueFps*>(g_base + desplazamiento);
    std::memset(bloque, 0, sizeof(*bloque));
    bloque->magic = kMagicFps;
    g_fps.store(bloque, std::memory_order_release);
    AvisoNum("[saltynx] FPS block created at offset", (long long)desplazamiento);
  } else if (!g_avisado_sin_sitio) {
    g_avisado_sin_sitio = true;
    Aviso("[saltynx] no room for the FPS block; retrying once per second");
  }
  if (BloqueFps* bloque = g_fps.load(std::memory_order_acquire)) {
    // Publish FPS and resolution together without waiting for the next second or presentation.
    SembrarBloque(bloque);
    AvisarPublicado(bloque);
    g_intentos = 0;
  } else {
    // Mapped without a block: keep advancing the attempt count so the next tick can request space and a session
    // again.
    ++g_intentos;
  }

  /* Provide a Reverse-NX block because its injected plugin does not run here. The overlay finds NXRT without a handshake and can then request handheld/docked mode. pluginActive means the game queried the mode; ModoBase sets it so controls appear. */
  auto* reverse = static_cast<BloqueReverseNx*>(BuscarMarca(kMagicReverseNx));
  if (!reverse && reservada_nx) {
    reverse = reinterpret_cast<BloqueReverseNx*>(g_base + desplazamiento_nx);
    std::memset(reverse, 0, sizeof(*reverse));
    reverse->magic = kMagicReverseNx;
    reverse->por_defecto = true;  // Follow the system until the user requests another mode.
    AvisoNum("[saltynx] Reverse-NX block created at offset", (long long)desplazamiento_nx);
  } else if (!g_avisado_reverse) {
    g_avisado_reverse = true;  // Log once; this retries every second.
    Aviso(reverse ? "[saltynx] Reverse-NX block already present" : "[saltynx] no room for the Reverse-NX block");
  }
  // Store only a valid block; a failed retry must not overwrite a good pointer with nullptr.
  if (reverse) {
    g_reverse.store(reverse, std::memory_order_release);
  }
}

void Actualizar(double fps_segundo, double fps_media, uint32_t ancho, uint32_t alto, uint64_t fotogramas) {
  // Save values before publishing so a late block can be seeded immediately when available.
  const double tope = fps_segundo < 0.0 ? 0.0 : (fps_segundo > 255.0 ? 255.0 : fps_segundo);
  g_ultimo_fps = uint8_t(tope + 0.5);
  g_ultima_media = float(fps_media);
  if (ancho && alto) {
    g_ultimo_ancho = uint16_t(ancho);
    g_ultimo_alto = uint16_t(alto);
  }
  g_ultimos_fotogramas = fotogramas;

  /* Read Speed: this NRO reads through libnx rather than the nn::fs::detail::FileAccessor::Read hook. Match NX-FPS-Common.hpp by dividing bytes read since the last query by elapsed seconds. readSpeedPerSecond is a float in bytes/second; the overlay converts to MiB/s. Write 1 when speed falls to zero after a nonzero value. */
  {
    const uint64_t bytes = RexIoBytesLeidos();
    const uint64_t tick = armGetSystemTick();
    if (g_lectura_tick_previo != 0 && tick > g_lectura_tick_previo) {
      const double segundos = double(armTicksToNs(tick - g_lectura_tick_previo)) / 1e9;
      float velocidad = float(double(bytes - g_lectura_bytes_previos) / segundos);
      if (velocidad == 0.0f && g_lectura_por_segundo != 0.0f) {
        velocidad = 1.0f;
      }
      g_lectura_por_segundo = velocidad;
    }
    g_lectura_bytes_previos = bytes;
    g_lectura_tick_previo = tick;
  }

  /* Check the block signature each tick. It may be missing, unallocated or overwritten by another shared-page client. Reconnect when needed so the overlay can open at any time without restarting the game. */
  BloqueFps* bloque = g_fps.load(std::memory_order_acquire);
  // Check the enable switch here too because profiling starts before game settings are applied. Disabling releases
  // the block and stops Latir writes; reenabling recovers it on the next tick.
  if (!g_habilitado.load(std::memory_order_relaxed)) {
    if (bloque) {
      bloque->plugin_activo = false;
      g_fps.store(nullptr, std::memory_order_release);
    }
    return;
  }
  if (!BloqueValido(bloque)) {
    Iniciar();
    bloque = g_fps.load(std::memory_order_acquire);
  }
  if (bloque) {
    // Always publish FPS and resolution together. Latir runs only during presentation, so relying on it for
    // resolution would leave the overlay's RES row empty during loading.
    SembrarBloque(bloque);
    bloque->lectura_por_segundo = g_lectura_por_segundo;  // Read Speed.
  }
  if (BloqueReverseNx* reverse = g_reverse.load(std::memory_order_acquire)) {
    g_reverse_activo.store(!reverse->por_defecto, std::memory_order_relaxed);
    g_reverse_en_base.store(reverse->en_base, std::memory_order_relaxed);
  }
}

/* Per-presentation heartbeat from the render thread. The overlay clears pluginActive and waits 100 ms for it to return; it also writes renderCalls[0].calls = 0xFFFF and waits for a replacement. Once-per-second updates are too slow. Fill FPSticks[10] with actual presentation intervals because the overlay computes average FPS from ticks and zeros would yield infinity. */
void Latir(uint32_t ancho, uint32_t alto) {
  BloqueFps* bloque = g_fps.load(std::memory_order_acquire);
  // If the signature disappeared, discard the pointer and let the profiling thread's one-second tick recover it.
  // This per-frame render-thread path performs no IPC or scanning.
  if (!BloqueValido(bloque)) {
    if (bloque) {
      g_fps.store(nullptr, std::memory_order_release);
    }
    return;
  }
  bloque->plugin_activo = true;
  bloque->api = kApiVulkan;

  // When a new block appears, discard the first interval; the previous timestamp may be minutes old and would
  // distort average FPS.
  static const BloqueFps* ultimo_visto = nullptr;  // Render thread only.
  if (ultimo_visto != bloque) {
    ultimo_visto = bloque;
    g_tick_previo = 0;
  }

  const uint64_t ahora = armGetSystemTick();
  if (g_tick_previo != 0) {
    const uint64_t salto = ahora - g_tick_previo;
    bloque->ticks[g_tick_pos] = uint32_t(salto > 0xFFFFFFFFull ? 0xFFFFFFFFull : salto);
    g_tick_pos = (g_tick_pos + 1) % 10;
    ++g_fotogramas;
    bloque->numero_fotograma = g_fotogramas;
  }
  g_tick_previo = ahora;

  if (ancho && alto) {
    // calls cannot equal the overlay's 0xFFFF sentinel. Use the last second's frame count, as NX-FPS does.
    const uint16_t cuantas = bloque->fps ? bloque->fps : uint16_t(1);
    const ResolucionLlamadas r = {uint16_t(ancho), uint16_t(alto), cuantas};
    bloque->render[0] = r;
    bloque->viewport[0] = r;
  }
}

void Habilitar(bool habilitado) { g_habilitado.store(habilitado ? 1 : 0, std::memory_order_relaxed); }

bool ModoBase(bool real) {
  BloqueReverseNx* reverse = g_reverse.load(std::memory_order_acquire);
  if (!reverse) {
    return real;
  }
  // Reverse-NX pluginActive means the game queried the mode; without it the overlay hides its controls.
  reverse->plugin_activo = true;
  if (reverse->por_defecto) {
    reverse->en_base = real;  // System controlled: reflect the actual mode in the overlay.
    return real;
  }
  return reverse->en_base;
}

EstadoReverseNx EstadoReverse() {
  const BloqueReverseNx* reverse = g_reverse.load(std::memory_order_acquire);
  if (!reverse) {
    return {false, false, false, false};
  }
  return {true, reverse->en_base, reverse->por_defecto, reverse->plugin_activo};
}

}  // namespace lmnx::saltynx

/* Game enable switch, set before the profiling thread starts. */
extern "C" void RexSwitchSaltyNxHabilitar(int habilitado) {
  lmnx::saltynx::Habilitar(habilitado != 0);
}

#endif  // defined(__SWITCH__)
