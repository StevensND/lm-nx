# Avoid checking two thread_local recording variables on every GX FIFO write. With -mtp=soft each access calls
# __aarch64_read_tp. A global active-recording counter allows returning a shared inactive recording without TLS
# when zero; writes still go to the FIFO. Otherwise use the original path. prepare_aurora_native.py calls
# aplicar(destino) after transformaciones_41.


def aplicar(destino):
    def cambiar(rel, viejo, nuevo):
        ruta = destino / rel
        texto = ruta.read_text(encoding="utf-8")
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"Global recording, {rel}: {n} matches; expected one")
        ruta.write_text(texto.replace(viejo, nuevo, 1), encoding="utf-8")

    cambiar(
        "lib/gx/fifo.hpp",
        """extern thread_local Recording tOwnRecording;
extern thread_local Recording* tJoinedRecording;
inline Recording& recording() noexcept {
  Recording* joined = tJoinedRecording;
  return joined != nullptr ? *joined : tOwnRecording;
}""",
        """extern thread_local Recording tOwnRecording;
extern thread_local Recording* tJoinedRecording;
// LMNX: recordings active in any thread (begin_display_list / end_display_list). While it is 0 every thread's
// recording is inactive, so recording() returns sIdleRecording (never active, never written) without reading the
// thread-local variables: each access is a call to __aarch64_read_tp with -mtp=soft, and every GX command made one or
// more. recordingOfThread() is the thread's own (or joined) recording, for the functions that change it.
extern std::atomic<uint32_t> sActiveRecordings;
extern Recording sIdleRecording;
inline Recording& recordingOfThread() noexcept {
  Recording* joined = tJoinedRecording;
  return joined != nullptr ? *joined : tOwnRecording;
}
inline Recording& recording() noexcept {
  if (sActiveRecordings.load(std::memory_order_relaxed) == 0)
    LIKELY { return sIdleRecording; }
  return recordingOfThread();
}""",
    )
    cambiar("lib/gx/fifo.hpp", "#include <cstring>\n", "#include <atomic>\n#include <cstring>\n")

    cambiar(
        "lib/gx/fifo.cpp",
        "thread_local Recording* tJoinedRecording = nullptr;\n",
        "thread_local Recording* tJoinedRecording = nullptr;\n"
        "std::atomic<uint32_t> sActiveRecordings{0}; // LMNX: see fifo.hpp\n"
        "Recording sIdleRecording;\n",
    )
    cambiar(
        "lib/gx/fifo.cpp",
        "  detail::recording() = {};\n",
        "  if (detail::recordingOfThread().active) {\n"
        "    detail::sActiveRecordings.fetch_sub(1, std::memory_order_relaxed);\n"
        "  }\n"
        "  detail::recordingOfThread() = {};\n",
    )
    cambiar(
        "lib/gx/fifo.cpp",
        """void begin_display_list(uint8_t* buf, uint32_t size) {
  auto& rec = detail::recording();
  rec.active = true;""",
        """void begin_display_list(uint8_t* buf, uint32_t size) {
  auto& rec = detail::recordingOfThread();
  if (!rec.active) {
    detail::sActiveRecordings.fetch_add(1, std::memory_order_relaxed);
  }
  rec.active = true;""",
    )
    cambiar(
        "lib/gx/fifo.cpp",
        """uint32_t end_display_list() {
  auto& rec = detail::recording();
  rec.active = false;""",
        """uint32_t end_display_list() {
  auto& rec = detail::recordingOfThread();
  const bool wasActive = rec.active;
  rec.active = false;
  if (wasActive) {
    detail::sActiveRecordings.fetch_sub(1, std::memory_order_relaxed);
  }""",
    )
