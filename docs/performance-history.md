# Performance history

LM-NX reached about **30 FPS at stock clocks** in the measured foyer and Master Bedroom. This page follows the
work that made that possible, including changes that failed. A frame at 30 FPS has about **33.3 ms** to finish.
The CPU game thread, the graphics worker and the GPU all have to stay within that budget.

This is a dated account of console measurements and the development notes that explain them. Early emulator and
PC observations are identified separately. They are not Switch benchmarks. See [measuring.md](measuring.md) for
the method and [deko3d-devlog.md](deko3d-devlog.md) for renderer correctness work.

## Reading the results

The normal handheld target is **1280x720**, CPU **1020 MHz**, GPU **460.8 MHz**, memory **1331.2 MHz**.
Docked operation uses **1920x1080**, CPU **1020 MHz**, GPU **768 MHz**, memory **1600 MHz**. These are the project's
stock performance settings, without overclocking. Not every early session recorded all three clocks; those early
results explain the development sequence rather than establish a controlled speedup against the release.

Later foyer tests used a repeatable position near the entrance. Moving through rooms, fighting ghosts, opening a
menu and loading a save change the workload. Measurements with the diagnostic panel open also include its cost.
Several changes landed together, so their individual gains cannot always be separated.

| Date | Milestone | Observed result |
|---|---|---|
| 2026-10-22 | First real-console run | Black image; about 0.46 presentations/s; repeated long fence waits |
| 2026-10-23 | Visible loading bar, renderer still too expensive | About 0.81-0.84 images/s in the early console test |
| 2026-10-25 | More command segments allowed in flight | 15.7 to 17.7 FPS in that comparison |
| 2026-10-25 | Filtered snapshots | Foyer 17.9 to 19.2 FPS |
| 2026-10-25 | Blending, recording and memory changes | Foyer 19.2 to 21.7 FPS |
| 2026-10-26 | One-pass early depth behavior | GPU 34.4 to 23.8 ms; CPU still above the frame budget |
| 2026-10-26 | Less game-thread work and less measurement overhead | Foyer 28.6 FPS with the panel off |
| 2026-10-26 | Buffered logging and broader compiler optimization | Foyer 29.5 FPS without PGO |
| 2026-10-28 | Clean baseline before final training | Foyer 30.0 FPS; Master Bedroom about 26.5 FPS |
| 2026-10-28 | Console-trained PGO with LTO | Foyer 30.0; Master Bedroom 29.4 FPS average |
| 2026-10-30 | Docked 1080p | Foyer 30.0 FPS; GPU about 30.46 ms |

These rows are milestones, not a sum of independent gains. In particular, the first black-screen runs cannot be
used to calculate a meaningful gameplay speedup.

## 2026-10-21 to 2026-10-23: first frames were not yet playable

The renderer used deko3d from the start. There was no intermediate Switch renderer to replace. The first work was
connecting Aurora's GameCube graphics to native resources, translating shaders, and keeping resources alive until
the GPU finished with them.

The first emulator observation was 1-3 FPS after shader preparation. The real Switch exposed a different failure:
the first run prepared **6,110 pipeline requests in 163.13 seconds**, using an existing shader cache and one
worker, then produced a black image. There were **337 warnings** about fences not signaling after two seconds.
A fence is a signal that GPU work has finished. About **0.46 presentations/s** here means presentation attempts,
not playable frames.

Aurora's GPU keep-warm work was a major problem: it filled a **64 MiB** buffer repeatedly, up to **400 passes per
frame**. That artificial work was disabled on Switch. A PC check reduced light-screen GPU time from **48 to 1 ms**;
that is a PC diagnosis, not a measured Switch gain.

The loading bar had a separate bug: the native adapter did not supply its vertex attributes, so its geometry
collapsed. Fixing it made progress visible. A later cold-cache console run took **946.02 seconds**, with one worker,
and still ran at only **0.81-0.84 images/s** afterwards. Four in-flight segments did not solve the large per-draw
CPU cost. This ruled out the idea that segment waits alone explained the slow renderer.

