// Fourth Switch core (issues StevensND/nfsmw-nx#23 and PalindromicBreadLoaf/ARMSX2-NX#12). Normal homebrew
// receives cores 0-2 (mask 0x7); core 3 belongs to the system. A modified hbl.nsp with NPDM highest_cpu_id = 3,
// launched through title takeover, can grant mask 0xF. Move only one heavy thread to core 3; runtime.cpp,
// thread_policy.c and pipeline preloading keep others on 0-2. Aurora's GX FIFO thread moves only with
// fifo_nucleo3.flag because measurements showed no FPS gain by default. Without mask 0xF nothing changes.
// Results go to logs/ProjectLM-NX.log under the existing log tag.
#include <switch.h>

#include "perfil_hilos.h"

#include <cstdio>
#include <cstring>
#include <mutex>

extern "C" void lmnx_hilo_aurora(const char* nombre) {
    static std::once_flag aviso;
    static uint64_t mascara = 0;
    std::call_once(aviso, [] {
        if (R_FAILED(svcGetInfo(&mascara, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0))) mascara = 0;
        std::printf("[núcleos] process mask 0x%llx: core 3 %s\n", (unsigned long long)mascara,
                    (mascara & 8) ? "available (four-core hbl.nsp and application mode)" : "unavailable (0-2 only)");
    });
    if (nombre) lmnx_perfil_nombrar(nombre);  // panel de rendimiento: «Thread CPU use»
    if (!nombre || std::strcmp(nombre, "Aurora FIFO processor") != 0) return;
    // Aurora's Normal FIFO priority maps to 0x3A in SDL, below the game's 0x2C and core 3 system modules. This SD
    // flag requests 0x2C for comparison without recompiling.
    if (FILE* bandera = std::fopen("sdmc:/switch/LM-NX/fifo_prioridad_2c.flag", "rb")) {
        std::fclose(bandera);
        const Result rp = svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2C);
        std::printf("[núcleos] GX FIFO: priority 0x2C (fifo_prioridad_2c.flag): 0x%x\n", rp);
        std::fflush(stdout);
    }
    if (!(mascara & 8)) return;
    // Keep the FIFO on cores 0-2 by default. Core 3 is shared with system modules and showed longer wall time and
    // more stalls without an FPS gain. This SD flag moves it to core 3 for comparison.
    FILE* bandera = std::fopen("sdmc:/switch/LM-NX/fifo_nucleo3.flag", "rb");
    if (!bandera) {
        std::printf("[núcleos] GX FIFO -> core 3: no (default; use fifo_nucleo3.flag to move it)\n");
        std::fflush(stdout);
        return;
    }
    std::fclose(bandera);
    const Result r = svcSetThreadCoreMask(CUR_THREAD_HANDLE, 3, 1u << 3);
    std::printf("[núcleos] GX FIFO -> core 3: %s (0x%x)\n", R_SUCCEEDED(r) ? "yes" : "failed", r);
    std::fflush(stdout);
}
