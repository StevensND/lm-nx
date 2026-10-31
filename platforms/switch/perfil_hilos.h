// Performance panel Thread CPU use, as in banjo-nx: thread names and CPU use (platforms/switch/perfil.cpp). Weak
// symbols let test ELFs omit perfil.cpp without undefined strong symbols.
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
// Name the calling thread and register it if needed.
__attribute__((weak)) void lmnx_perfil_nombrar_hilo(const char* nombre);
// name 12.3% (c0 40, c3 9), ... since the previous call; return the written length.
__attribute__((weak)) size_t lmnx_perfil_uso_cpu(char* salida, size_t capacidad);
// Four per-core percentages used by all tracked threads in the last lmnx_perfil_uso_cpu window.
__attribute__((weak)) void lmnx_perfil_carga_nucleos(double* nucleos);
// Total tracked-thread CPU percentage in the last lmnx_perfil_uso_cpu window; 400 means all four cores.
__attribute__((weak)) double lmnx_perfil_cpu_total(void);
// The next lmnx_perfil_uso_cpu query only resets baselines after the panel is enabled again.
__attribute__((weak)) void lmnx_perfil_reiniciar_uso(void);

static inline void lmnx_perfil_nombrar(const char* nombre) {
    if (lmnx_perfil_nombrar_hilo) lmnx_perfil_nombrar_hilo(nombre);
}
#ifdef __cplusplus
}
#endif
