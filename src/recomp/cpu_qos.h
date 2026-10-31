#pragma once

// Thread scheduling policy for the time-critical threads (issue #18, docs/RENDERING.md "CPU
// scheduling: hybrid CPUs and power throttling"): priority, no power throttling (EcoQoS), and on
// hybrid CPUs (Intel P-cores + E-cores) the performance cores only. Logs the CPU, whether it is
// hybrid and, on hybrid CPUs, which kind of core each thread ran on (last_run.log).
//
// Test aids: LM_CPU_QOS=0 (none of this: the old behaviour), LM_CPU_SETS=p|any|e (hybrid: the
// performance cores (default), no CPU set, or the efficiency cores only: what bad scheduling costs),
// LM_CPU_LOG=<seconds> (core-use summary interval, default 60; 0 = off).

namespace recomp::cpu {

enum class Role : unsigned char {
    Host,    // main thread: frame loop, interpolation replay, pacing
    Render,  // Aurora render worker (encoding, submit)
    Fifo,    // Aurora FIFO processor
    Guest,   // guest (game) threads; only one runs at a time
    Timer,   // VI retrace / interrupt timers, vblank monitor
    Audio,   // AI DMA pacing thread
    Count,
};

#if defined(__SWITCH__) || defined(LMNX_PC_TEST)
// PORT (ProjectLM-NX): the console has no hybrid cores, priorities through these APIs or power
// throttling; its threads get their cores and priorities from platforms/switch/thread_policy.c
// (lmnx::plataforma::configurarHilo). Empty here, so the scheduler's tick costs nothing. The port's PC
// test builds (LMNX_PC_TEST, platforms/pc) compile the console's source list, without cpu_qos.cpp.
inline void init() {}
inline void setCurrent(Role) {}
inline void tick() {}
inline void report() {}
#else
// Once, early in main (before aurora_initialize): process-wide settings, CPU detection, the
// Aurora thread hooks.
void init();
// Applies the policy for `role` to the calling thread.
void setCurrent(Role role);
// Samples the core type the calling thread runs on (cheap; no-op unless the thread has a role).
void tick();
// Host thread, once per frame: periodic summary and window-focus changes in the log.
void report();
#endif

}  // namespace recomp::cpu
