# deko3d development log

This is the renderer's development history through the public release on **2026-10-31**. Newer entries are at the
bottom. It separates code and build checks from actual console observations. For the performance measurements in
context, see [performance-history.md](performance-history.md); for the current design, see
[native-renderer.md](native-renderer.md).

## 2026-10-21: native resource boundary and first image

The starting point was rob646464's recompilation and Aurora's GX support. The Switch path connected native resource
objects and ordered frame packets to the plume deko3d backend adapted from banjo-nx. Tint and uam provided the
shader path. The final Switch renderer did not create a Dawn device.

Early checks covered memory suballocation, command-list segmentation, shader-cache validation, EFB packing and
audio conversion. These were build or host checks, not proof of GPU execution on Switch.

The first image failures exposed several contracts:

- `RenderBox` fields were left, top, right, bottom, front and back. Misreading them produced zero-width copies.
- Unused trailing attachment slots had to remain unused, rather than become invalid render targets.
- Immediate shader data belonged at uniform binding **15**. The inherited backend convention used another slot.
- Submitted passes had to retain their resources without retaining the command encoder in a circular ownership chain.

Correcting the rectangle order restored the game image in the PC comparison and emulator observations. That did
not establish stability or speed on the real Switch.

## 2026-10-22: real hardware exposes long waits

The first console log prepared **6,110 pipeline requests in 163.13 seconds** on one thread with cached DKSH.
After entering the game, the screen stayed black and **337 fence warnings** reported waits longer than two seconds.
There were only about **0.46 presentations/s**.

The emulator did not reproduce the same failure. Queue size, memory visibility and synchronization were examined,
but the eventual keep-warm finding was more useful than assuming the largest new allocation caused the fault.
The visible fact at this point was a failed console run, not a playable low-FPS game.

## 2026-10-23: loading bar, keep-warm and batch encoding

Aurora's artificial GPU keep-warm passes were disabled on Switch. The work repeatedly filled a 64 MiB buffer and
had no useful role in drawing this game on the console.

The invisible loading bar was traced separately to missing vertex attributes in the native pipeline adapter.
Its draws were submitted, but their input positions collapsed. The adapter now derives the attribute table from
the vertex description and retains Tint's locations.

A cold-cache console test with the visible bar took **946.02 seconds**, still on one preparation thread. Game
presentation afterwards remained about **0.81-0.84 images/s**. Increasing in-flight segments alone had not solved
the CPU cost of encoding every draw through the shared abstraction.

A direct batch encoder was introduced. Instead of rebuilding resource and command state around each operation,
it prepares a pass and records its draw list together. SaltyNX reporting was connected to presentation so console
overlays could receive frame counts. Emulator improvements during this work were treated as diagnosis, not as
console benchmarks.

## 2026-10-24: platform integration and cache loading

The shared runtime fixes for malformed FIFO recovery, loader-thread GX ordering, memory guards and shader-cache
survival after interrupted sessions were integrated. Switch-specific startup, audio and input were retained.

The platform fixes included safe host staging for file reads, save replacement on the SD filesystem, native menu
input and packed shader-cache storage. Warm-cache startup reached about **3.6 seconds** in the recorded notes.
Cold compilation remained a separate long operation.

Broad barriers, descriptor invalidations and growing texture-object tables became the next renderer targets.
The GPU could still take **42-66 ms** per frame. A feedback-based six-bit blending experiment added about **11 ms**
and was rejected. Handheld resolution stayed at 1280x720.

## 2026-10-25: more overlap, fewer snapshots and cheaper state

Allowing **32 rather than 4** command segments in flight raised the measured frame rate from **15.7 to 17.7 FPS**,
with safe upload mapping preserved. Moving FIFO work to the fourth core did not earn a useful gain and was dropped
as a default.

Filtered snapshots reduced preserved guest data from **1.68 MB to 378 KiB per frame**, and snapshot time from
**2.1 to 1.39 ms**. The foyer improved from **17.9 to 19.2 FPS**.

Alternating blend surfaces, cheaper recording-state checks and simpler common memory access then produced
**21.7 FPS**, with game-thread time about **38 ms** and GPU time **38.6 ms**. These were combined changes, so their
individual contributions were not measured separately.

Forced inlining of all memory helpers was discarded: it nearly doubled code size in that test without improving
the game-thread time. The diagnostic panel also needed attention because allocator-statistics queries had added
about **10 ms** in the handoff path.

