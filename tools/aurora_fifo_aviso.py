# Notify GX FIFO waiters only when needed. Horizon lacks futex support; libstdc++ notify_all uses a mutex and
# condition variable. Advertise sleeping/waiting through sWorkerSleeping and sDrainWaiters. Avoid lost wakeups
# using Dekker-style seq_cst ordering: the producer increments sWorkerWake before reading sWorkerSleeping; the
# FIFO thread sets sleeping before rereading sWorkerWake. If the producer sees no sleeper, the later reread sees
# the increment and avoids waiting. Apply the same protocol to sProcessed/sDrainWaiters with a seq_cst fence
# after the sProcessed store. atomic::wait rechecks before blocking. prepare_aurora_native.py applies this after
# aurora_earlyz_fix.


def aplicar(destino):
    rel = "lib/gx/fifo.cpp"
    ruta = destino / rel
    texto = ruta.read_text(encoding="utf-8")
    cambios = [
        ("std::atomic<uint32_t> sWorkerWake{0};\n",
         "std::atomic<uint32_t> sWorkerWake{0};\n"
         "// LMNX: notify only a sleeping worker / a waiting drain (protocol in tools/aurora_fifo_aviso.py).\n"
         "std::atomic<bool> sWorkerSleeping{false};\n"
         "std::atomic<uint32_t> sDrainWaiters{0};\n"),
        ("""void wake_worker() noexcept {
  sWorkerWake.fetch_add(1, std::memory_order_release);
  sWorkerWake.notify_all();
}""",
         """void wake_worker() noexcept {
  sWorkerWake.fetch_add(1, std::memory_order_seq_cst);
  if (sWorkerSleeping.load(std::memory_order_seq_cst)) {
    sWorkerWake.notify_all();
  }
}"""),
        ("""    sProcessed.store(processed, order);
    sProcessed.notify_all();
""",
         """    sProcessed.store(processed, order);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (sDrainWaiters.load(std::memory_order_seq_cst) != 0) {
      sProcessed.notify_all();
    }
"""),
        ("""    sWorkerWake.wait(event, std::memory_order_acquire);
    if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {""",
         """    sWorkerSleeping.store(true, std::memory_order_seq_cst);
    if (sWorkerWake.load(std::memory_order_seq_cst) == event) {
      sWorkerWake.wait(event, std::memory_order_acquire);
    }
    sWorkerSleeping.store(false, std::memory_order_relaxed);
    if (lmnx::perfil::activo.load(std::memory_order_relaxed)) {"""),
        ("""      do {
        sProcessed.wait(processed, std::memory_order_acquire);
        processed = sProcessed.load(std::memory_order_acquire);
      } while (processed < target);""",
         """      sDrainWaiters.fetch_add(1, std::memory_order_seq_cst);
      processed = sProcessed.load(std::memory_order_seq_cst);
      while (processed < target) {
        sProcessed.wait(processed, std::memory_order_acquire);
        processed = sProcessed.load(std::memory_order_acquire);
      }
      sDrainWaiters.fetch_sub(1, std::memory_order_relaxed);"""),
    ]
    for viejo, nuevo in cambios:
        n = texto.count(viejo)
        if n != 1:
            raise ValueError(f"FIFO notifications, {rel}: {n} matches; expected one: {viejo[:70]!r}")
        texto = texto.replace(viejo, nuevo, 1)
    ruta.write_text(texto, encoding="utf-8")
