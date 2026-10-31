// Host test of Switch scheduler branches; does not call libnx.
#include "recomp/sched.h"
#include "runtime.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
namespace {
std::atomic<unsigned> llamadas{0}, finalizados{0};
unsigned char memoria[1024 * 1024]{};
}
namespace lmnx::plataforma { bool configurarHilo(Rol) { return true; } }
namespace recomp {
u8* g_mem = memoria;
bool g_traceOn = false, g_watchOn = false;
void log(const char*, ...) {}
[[noreturn]] void fatal(const char*, ...) { std::abort(); }
void watch_store(u32,u32,int) { std::abort(); }
void mmio_wr(u32,u32,int) { std::abort(); }
u32 mmio_rd(u32,int) { std::abort(); }
void trace_enter(u32,Ctx&) {}
void Ctx::call(u32) { ++llamadas; }
u32 viRetraceCount() { return 0; }
}
int main() {
    using namespace recomp;
    Ctx inicio;
    inicio.r[1] = 0x800F0000;
    sched::adoptBootThread(0x80000100, inicio);
    for (unsigned i=0; i<100; ++i) {
        if (!sched::create(0x80001000, 0x80010000, i, 0x800E0000, 0x1000, 8, 1)) return 1;
        sched::resume(0x80001000);
        if (!sched::isTerminated(0x80001000)) return 2;
    }
    sched::postIrqAfter(3600, [](Ctx&) { std::abort(); });
    for (unsigned i=0; i<4; ++i) sched::ownBackground(std::thread([] {
        while (!sched::stopping()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++finalizados;
    }));
    sched::shutdown();
    if (!sched::waitForThreads(2) || llamadas != 100 || finalizados != 4) return 3;
    std::puts("100 OSThread reuses; timer awakened; 4 workers joined without detach.");
}
