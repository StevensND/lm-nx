// Actual GCC counters; dump on exit after joining all workers. Redirect embedded PC paths to SD, as in NFSMW-NX.
#ifdef LMNX_PGO_GENERATE
#include "pgo_switch.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <sys/stat.h>
extern "C" FILE* __real_fopen(const char*,const char*);
extern "C" void __gcov_dump();
extern "C" FILE* __wrap_fopen(const char* ruta,const char* modo) {
    if(ruta){
        std::string normal=ruta;std::replace(normal.begin(),normal.end(),'\\','/');
        const std::string prefijo=std::string(LMNX_PGO_HOST_DIR)+"/";
        if(normal.starts_with(prefijo)) {
            auto nombre=normal.substr(prefijo.size());
            if(nombre.find('/')!=std::string::npos || nombre.find("..")!=std::string::npos)return nullptr;
            mkdir("sdmc:/switch/LM-NX/pgo",0777);
            return __real_fopen(("sdmc:/switch/LM-NX/pgo/"+nombre).c_str(),modo);
        }
    }
    return __real_fopen(ruta,modo);
}
extern "C" void lmnx_pgo_volcar(){__gcov_dump();}
extern "C" void __gcov_reset();
extern "C" void lmnx_pgo_tick(){
    static unsigned vueltas=0;
    if(++vueltas>=1800){vueltas=0;__gcov_dump();__gcov_reset();}
}
#else
extern "C" void lmnx_pgo_volcar(){}
extern "C" void lmnx_pgo_tick(){}
#endif
