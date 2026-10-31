// Actual CPU time per thread (GetThreadTimes), producer counters and plume GPU timestamps.
#include "gpu_pc.hpp"
#include <tlhelp32.h>
#include <chrono>
#include <cstdio>
#include <map>
#include <cstdlib>
#include <stdexcept>
namespace lmnx::pc {
namespace {
uint64_t dibujos=0,gpu=0,fotograma=0;bool gpuValido=true;
struct Hilo {HANDLE handle;uint64_t anterior;};
uint64_t cpu(HANDLE h){FILETIME c,e,k,u;if(!GetThreadTimes(h,&c,&e,&k,&u))return 0;return ((uint64_t(k.dwHighDateTime)<<32)|k.dwLowDateTime)+((uint64_t(u.dwHighDateTime)<<32)|u.dwLowDateTime);}
struct Registro {
    FILE* log=nullptr;std::map<DWORD,Hilo> hilos;
    std::chrono::steady_clock::time_point anterior=std::chrono::steady_clock::now(),censo{};
    Registro(){if(auto* ruta=std::getenv("LMNX_PERF_LOG")){log=std::fopen(ruta,"w");if(!log){std::fprintf(stderr,"[rendimiento] Could not open LMNX_PERF_LOG; logging disabled\n");return;}std::fputs("fotograma,intervalo_ms,fps,hilo_id,cpu_hilo_ms,dibujos,gpu_ms\n",log);}}
    ~Registro(){for(auto& [id,h]:hilos)CloseHandle(h.handle);if(log)std::fclose(log);}
};
}
void contarDibujo(){++dibujos;}
void registrarEnvio(uint64_t ns,bool presentar){
    static Registro r;if(!r.log)return;if(ns==UINT64_MAX)gpuValido=false;else gpu+=ns;if(!presentar)return;
    auto ahora=std::chrono::steady_clock::now();
    if(ahora-r.censo>std::chrono::seconds(1)) {
        // Do not retain finished thread handles or attribute a reused ID's CPU time to the old thread during a long
        // session.
        for(auto it=r.hilos.begin();it!=r.hilos.end();) {
            DWORD estado=STILL_ACTIVE;
            if(GetExitCodeThread(it->second.handle,&estado) && estado!=STILL_ACTIVE){CloseHandle(it->second.handle);it=r.hilos.erase(it);}else ++it;
        }
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 e{sizeof(e)};
        if(snap!=INVALID_HANDLE_VALUE){if(Thread32First(snap,&e))do{if(e.th32OwnerProcessID==GetCurrentProcessId() && !r.hilos.contains(e.th32ThreadID)) {
            auto h=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,e.th32ThreadID);if(h)r.hilos.emplace(e.th32ThreadID,Hilo{h,cpu(h)});
        }}while(Thread32Next(snap,&e));CloseHandle(snap);}r.censo=ahora;
    }
    ++fotograma;
    double ms=std::chrono::duration<double,std::milli>(ahora-r.anterior).count();
    for(auto& [id,h]:r.hilos){auto t=cpu(h.handle);std::fprintf(r.log,"%llu,%.6f,%.3f,%lu,%.6f,%llu,%.6f\n",fotograma,ms,ms>0?1000./ms:0,id,t>=h.anterior?double(t-h.anterior)/10000.:0.,dibujos,gpuValido?double(gpu)/1e6:-1.);h.anterior=t;}
    std::fflush(r.log);r.anterior=ahora;gpu=0;dibujos=0;gpuValido=true;
}
}
