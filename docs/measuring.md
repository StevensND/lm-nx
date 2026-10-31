# Measuring performance on the Switch

Measure the actual console, at known clocks, in the same scene. A successful compile or a fast PC comparison does
not establish Switch performance. LM-NX's history contains several cases where a plausible counter described
waiting, diagnostic overhead or the wrong workload. See [performance-history.md](performance-history.md).

## A repeatable test

1. Install the exact NRO and matching shaders. Keep its ELF and a record of the settings.
2. Start in the intended hardware mode. LM-NX selects 720p handheld or 1080p docked at startup.
3. Confirm the real CPU, GPU and memory clocks. Keep external profiles consistent.
4. Load the same save and reach the same place, facing the same direction.
5. Let loading and shader preparation finish. Measure gameplay separately from the loading screen and menu.
6. Collect several reporting windows, then repeat. Keep room transitions and fights as separate cases.

The foyer near the entrance was a fixed reference during development. A useful route also includes the stairs,
corridor, study and a ghost capture. The Master Bedroom exposed load that the foyer did not. Reaching 30 FPS in one
position does not establish every room's performance.

Open settings with **L3 + R3**. For final rate comparisons, disable the performance panel and detailed stack
sampling. The panel itself cost about **2 FPS** in a documented foyer comparison: **26.7 FPS with it**, **28.6
without it**.

## Identify the limit before changing it

A 30 FPS frame has about **33.3 ms**. The CPU game thread prepares work, the graphics worker translates and encodes
it, and the GPU executes it. Their work can overlap. Adding their times usually does not give the frame time.

| Observation | Likely next question |
|---|---|
| Game-thread time exceeds the budget, GPU is faster | Which game or GX-call work is on the critical path? |
| FIFO/encoding worker is full, GPU waits for work | Which per-draw preparation or copy dominates? |
| GPU work approaches the frame time | Which passes, copies or pixel operations dominate? |
| Elapsed time is high but CPU use is low | Is the thread waiting, preempted or blocked on a lock? |
| Only first start is slow | Are shaders compiling rather than loading? |

Check actual work and waiting separately. `GXCopyDisp` included about **3.5 ms of handoff waiting** in one trace;
that was not 3.5 ms of drawing arithmetic. A native rewrite of that call would not automatically remove the wait.

## Compare one change at a time

An A/B comparison runs version A and version B under matching conditions. When a runtime switch safely selects the
two paths, alternate them within the same session and log the active mode. Otherwise install each NRO in turn and
repeat the same route. Keep unrelated settings fixed.

Make sure the feature acts. The first one-pass early-depth implementation compiled but did not match the scene's
multiple color targets. An enabled setting alone would have suggested it failed as an optimization. Counters of
eligible draws, merged draws and fallback reasons showed that it was not yet being exercised.

Compare the picture as well as the time. Check alpha cutouts, fades, shadows, lights and Game Boy Horror after
changes to depth order or EFB copies. Compilation success and identical source expressions do not prove identical
GPU output.

## Read counters by their implementation

- **Cumulative or interval:** subtract running totals before dividing by the frames in one window.
- **Unit and denominator:** milliseconds per frame, CPU percent of one core and microseconds per draw answer
  different questions.
- **Attempts and actions:** count both calls and actual work. A zero failure count proves little if nothing ran.
- **Memory scope:** live, peak and cumulative allocations are different. Backend block usage excludes some SDK
  allocations and is not the process's entire memory use.
- **Shader counts:** configurations, pipeline requests, translated stages and distinct DKSH entries are different.
  Three preparation threads do not mean three independent uam compiles on the console.

In early tests, allocator statistics in the panel cost about **10 ms** in the handoff path. Per-GX-call timers
later added about **2 ms** with the panel. A measurement tool can dominate what it measures. Use targeted timers,
sample where appropriate, and repeat without instrumentation.

## CPU stack sampling

The console sampler pauses threads briefly and records their program locations and call chains. Resolve addresses
against the **matching ELF**. A profile from another NRO can assign costs to the wrong functions.

Samples help locate work but are not an exact stopwatch for every function. Lost samples, short windows, pauses,
inlining and the way stacks are grouped can distort percentages. During development, snapshot copying appeared
much more expensive in a stack summary than the direct **1.4 ms** timer supported. Changes based on that estimate
promised more than they delivered. Confirm a proposed saving with a timer around the actual operation.

Keep profiling runs separate from final FPS runs. The sampler itself consumes CPU and changes scheduling.

## GPU timing

GPU markers bracket submitted work, not necessarily one source function. Know where each timestamp is recorded and
when its result becomes safe to read. The native implementation converts GPU ticks using **625/384 nanoseconds
per tick**, about **1.627 ns**, and publishes a frame's result after its submissions are complete. Its reported
millisecond values already include that conversion: do not multiply them again. Check total duration against wall
time before interpreting small changes. See [native_gfx.cpp](../LMNX-Deko3D/src/native_gfx.cpp).

An interval named after a conversion can include later unmarked encoding or transfer work. The two RGB565
conversion intervals measured at **4.99 ms per frame** did not prove that 4.99 ms belonged to shader arithmetic.
Likewise, an apparent saving in one named pass may have moved work into the next interval.

For docked 1080p, the foyer report gave GPU **30.46 ms**, game **25.4 ms** and FIFO **23.1 ms**. That points to the
GPU budget. It would be misleading to claim another CPU optimization alone supplies a large docked margin.

## Smoothness and percentiles

FPS is frames per second. Frame time tells how long each frame takes; frame pacing describes how evenly frames
reach the screen. At a 60 Hz display refresh, two refreshes are about 33.3 ms and three are 50 ms. Missed deadlines
can therefore be noticeable even when the average is close to 30 FPS.

Report the average together with slow-frame information and the measured scene. A “1% low” summarizes the slowest
part of a sample; state the calculation if producing a new report. The historical Master Bedroom test reported
**29.4 FPS average and 21.5 FPS 1% low**. That supports “about 30”, not “never below 30”.

## Audio and startup have their own tests

An empty audio output queue indicates missing samples, but crackling can instead come from damaged compressed
input. The thunder investigation combined a recent-audio recording with disc offsets to prove a repeated chunk.
See [switch-audio.md](switch-audio.md).

For startup, record whether the translation and DKSH caches were present and usable. A warm start loading cached
entries is not comparable to a full **772-778 second** console compile. The usual installation supplies
browser-built caches; see [shaders.md](shaders.md).