## 2026-10-26: single-pass depth and alpha behavior

The key GPU experiment compared the split early-depth route with it disabled. GPU time dropped by **9.2 ms** and
draws fell from **1,016 to 634**. This identified duplicated work, but turning correctness off was not the solution.

The first one-pass implementation did not activate in the main scene: it expected one color target, while the
scene used two. Multiple-target handling, alpha preservation and inherited pipeline state were corrected. The
successful console comparison reduced GPU time from **34.4 to 23.8 ms**, while alpha cutouts remained correct.

Readbacks requested only through cache invalidation were removed from the normal Switch route after tests of the
displayed image and Game Boy Horror. RGB565 copy shaders were specialized by filtering and quantization mode, and
redundant clears were removed. The measured **4.99 ms** for two conversion intervals was not all shader cost:
later work without a separate marker could be charged to those intervals.

## 2026-10-26: CPU work and compiler coverage

Index scanning, recording notifications, matrices, lights and grouped FIFO writes reduced the producer's overhead.
Timing every GX call was restricted to diagnostics; it had itself cost about **2 ms** with the panel.

The foyer reached **28.6 FPS with the panel off**. Buffered logging and broader LTO coverage over Aurora and deko3d
then gave **29.5 FPS without PGO**. A small console profile could reach about 30 in the foyer, while heavier rooms
still needed CPU work.

Some attempts at extra overlap introduced races with graphics and menu work; another measured overlap path worsened
frame time from **41.96 to 43.65 ms**. Those paths were not retained merely because they used more concurrency.

## 2026-10-27 to 2026-10-28: correctness under slow loading, then PGO

A crash seen in slow instrumented builds came from room and sky allocations sharing a solid heap. The sky reserved
too much before room loading finished. A targeted limit fixed it, including a deliberately slow **9 FPS** load.
The earlier signed-overflow compiler option had not solved that allocation race.

After CPU experiments and removal of changes without measured gains, the clean baseline reached **30.0 FPS in the
foyer** and about **26.5 FPS in the Master Bedroom**. A new console profile, covering about **54 minutes** from the
start to the first miniboss, was used with LTO. The Master Bedroom then averaged **29.4 FPS**, with GPU **30.2 ms**
and game-thread time **27.7 ms**. The recorded 1% low was **21.5 FPS**.

Visual work also addressed Game Boy Horror map state and a foyer pillar overlap visible in widescreen. The pillar
issue was geometry sharing a depth plane, not a shadow pass failure. These fixes belong to correctness, not to the
FPS gains above.

## 2026-10-30: browser caches and docked 1080p

The shader forge combined the native Aurora generator, Tint and patched uam for both host and WebAssembly builds.
The resulting caches matched the console's output byte for byte in the recorded checks. One browser test with
**11 workers** completed in about **20 seconds**; that is a particular computer's result, not a requirement or a
promise for every browser.

Docked startup selected 1920x1080. The foyer ran at **30.0 FPS**, GPU **30.46 ms**, game **25.4 ms**, FIFO **23.1 ms**.
The image was reported correct. The GPU now had little margin; a full-game docked performance guarantee was not
established. Resolution selection remains a startup decision.

## 2026-10-31: release startup and audio closure

The installer and game settled on `sdmc:/switch/LM-NX/` and `LM-NX.nro`. Shader compilation on the console remained
as a fallback when browser-built caches were missing or unusable. Recorded full builds compiled about
**2,028-2,051 shaders in 772-778 seconds**, using three preparation threads for the pipeline work.

The bar was changed to count stored shaders rather than graphics-state requests. It shows **“Compiling shaders...
N of about 2050”**, a percentage and estimated minutes left. The total is deliberately approximate because
different graphics states share compiled shaders. See [shaders.md](shaders.md).

The thunder crackling was fixed by preserving guest scheduling order around small asynchronous disc requests.
Repeated console loads confirmed the result; temporary audio recording and trace tools were removed. The full
investigation is in [switch-audio.md](switch-audio.md#the-thunder-crackling-investigation-2026-10-30-to-2026-10-31).

The public release uses one NRO, matching cache identities and the browser installer. The measured rooms run close
to the original 30 FPS target at stock clocks. The history still records slower frames and the limited training
route, rather than treating a successful release as proof of every room and option.