The next step was a direct batch path: prepare a whole render pass and encode it together, reusing its state,
resource bindings and texture transitions. This removed repeated generic operations around every draw.
See [native-renderer.md](native-renderer.md).

## 2026-10-24: remove stalls before chasing arithmetic

Several system costs had to be separated from rendering:

- Frequent synchronous log writes to the SD card disturbed gameplay. Logging was reduced, then later moved behind
  a buffered writer.
- Disc reads directly into guest memory could fail when those pages were unsuitable for system file operations.
  A host-owned staging buffer made reads safe before copying them to the game's memory.
- Thousands of separate shader-cache files were replaced by packed files. A warm-cache startup was recorded at
  about **3.6 seconds** during this period. This is loading saved shaders, not compiling them from scratch.
- Broad GPU barriers and repeated descriptor invalidation were reviewed. A barrier orders GPU operations; a
  descriptor tells a shader where a texture or buffer is. Replacing descriptor tables every frame created extra
  invalidation work rather than useful drawing.

The emerging limits were roughly **45 ms** on the game thread and **42-66 ms** of GPU work. Either one was too
slow for 30 FPS. Local-register and stack-access experiments did not yet show a clear improvement.

A proposed six-bit blending path read the image it was also drawing into. It added about **11 ms** of GPU time and
was rejected. Lowering handheld resolution was also set aside: optimization continued at 1280x720.

## 2026-10-25: overlap useful work and reduce copies

### More command segments in flight

Increasing the allowed in-flight segments from **4 to 32**, while retaining safe mapped-buffer handling, raised
the measured rate from **15.7 to 17.7 FPS**. More work could reach the GPU before the CPU waited for completion.
This required retaining every referenced resource until its fence; reusing memory early would only trade a wait
for corruption.

Moving the GX FIFO worker onto the fourth core did not produce a useful FPS gain and reduced its available CPU
time. That experimental placement was dropped as a default. Ordinary application work uses the permitted cores;
the release does not require a modified loader to reserve another core.

The panel itself exposed a trap: querying allocator statistics with `mallinfo()` cost about **10 ms** in the handoff
path. A tool used to find a bottleneck had become one.

### Filter snapshots instead of copying everything

Aurora must preserve some guest data while another thread consumes its graphics commands. These preserved copies
are snapshots. Filtering them using pages marked by cache-management and transfer operations reduced snapshot
work from **2.1 ms and 1.68 MB per frame** to **1.39 ms and 378 KiB per frame**. The foyer rose from **17.9 to
19.2 FPS**, with no reported face flicker or moving-hose corruption in that test.

The three important times were then close: game thread **42 ms**, FIFO **35 ms**, GPU **42 ms**. Optimizing only one
would soon expose another.

### Blending, GX recording and memory access

The next batch alternated two blending surfaces, removed thread-local recording checks from the GX hot path, and
simplified the common guest-memory address check. The foyer rose from **19.2 to 21.7 FPS**:

| Work per frame | Before | After |
|---|---|---|
| Game thread | 42 ms | 38 ms |
| GPU | 41.9 ms | 38.6 ms |
| FIFO | about 35 ms | about 35 ms |

This is the gain of the batch, not evidence for a separate FPS gain from each part. The program's code also grew,
from **36.5 to 46.9 MB** in its code section, as the compiler inlined more functions.

Forced inlining of all common memory accessors had already failed: code grew from **42 to 80 MB**, while game-thread
time stayed about **46.9 ms** in that experiment. Bigger generated code was not automatically faster.

## 2026-10-26: remove the duplicated depth pass

Branchless index scanning and moving completed readback hashing out of a shared lock reduced FIFO time from
**35 to 31.4 ms**. The foyer reached **21.9 FPS with the panel**, about **24 FPS without it**. GPU time was still
about **38.1 ms**.

A diagnostic comparison disabled the split early-depth path. Early depth testing decides whether a pixel is hidden
before running its color shader. The compatibility path split some draws into two passes to preserve GameCube
depth and alpha behavior. In this scene that duplicated expensive work:

- GPU time fell from about **38.1 to 28.9 ms**, a **9.2 ms** reduction.
- Draw count fell from **1,016 to 634**.
- The second EFB pass fell from **13.9 to 7.4 ms**. The EFB is the GameCube's rendering buffer.

Disabling correct behavior was only a diagnostic. The actual solution merged eligible depth and color work into
one pass, preserving alpha behavior. The first implementation did nothing because it required one color target,
while the scene had two, including a normal buffer for lighting. Fixing multiple-target handling, alpha preservation
and inherited state made the optimization take effect.

The successful console comparison recorded **34.4 to 23.8 ms** of GPU work, with the second EFB pass **13.6 to
6.4 ms**. Eligible pipelines used the single-pass path, and alpha cutouts remained correct in the console check.

Unneeded EFB-to-RAM work was another cost. A test that removed readbacks requested only through cache invalidation
saved about **2.3 ms** on the game thread and **3.8 ms** on the GPU. It retained the displayed image and Game Boy
Horror behavior in the tested scenes. This is a game-specific decision; another GameCube game may read those
pixels on its CPU.

Two full-screen RGB565 conversions had been measured together at **4.99 ms per frame**. Their shaders were
specialized and redundant clears removed. The marker interval could include later encoding and copy work, so the
whole 4.99 ms cannot be attributed to shader arithmetic or claimed as the saving.

## 2026-10-26: the game thread becomes the limit

With GPU time down to **23.8 ms**, the game thread still took about **37 ms**. It could not meet a 33.3 ms frame
budget, regardless of further GPU savings.

The next work reduced guest instruction and GX-call overhead:

- a direct path for unquantized paired-single loads and stores;
- a native replacement for a frequently used object lookup;
- display-list validation by format version;
- batched matrix and lighting writes;
- grouped consecutive writes to the GX FIFO;
- fewer timer checks and less per-call timing instrumentation.

The first load/store and lookup batch reduced game-thread time from **37.1 to 35.9 ms**. That was useful but much
smaller than stack-sampling percentages had suggested. Later measurements reached **35.6 ms** and **24.1 FPS with
the panel**.

Timing GX functions clarified the remaining work: about **27,000 calls per frame** accounted for **14.5 ms** in one
measurement. But not all of that was useful CPU work. `GXCopyDisp` included about **3.5 ms of handoff waiting**;
it was not a 3.5 ms drawing calculation. Display lists ran about **412 times per frame**, at **2.44 ms** combined.

Grouped FIFO writes reduced repeated checking while preserving byte order, individual write widths, interrupts and
fallbacks. The documented PAL generation grouped **537 writes at 66 static sites**. Those are code-generation
counts, not calls per frame or an FPS measurement.

Removing an ineffective vertex-format size cache and restricting GX timing to diagnostic runs mattered too.
Per-call timing had cost about **2 ms** with the panel. The resulting foyer measurements were:

| Measurement | Result |
|---|---|
| Panel enabled | 26.7 FPS |
| Panel disabled | 28.6 FPS |
| Game / FIFO / GPU | 32.0 / 28.8 / 23.8 ms |

Buffered asynchronous logging, block index processing, and including Aurora and deko3d in the compiler's whole
program optimization then gave **29.5 FPS in the foyer**, **30 FPS in the corridor**, and **29.5-30.5 FPS in the
study**, without PGO. Door transitions no longer showed the reported long logging stalls in that run.

## 2026-10-26 to 2026-10-28: PGO, further CPU work and a clean baseline

