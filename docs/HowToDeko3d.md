# Building a deko3d renderer around Aurora

This is a practical guide to the approach used by LM-NX. Its Switch renderer started with deko3d. The useful lesson
for another GameCube port is how to connect Aurora's graphics behavior to native resources, then verify and reduce
the cost. Read [native-renderer.md](native-renderer.md) for the current architecture and
[deko3d-devlog.md](deko3d-devlog.md) for the dated results.

## What deko3d provides

[deko3d](glossary.md#deko3d) is a low-level graphics interface for the Switch. You allocate GPU memory, describe
images and buffers, record commands, submit work and wait for completion. It does not make resource lifetime or
GameCube compatibility decisions for you.

LM-NX's higher-level code still has objects called pipelines: shader selections and drawing state prepared for a
draw. These are the port's abstraction. They are not Vulkan pipeline objects inside a separate graphics driver.
uam compiles GLSL into DKSH, the shader binary format deko3d loads.

## 1. Prove the shader path before the game

The path here is **Aurora -> WGSL -> Tint -> GLSL -> uam -> DKSH**. WGSL and GLSL are shader languages. Tint translates
between them; uam produces code for the Switch GPU. Use the same patched generator, compiler options, resource
bindings and cache format on the build machine, in the browser and on the console.

Check a small set of vertex and fragment shaders first, then every configuration the game is expected to prepare.
Inspect translation errors and warnings. Confirm stage identity, buffer layout, texture bindings, coordinate
conventions and compiled-code validation. A fragment stage's number in one API need not equal its number in a DKSH
file.

Cache keys must include the translation and compiler identity. Compare full source and metadata as required by the
format, rather than trusting only a filename hash. A successful compile proves that a shader is accepted; it does
not prove that the resulting picture is right. See [shaders.md](shaders.md).

## 2. Build a small native graphics test

Before debugging thousands of game draws, test these operations with your own geometry and colors:

1. Clear a target and draw a triangle.
2. Upload and sample a small texture with different colors in each corner.
3. Copy a nonzero rectangle and verify its position and dimensions.
4. Draw overlapping geometry with depth testing.
5. Read color and depth back after a fence.
6. Present the result, then close and reopen the program.

Check the actual order of rectangle fields, whether a width is measured in pixels or bytes, and the coordinate
origin. LM-NX's initial zero-width EFB copies came from a wrong `RenderBox` field order. A visible menu did not
establish that the game images were being copied correctly.

## 3. Define the resource boundary

Aurora needs more than an executor for the final command list. Its recording code, resource types, callbacks,
frame packets and pipeline preparation all participate in the contract. Replace that boundary consistently.

LM-NX uses its own native resource objects in [LMNX-Deko3D/include/lmnx/](../LMNX-Deko3D/include/lmnx/).
A submitted packet retains what it uses. Upload storage and descriptor tables remain immutable while the GPU may
read them. Readback bytes become available only after completion. Do not hold a global lock while waiting for a
worker that may need the same lock.

Start with conservative ordering. Then identify individual redundant barriers, copies and state changes, checking
the picture after each reduction. Separate CPU recording time from time waiting for a GPU fence.

## 4. Bring up presentation and the settings menu early

The loading screen is part of the renderer test. Configure the surface before preparing the full shader set so
the user can see progress. Supply vertex attributes from the draw description, preserving the locations Tint
emits. LM-NX's first progress bar submitted work but displayed nothing because its inputs were constant.

Keep loading, compilation and gameplay counters separate. The menu is operated with **L3 + R3** in LM-NX; that is a
project control choice, not a deko3d requirement. Menu drawing must also preserve or invalidate any state remembered
by the game's drawing path.

## 5. Close the GameCube compatibility cases

At minimum, check EFB copies, reduced-precision blending, depth/alpha ordering, texture formats, lighting buffers,
shadows, fades and the final display copy. A feature that works with one render target may fail when lighting adds
another target. A full-screen conversion may safely omit a clear only if it writes every required pixel and channel.

Keep a PC reference for shared logic. Compare images by pass and purpose, not by a dump's ordinal number. Two
backends can split the same work into different numbers of passes. Always finish with a real-console visual check.

## 6. Make the CPU path small

The early generic resource path was much too expensive per draw. LM-NX added a direct batch encoder that prepares
state once for a pass and emits its draws together. The useful order of work was:

- reduce per-call checks and repeated resource preparation;
- reuse unchanged texture tables and bindings;
- snapshot only guest data that must survive asynchronous consumption;
- group safe consecutive FIFO writes;
- let enough command segments remain in flight to overlap recording and execution;
- measure again before changing thread placement or priorities.

More overlap needs more retained data. Faster recording is no improvement if it overwrites an upload still in use.
See [platform-notes.md](platform-notes.md) for thread ownership and memory rules.

## 7. Remove GPU work while preserving the picture

The largest late GPU gain was merging eligible early-depth draws: **34.4 to 23.8 ms** in the measured scene.
Reducing barriers alone had not produced that result. Count how often an optimization actually acts, inspect
fallback reasons, and compare the affected image features.

Do not assume every full-screen pass is interchangeable. Six-bit rounding between passes can be observable.
Do not sample an active destination merely to avoid a copy without establishing legal, correct feedback behavior.
At docked 1080p the measured foyer GPU time was already about **30.5 ms**, so pixel work remains important.

## 8. Deliver matching caches

The installer compiles shaders in WebAssembly and writes the exact translation and DKSH package formats used by
the console. Each browser worker owns its compiler instance. On the console, three pipeline-preparation threads
share a serialized uam worker because that compiler is not reentrant.

A missing cache still has a visible fallback. Count real shaders rather than pipeline requests, estimate the time
left honestly, save the results and resume with saved entries on later starts. See [installer.md](installer.md).

## References and credit

The implementation adapts work from **centollOS** (SwitchWaker and SwitchWakerHD) and **banjo-nx**, with **Aurora**
by **encounter**. deko3d and uam come from **fincs** and **devkitPro**; **switchbrew** provides libnx. The
[repository credits](../README.md#credits) and [third-party notices](../THIRD_PARTY_NOTICES.md) identify the sources
and their licenses.
