# Using LM-NX as a guide for another GameCube port

This guide explains what can be reused and what needs new evidence. LM-NX combines rob646464's static recompilation,
Aurora's GameCube graphics support and a native Switch backend. It does not make a new game's executable, hooks,
audio behavior or resource use interchangeable with Luigi's Mansion.

Start with the [glossary](glossary.md) and [native-renderer.md](native-renderer.md). The retained
[PORTING.md](PORTING.md) describes the shared platform integration.

## 1. Establish the game's recompilation

Identify a supported executable and verify it by content. Find function boundaries, indirect calls, jump tables
and the GameCube services it expects. Get a working reference before introducing performance changes.

LM-NX's symbols, hooks, address maps and generated function names belong to specific Luigi's Mansion programs.
Copying those declarations does not translate another game. Keep any code derived from a user's executable local,
and check the licenses of the support code you adapt.

## 2. Define the runtime contract

The game needs more than translated CPU instructions. It expects memory mirrors, hardware registers, interrupts,
thread scheduling, DVD reads, save storage, controllers and sound. Decide which services your runtime provides
before substituting their implementations.

Preserve asynchronous request ordering. The thunder bug demonstrates the risk: releasing the guest CPU inside a
small disc request let a callback enter before the caller advanced its offset. Larger buffers would not fix that
ordering mistake. See [switch-audio.md](switch-audio.md).

## 3. Connect Aurora to a native renderer

The reusable design is an ordered packet of graphics operations with explicit resource ownership. The adapter
must cover resources, recording, pipeline preparation and callbacks, not only the final command executor.

Prove copies, vertex inputs, textures, depth and presentation with small original test patterns. Then bring up the
loading screen and menu before a full shader preparation. Do not treat visible menu text as proof that game-image
copies work. See [HowToDeko3d.md](HowToDeko3d.md).

Useful pieces to study here include:

- [LMNX-Deko3D/](../LMNX-Deko3D/): native objects, deko3d submission and cache formats;
- [platforms/switch/](../platforms/switch/): libnx startup, SDL adaptation, input, audio and logging;
- [tools/prepare_aurora_native.py](../tools/prepare_aurora_native.py): preparation of the native Aurora boundary;
- [tools/shader_forge.cpp](../tools/shader_forge.cpp): matching host/browser shader generation and compilation.

## 4. Prepare shaders and a visible fallback

Aurora produces WGSL; Tint translates it to GLSL; uam compiles it to DKSH. Keep stage identity, bindings and compiler
options consistent across every tool. Save compiled entries in a format the console validates.

Collect graphics configurations broadly enough for the new game and its options. Luigi's Mansion's pipeline list
cannot establish coverage for another title. Pipeline requests and unique compiled shaders are different counts.
If compilation is needed on the console, show progress, save completed work and explain the expected wait.

For browser preparation, each worker can own a compiler instance. The console's shared non-reentrant compiler
requires serialization. See [shaders.md](shaders.md) and [installer.md](installer.md).

## 5. Measure on real hardware

Use normal application mode and known stock clocks. The emulator missed an early real-console failure in LM-NX,
and desktop per-draw costs did not predict Switch costs. Measure game-thread CPU time, graphics-worker time, GPU
work and waits separately. See [measuring.md](measuring.md).

Pick a fixed scene and a gameplay route, including transitions and sound. Keep visual checks alongside FPS. Record
the exact executable, settings, cache state and tested areas. A benchmark without those conditions is hard to reuse.

## 6. Reduce work before adding concurrency

LM-NX benefited from batching draws, filtering snapshots, simplifying common memory access, grouping safe FIFO
writes, avoiding repeated state preparation and merging eligible depth/color passes. More in-flight segments helped
only with correct resource lifetime. A fourth-core experiment and forced inlining did not pay off.

Game-specific decisions need fresh proof. Another game may read EFB pixels that Luigi's Mansion does not, use alpha
or depth combinations outside the merged path, or depend on different stream timing. Preserve conservative fallbacks
until the new cases are understood. The dated evidence is in [performance-history.md](performance-history.md).

## 7. Train and distribute a coherent build

Use LTO and console-trained PGO after establishing a working baseline. Keep source and build identity stable between
training and use. A short route improves common paths; it does not represent every part of the game.

An installer should verify the user's program, prepare required derived data locally and package a matched NRO,
pipeline list and shader cache. Updates should preserve discs and saves. Check the package on the console after
the page deploys, not only the download's checksum.

Credit **rob646464**, **encounter**, **TwilitRealm**, **centollOS**, **TP-Aleks**, **fincs**, **devkitPro**,
**switchbrew**, the **Dolphin team** and **banjo-nx** as applicable to the code you reuse. Preserve the relevant
licenses and notices; see [the repository credits](../README.md#credits) and
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
