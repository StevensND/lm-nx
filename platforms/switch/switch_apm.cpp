/* Request stock handheld GPU clocks through Horizon's apm API, without changing memory clocks. The GPU can use 460.8 MHz instead of 307.2 MHz. Select the low-EMC profile first: 0x92220008 pairs GPU 460.8 MHz with EMC 1331.2 MHz; 0x92220007 uses EMC 1600 MHz. Clock changes are asynchronous, so poll for 1.5 seconds instead of trusting an immediate read. Keep a watchdog for later mode changes or resume. pcv can override clkrst to enforce the active apm profile, so repeated corrections cannot fix an unsuitable profile. Never raise memory above its original clock by default; if no suitable profile exists, keep the original GPU setting. nfsmw_switch_ram_1600 explicitly permits the higher-memory profile. These are stock console profiles, not KIP or system-module changes. */

// Stock clocks as in NFSMW-NX: handheld GPU at 460.8 MHz without changing memory; leave docked clocks unchanged.

#if defined(__SWITCH__)

#include <switch.h>

#include <atomic>
#include <cstdio>

namespace {

std::atomic<int> g_pedido{0};
std::atomic<bool> g_hecho{false};
/* Optional fallback: allow a 1600 MHz RAM profile if the firmware has no high-GPU profile preserving memory clocks. Disabled by default even if that leaves the GPU at its original clock. */
std::atomic<bool> g_permitir_ram_1600{false};

/* Watchdog state: g_emc_original is the original memory clock in Hz. Zero means no profile change has occurred; the watchdog then does nothing and does not open clkrst. */
std::atomic<u32> g_emc_original{0};
std::atomic<int> g_tics_emc{0};
std::atomic<int> g_correcciones_emc{0};
std::atomic<bool> g_aviso_emc{false};
std::atomic<bool> g_rendido_emc{false};

/* Check every second for the first kTicsSeguidos ticks while the system applies the change. Then check once per kTicsEspaciados ticks to catch mode changes and resume without opening clkrst every second for the whole session. */
constexpr int kTicsSeguidos = 20;
constexpr int kTicsEspaciados = 30;
/* Limit corrections: pcv may override accepted clkrst requests to restore the active apm profile's EMC. Repeated corrections indicate an unsuitable profile; log it and stop retrying. */
constexpr int kMaxCorreccionesEmc = 3;

/* Clock changes can arrive after 250 ms, so poll every 150 ms for 1.5 seconds. Reject the profile as soon as any reading shows changed memory clocks. A suitable profile takes the full polling interval. */
constexpr u64 kSondeoPasoNs = 150000000ULL;
constexpr int kSondeoPasos = 10;  // 10 x 150 ms = 1,5 s
constexpr u32 kMargenHz = 20000000u;  // 20 MHz tolerance clearly distinguishes 1597.5 MHz from 1331.2 MHz.

/* Read hardware frequencies in Hz; return false when clkrst is unavailable. Open and close per call because this runs only a few times per session. */
bool LeerRelojes(u32& gpu_hz, u32& emc_hz) {
  gpu_hz = 0;
  emc_hz = 0;
  if (R_FAILED(clkrstInitialize())) {
    return false;
  }
  bool ok = true;
  ClkrstSession ses_gpu{};
  ClkrstSession ses_emc{};
  if (R_SUCCEEDED(clkrstOpenSession(&ses_gpu, PcvModuleId_GPU, 3))) {
    if (R_FAILED(clkrstGetClockRate(&ses_gpu, &gpu_hz))) ok = false;
    clkrstCloseSession(&ses_gpu);
  } else {
    ok = false;
  }
  if (R_SUCCEEDED(clkrstOpenSession(&ses_emc, PcvModuleId_EMC, 3))) {
    if (R_FAILED(clkrstGetClockRate(&ses_emc, &emc_hz))) ok = false;
    clkrstCloseSession(&ses_emc);
  } else {
    ok = false;
  }
  clkrstExit();
  return ok;
}

/* Poll clocks for kSondeoPasos * kSondeoPasoNs. Return true only if memory stayed unchanged throughout. Stop at the first changed reading, return false and leave the latest values in gpu/emc. */
bool MemoriaQuietaDurante(u32 emc0, u32& gpu, u32& emc) {
  for (int i = 0; i < kSondeoPasos; ++i) {
    svcSleepThread(kSondeoPasoNs);
    if (!LeerRelojes(gpu, emc)) {
      return false;
    }
    if ((emc > emc0 ? emc - emc0 : emc0 - emc) > kMargenHz) {
      return false;
    }
  }
  return true;
}

/* Set the memory module clock in Hz. Return whether the request was accepted; check the actual clock later because changes are asynchronous. */
bool FijarMemoria(u32 hz) {
  bool ok = false;
  if (R_SUCCEEDED(clkrstInitialize())) {
    ClkrstSession ses{};
    if (R_SUCCEEDED(clkrstOpenSession(&ses, PcvModuleId_EMC, 3))) {
      ok = R_SUCCEEDED(clkrstSetClockRate(&ses, hz));
      clkrstCloseSession(&ses);
    }
    clkrstExit();
  }
  return ok;
}

struct Candidata {
  int mhz;
  uint32_t config;
  int emc;  // For logging only: the Horizon table's description of this profile.
};
/* Horizon PerformanceConfiguration profiles, ordered by EMC so memory-preserving profiles are tried first. 0x00020003 is GPU 307.2 / EMC 1331.2 MHz; 0x92220007 is GPU 460.8 / EMC 1600 MHz; 0x92220008 is GPU 460.8 / EMC 1331.2 MHz. Unsupported identifiers return an error without changing clocks; verify each accepted profile by polling. */
constexpr Candidata kCandidatas[] = {
    {460, 0x92220008u, 1331},  // GPU 460.8 + EMC 1331.2 MHz: preserves memory clocks.
    {460, 0x92220007u, 1600},  // GPU 460.8 + EMC 1600 MHz: raises memory clocks.
    {384, 0x00020004u, 1331},  // GPU 384 + EMC 1331,2
    {384, 0x00010000u, 1600},  // GPU 384 + EMC 1600
};


}  // namespace

