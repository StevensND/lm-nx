/* FPS and resolution through SaltyNX shared memory, plus Reverse-NX state. Block details are in platforms/switch/switch_saltynx.cpp. */
#ifndef LMNX_SWITCH_SALTYNX_H_
#define LMNX_SWITCH_SALTYNX_H_

#include <cstdint>



namespace lmnx::saltynx {

#if defined(__SWITCH__)

// Connect to SaltySD, allocate the block and locate Reverse-NX. Do nothing without SaltyNX. Safe to call
// repeatedly: retry until a valid block exists, scanning the mapped page before using IPC.
void Iniciar();

// Call once per second with recent FPS, average FPS, presented resolution and total frame count. Publish FPS and
// resolution together and reconnect a missing block. Call from startup so data appears during loading and when
// the overlay opens later.
void Actualizar(double fps_segundo, double fps_media, uint32_t ancho, uint32_t alto, uint64_t fotogramas);

// Call on every presentation. The overlay clears heartbeat and resolution markers and waits only 100 ms for
// restoration. Also record frame intervals used for average FPS.
void Latir(uint32_t ancho, uint32_t alto);

// Enable or disable through the game's setting before the profiling thread starts.
void Habilitar(bool habilitado);

// Use Reverse-NX mode when active; otherwise use the supplied actual mode.
bool ModoBase(bool real);

// Raw Reverse-NX state for profiling. por_defecto means Controlled by system: when true, Mode only reflects
// actual console state and does not override it. Without a block, hay = false and other values have no meaning.
struct EstadoReverseNx {
  bool hay;
  bool en_base;
  bool por_defecto;
  bool plugin_activo;
};
EstadoReverseNx EstadoReverse();

#else

inline void Iniciar() {}
inline void Actualizar(double, double, uint32_t, uint32_t, uint64_t) {}
inline void Latir(uint32_t, uint32_t) {}
inline void Habilitar(bool) {}
inline bool ModoBase(bool real) { return real; }
struct EstadoReverseNx {
  bool hay;
  bool en_base;
  bool por_defecto;
  bool plugin_activo;
};
inline EstadoReverseNx EstadoReverse() { return {false, false, false, false}; }

#endif  // defined(__SWITCH__)

}  // namespace lmnx::saltynx

#endif  // LMNX_SWITCH_SALTYNX_H_
