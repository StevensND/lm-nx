#include "diagnostico_estado.hpp"
extern "C" void lmnx_diagnostico_estado(bool juego) {
    static uint64_t siguiente=0;
    lmnx::diagnostico::marcaJuego.store(juego?siguiente++:UINT64_MAX,std::memory_order_relaxed);
}
