// SwitchWaker method: enlarge pthread stacks without detaching threads. Each owner retains its join; no system
// cores are reserved.
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <switch.h>
int __real_pthread_create(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);
struct Inicio { void*(*funcion)(void*); void* argumento; };
static void* iniciar(void* dato) {
    const struct Inicio inicio = *(struct Inicio*)dato;
    free(dato);
    uint64_t permitidos = 0;
    if (R_SUCCEEDED(svcGetInfo(&permitidos, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0))) {
        const uint64_t mascara = permitidos & 7;
        if (mascara) svcSetThreadCoreMask(CUR_THREAD_HANDLE, (mascara & 4) ? 2 : __builtin_ctzll(mascara), mascara);
    }
    svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2C);
    return inicio.funcion(inicio.argumento);
}
int __wrap_pthread_create(pthread_t* hilo, const pthread_attr_t* atributos, void*(*funcion)(void*), void* argumento) {
    pthread_attr_t propios;
    size_t bytes = 0;
    if (atributos) {
        propios = *atributos;
        pthread_attr_getstacksize(atributos, &bytes);
        void* memoria = NULL;
        if (pthread_attr_getstackaddr(atributos, &memoria) == 0 && memoria)
            return __real_pthread_create(hilo, atributos, funcion, argumento);
    } else {
        const int codigo = pthread_attr_init(&propios);
        if (codigo) return codigo;
    }
    if (bytes < (1u << 20)) bytes = 1u << 20;
    if (bytes > SIZE_MAX - 0xFFF) return EINVAL;
    int codigo = pthread_attr_setstacksize(&propios, (bytes + 0xFFF) & ~(size_t)0xFFF);
    struct Inicio* inicio = codigo ? NULL : malloc(sizeof *inicio);
    if (!codigo && !inicio) codigo = ENOMEM;
    if (!codigo) {
        *inicio = (struct Inicio){funcion, argumento};
        codigo = __real_pthread_create(hilo, &propios, iniciar, inicio);
        if (codigo) free(inicio);
    }
    if (!atributos) pthread_attr_destroy(&propios);
    return codigo;
}