extern "C" void RexSwitchApmPedirGpuMhz(int mhz) {
  g_pedido.store(mhz, std::memory_order_relaxed);
}

extern "C" void RexSwitchApmPermitirRam1600(int permitir) {
  g_permitir_ram_1600.store(permitir != 0, std::memory_order_relaxed);
}

extern "C" void RexSwitchApmAplicar(void) {
  const int mhz = g_pedido.load(std::memory_order_relaxed);
  if (mhz <= 0 || g_hecho.exchange(true)) {
    return;
  }

  ApmPerformanceMode modo = ApmPerformanceMode_Invalid;
  if (R_FAILED(apmGetPerformanceMode(&modo))) {
    std::fprintf(stderr, "[apm] could not read performance mode; clocks unchanged\n");
    return;
  }
  if (modo != ApmPerformanceMode_Normal) {
    std::fprintf(stderr, "[apm] console is docked (mode %d): no clock request\n", int(modo));
    return;
  }

  uint32_t config_original = 0;
  const bool hay_original = R_SUCCEEDED(apmGetPerformanceConfiguration(modo, &config_original));

  u32 gpu0 = 0, emc0 = 0;
  if (!LeerRelojes(gpu0, emc0)) {
    std::fprintf(stderr,
                 "[apm] could not read actual clocks (clkrst): clocks unchanged because without "
                 "verification, preserving memory clocks cannot be guaranteed\n");
    return;
  }
  std::fprintf(stderr, "[apm] initial: config 0x%08X, GPU %.1f MHz, memory %.1f MHz\n",
               hay_original ? config_original : 0u, double(gpu0) / 1e6, double(emc0) / 1e6);

  const u32 objetivo_gpu_hz = u32(mhz) * 1000000u;

  /* Try profiles in table order, low EMC first. Accept the first that raises GPU clocks without changing memory, verified by 1.5 seconds of polling because changes are asynchronous. */
  for (const Candidata& c : kCandidatas) {
    if (c.mhz != mhz) {
      continue;
    }
    if (R_FAILED(apmSetPerformanceConfiguration(modo, c.config))) {
      continue;  // This profile is unavailable on this firmware.
    }
    u32 gpu1 = 0, emc1 = 0;
    const bool quieta_de_verdad = MemoriaQuietaDurante(emc0, gpu1, emc1);
    const bool memoria_quieta =
        quieta_de_verdad || g_permitir_ram_1600.load(std::memory_order_relaxed);
    const bool gpu_subio = gpu1 + kMargenHz >= objetivo_gpu_hz;
    if (gpu_subio && memoria_quieta) {
      if (!quieta_de_verdad) {
        std::fprintf(stderr,
                     "[apm] 0x%08X raises memory to %.1f MHz, but raising RAM was enabled: "
                     "true: accepted without memory monitoring\n",
                     c.config, double(emc1) / 1e6);
        std::fprintf(stderr, "[apm] Applied 0x%08X: GPU %.1f MHz (was %.1f), memory %.1f MHz\n",
                     c.config, double(gpu1) / 1e6, double(gpu0) / 1e6, double(emc1) / 1e6);
        return;  // Do not arm the watchdog; lowering memory would contradict the explicit request.
      }
      /* Accepted; arm the watchdog because the system may later reapply EMC after a mode change or resume. */
      g_emc_original.store(emc0, std::memory_order_relaxed);
      std::fprintf(stderr,
                   "[apm] Applied profile 0x%08X (table: GPU %d, EMC %d): GPU %.1f MHz (was "
                   "%.1f); memory remains at %.1f MHz, verified for %.1f consecutive s; "
                   "monitoring enabled\n",
                   c.config, c.mhz, c.emc, double(gpu1) / 1e6, double(gpu0) / 1e6,
                   double(emc1) / 1e6, double(kSondeoPasoNs) * kSondeoPasos / 1e9);
      return;
    }
    std::fprintf(stderr, "[apm] rejected 0x%08X: GPU %.1f MHz, memory %.1f MHz (%s)\n", c.config,
                 double(gpu1) / 1e6, double(emc1) / 1e6,
                 !gpu_subio ? "GPU did not reach requested clock" : "MEMORY CLOCK CHANGED");
  }

  /* No suitable profile. Leave the original clocks: pcv enforces the active apm profile's EMC and can undo clkrst corrections. Log the available higher-memory option without selecting it automatically. */
  if (hay_original && R_SUCCEEDED(apmSetPerformanceConfiguration(modo, config_original))) {
    u32 gpu2 = 0, emc2 = 0;
    MemoriaQuietaDurante(emc0, gpu2, emc2);
    std::fprintf(stderr,
                 "[apm] no %d MHz profile on this firmware preserves memory "
                 "at %.1f MHz; restoring original 0x%08X (GPU %.1f MHz, memory %.1f MHz).\n"
                 "[apm] To allow the higher GPU clock with RAM at 1600, set "
                 "nfsmw_switch_ram_1600 = true in nfsmw.toml\n",
                 mhz, double(emc0) / 1e6, config_original, double(gpu2) / 1e6, double(emc2) / 1e6);
  } else {
    std::fprintf(stderr, "[apm] no %d MHz profile worked and the original could not be restored; "
                         "check clocks in the overlay\n",
                 mhz);
  }
}