[PGO](glossary.md#pgo) builds from a record of real execution. [LTO](glossary.md#lto) lets the compiler optimize
across source files during linking. They complement the renderer changes; they do not remove unnecessary GPU work
by themselves.

An early profile from about **10 minutes** of console play brought the foyer from **28.8 to about 30 FPS**. It did
not solve the heavier Master Bedroom, then around **20-22 FPS**, with about **34.5 ms** of GPU work in the noted
session. A later profile improved a wider gameplay route, but slow rooms still exposed game-thread work.

Profiling builds were unusually slow with atomic counter updates. Switching to single updates let them run well
enough to train; correction of inconsistent counters is enabled when using the profile. A separate crash during
slow loading was traced to competing allocations in a shared room heap. The sky loader reserved too much while
another thread loaded room models. A targeted limit on that allocation fixed it, including a deliberately slow
**9 FPS** load test. It was not solved by compiler overflow flags alone.

Further trials examined object searches, snapshots, register handling, function wrappers, normalization, static
guards and FIFO publication. Changes without a measurable gain were reverted. Function alignment at **64 bytes**
was retained. The final cleaned baseline on 2026-10-28 recorded **30.0 FPS in the foyer** and about **26.5 FPS in
the Master Bedroom**, before the final PGO/LTO build.

The final recorded training session ran from the start to the first miniboss, about **54 minutes**, producing
**214 counter files**, about **3.9 MB**. It is a short route through the game, not full-game coverage. The resulting
optimized build recorded:

| Scene | Result |
|---|---|
| Foyer | 30.0 FPS |
| Master Bedroom | 29.4 FPS average, up from about 26.5 |
| Master Bedroom game thread / GPU | 27.7 / 30.2 ms |
| Master Bedroom reported 1% low | 21.5 FPS |

A follow-up build recorded **29.7 FPS average** and **21.6 FPS 1% low** in the Master Bedroom. These results support
“about 30 FPS”; they do not support “never below 30”. With CPU time reduced, the GPU was the next limit again.

## 2026-10-30: docked 1080p exposes the GPU limit

The resolution selected at startup became **1920x1080 docked**, retaining **1280x720 handheld**. Docking or undocking
while playing does not recreate that choice; restart to select the other mode.

The recorded docked foyer test at stock clocks showed:

| Measure | Result |
|---|---|
| Frame rate | 30.0 FPS |
| Median frame time | 33.3 ms |
| Frames lasting two / three screen refreshes | 93% / 6% |
| GPU time | 30.46 ms |
| Game-thread time | 25.4 ms |
| FIFO time | 23.1 ms |

At 1080p the GPU has little room left inside the 33.3 ms budget. The notes describe this as about **2.5 ms of
margin**. A faster CPU path cannot fix a room whose GPU work exceeds that budget. These docked numbers are from
the recorded console test report; a full docked playthrough was not established.

## What was tried and dropped

| Experiment | Why it was dropped or restricted |
|---|---|
| More segment overlap as the only early fix | Did not remove the per-draw CPU bottleneck |
| FIFO on the fourth core | No useful FPS gain; not a normal application resource |
| Forced inlining of all memory accessors | Much larger code, no measured game-thread improvement |
| Reading the active color image for six-bit blending | Added about 11 ms of GPU work |
| Extra CPU overlap around frame handoff | One measured comparison worsened from 41.96 to 43.65 ms |
| Releasing the game CPU around menu/render work | Races with settings and graphics operations made it unsafe |
| Vertex-format size cache | Frequent real format changes made snapshot work worse, 1.19 to 1.64 ms |
| Replacing game memory routines based only on sampling | They were not a demonstrated timing bottleneck |
| Merging all six-bit quantization passes | Would change intermediate rounding and the image |
| Lower handheld resolution | Kept 1280x720 as the visual target |

## Startup compilation is a separate performance story

Preparing **6,110 pipeline requests** does not mean compiling 6,110 different shaders: many requests share a shader.
Later cold-cache console runs compiled about **2,028-2,051 shaders in 772-778 seconds**, using three preparation
threads. That is roughly **13 minutes**, once, if the matching cache is missing.

The installer now builds those caches in the browser. Normal first launch therefore loads prepared shaders instead
of repeating that work on the Switch. The console fallback remains, with a real shader count and an estimated time
left. See [shaders.md](shaders.md) and [installer.md](installer.md).

The lasting lesson is to identify the current limit again after each gain. This port successively hit artificial
GPU work, CPU encoding cost, submission waits, duplicated GPU passes, the game thread, and finally the 1080p GPU
budget. No single compiler flag or thread-priority change explains the result.
