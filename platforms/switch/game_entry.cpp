// Actual libnx entry point for the static AOT runtime; no fake GameModule.
#include "runtime.hpp"
#include "switch_saltynx.h"
namespace lmnx::perfil { void Iniciar(); }
#include <thread>
extern "C" void RexSwitchApmPedirGpuMhz(int mhz);
extern "C" void RexSwitchApmAplicar(void);
extern "C" void RexSwitchApmVigilar(void);
#include <cstdio>
#include <unistd.h>
#include <switch.h>
namespace recomp { extern unsigned char* g_mem; }
extern "C" {
// libnx provides these weak symbols. A separate stack also allows reporting a crashing thread's stack overflow.
alignas(16) unsigned char __nx_exception_stack[64*1024];
u64 __nx_exception_stack_size=sizeof(__nx_exception_stack);
extern char __start__[];
void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    FILE* f=std::fopen("sdmc:/switch/LM-NX/logs/excepcion-ultima.log","w");if(!f)f=stderr;
    std::fprintf(f,"[excepción Horizon] cause=%08x base=%p pc=%016llx lr=%016llx sp=%016llx fp=%016llx far=%016llx\n",
        ctx->error_desc,__start__,(unsigned long long)ctx->pc.x,(unsigned long long)ctx->lr.x,
        (unsigned long long)ctx->sp.x,(unsigned long long)ctx->fp.x,(unsigned long long)ctx->far.x);
    for(unsigned i=0;i<29;++i)std::fprintf(f,"x%u=%016llx\n",i,(unsigned long long)ctx->cpu_gprs[i].x);
    // Memory properties at the fault address, including guest RAM allocated on the heap.
    {MemoryInfo m{};u32 pg=0;if(R_SUCCEEDED(svcQueryMemory(&m,&pg,ctx->far.x)))
        std::fprintf(f,"far: region=%016llx+%llx tipo=%x atributos=%x permisos=%x\n",(unsigned long long)m.addr,(unsigned long long)m.size,m.type,m.attr,m.perm);}
    std::fprintf(f,"g_mem=%p\n",(void*)recomp::g_mem);
    MemoryInfo memoria{};u32 pagina=0;uintptr_t fp=ctx->fp.x;
    if(R_SUCCEEDED(svcQueryMemory(&memoria,&pagina,ctx->sp.x)) && (memoria.perm&Perm_R)) {
        const uintptr_t fin=memoria.addr+memoria.size;
        for(unsigned i=0;i<48 && !(fp&15) && fp>=ctx->sp.x && fp<=fin && fin-fp>=16;++i) {
            const auto* marco=reinterpret_cast<const uintptr_t*>(fp);
            std::fprintf(f,"#%u pc=%016llx\n",i,(unsigned long long)marco[1]);
            if(marco[0]<=fp)break;fp=marco[0];
        }
    }
    std::fflush(f);if(f!=stderr)std::fclose(f);
    lmnx::plataforma::vaciarLog(true);  // Remaining bytes in the asynchronous log ring.
    svcExitProcess();
}
}
extern "C" int aurora_main(int argc, char* argv[]);
int main(int argc, char* argv[]) {
    std::string error;
    if (!lmnx::plataforma::prepararDirectorios(error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (chdir(lmnx::plataforma::raiz) != 0) {
        std::perror("Could not select the LM-NX SD directory");
        return 1;
    }
    // Switch stdout/stderr have no default destination. Send game, Aurora and crash-handler output to SD logs. Keep
    // stderr separate because two FILE objects writing to one file can overwrite each other. Use log_asincrono.cpp
    // to avoid synchronous SD write stalls.
    if (!lmnx::plataforma::iniciarLogAsincrono() && std::freopen("logs/ProjectLM-NX.log", "w", stdout)) {
        std::setvbuf(stdout, nullptr, _IOFBF, 16 * 1024);  // Buffer blocks to avoid a synchronous SD write per line.
        if (std::freopen("logs/errores.log", "w", stderr)) std::setvbuf(stderr, nullptr, _IONBF, 0);
    }
    std::printf("[arranque] ProjectLM-NX\n");
    // Publish FPS and resolution for SaltyNX / Horizon OC Monitor, which does not inject into homebrew.
    lmnx::saltynx::Iniciar();
    // Profile CPU per thread and, with perfil_pilas.flag on the SD card, frequent stacks in logs/perfil.log.
    lmnx::perfil::Iniciar();
    // Stock clocks, as in NFSMW-NX (platforms/switch/switch_apm.cpp): request and monitor a 460.8 MHz GPU in
    // handheld mode without changing memory clocks. Leave docked mode at 768 MHz. Use a persistent thread because
    // Horizon does not support detach().
    RexSwitchApmPedirGpuMhz(460);
    // Heap allocated without destruction; destroying a joinable std::thread calls std::terminate.
    new std::thread([] {
        RexSwitchApmAplicar();
        for (;;) {
            svcSleepThread(1000000000LL);
            RexSwitchApmVigilar();
        }
    });
    std::atexit([] {
        std::printf("[salida] exit() called\n");
        std::fflush(stdout);
        std::fflush(stderr);
        lmnx::plataforma::detenerLog();
    });
    lmnx::plataforma::configurarHilo(lmnx::plataforma::Rol::Juego);
    return aurora_main(argc, argv);
}