extern "C" void RexSwitchApmVigilar(void) {
  const u32 emc0 = g_emc_original.load(std::memory_order_relaxed);
  if (emc0 == 0 || g_rendido_emc.load(std::memory_order_relaxed)) {
    return;  // Nothing changed, or this firmware does not permit corrections.
  }

  const int tic = g_tics_emc.fetch_add(1, std::memory_order_relaxed);
  if (tic >= kTicsSeguidos && (tic % kTicsEspaciados) != 0) {
    return;
  }

  u32 gpu = 0, emc = 0;
  if (!LeerRelojes(gpu, emc) || emc <= emc0 + kMargenHz) {
    return;  // At the expected clock.
  }

  /* Memory changed after profile verification. */
  const bool bajada = FijarMemoria(emc0);
  const int n = g_correcciones_emc.fetch_add(1, std::memory_order_relaxed) + 1;
  if (!g_aviso_emc.exchange(true)) {
    std::fprintf(stderr,
                 "[apm] memory rose to %.1f MHz (t=%d s): request %.1f with clkrst "
                 "(%s)\n",
                 double(emc) / 1e6, tic, double(emc0) / 1e6, bajada ? "accepted" : "REJECTED");
  }
  if (!bajada || n >= kMaxCorreccionesEmc) {
    g_rendido_emc.store(true, std::memory_order_relaxed);
    std::fprintf(stderr,
                 "[apm] stopping memory corrections after %d attempts: this firmware "
                 "overrides them. GPU keeps the requested clock; RAM follows the system\n",
                 n);
  }
}

#else   // !__SWITCH__
extern "C" void RexSwitchApmPedirGpuMhz(int) {}
extern "C" void RexSwitchApmPermitirRam1600(int) {}
extern "C" void RexSwitchApmAplicar(void) {}
extern "C" void RexSwitchApmVigilar(void) {}
#endif  // __SWITCH__
