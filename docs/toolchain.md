# From the disc program to a Switch NRO

The toolchain turns source and the owner's executable into a Switch program. LM-NX translates PowerPC code to
C++, then compiles it for the Switch's ARM processor. For commands, see [building.md](building.md); for the shared
translator design, see [RECOMPILER.md](RECOMPILER.md).

## The compilation path

1. `tools/prepare_glme01.py` reads `main.dol` from a compatible disc and verifies its SHA-1.
2. The host `lmrecompile` uses the program, symbols, function splits and hook declarations to emit C++.
3. The generated code links statically with the runtime and high-level GameCube service replacements.
4. devkitA64 compiles for Horizon. Packaging tools add homebrew metadata and produce the NRO.

This is ahead-of-time compilation, or AOT: instructions are translated before play. The service layer uses HLE,
high-level emulation, to provide GameCube functions. A native renderer does not make that service layer disappear.
Extracted executables and generated game code remain local build outputs, not public documentation examples.

## Calls, registers and memory

Known destinations use direct C++ calls. Unknown destinations use runtime dispatch, and hooks retain their public
entry points. LM-NX already had direct calls; a separate global direct-call rewrite was not the missing optimization.

Registers are a processor's working slots. The runtime represents them in a context structure. Local-register
optimizations keep values in C++ locals so the ARM compiler can use hardware registers. Calls, interrupts and hooks
can observe the context, so values must be synchronized at the required boundaries. Ordinary function-call rules
alone do not describe all those observers.

The common Switch memory path uses a fixed backing array and a cheap ordinary-RAM check. Special addresses still
use hardware-register and locked-cache handling. GameCube byte order is big endian; ARM is little endian. Compiler
byte-swap builtins preserve that contract while allowing appropriate ARM instructions.

Gekko paired-single instructions operate on pairs of values and may use quantized loads and stores. Direct paths
for ordinary floating-point data helped, but replacing every operation with vector math would require proof about
rounding, exceptional values and quantization. The build keeps `-ffp-contract=off` and avoids fast-math.

## Grouping FIFO writes

The game sends many small writes to the GX command port. The translator groups safe consecutive writes, reducing
repeated waiting and recording checks. It preserves values, widths, order and byte order. Grouping does not cross
an intervening read, call, branch or interrupt polling boundary.

The documented European generation grouped **537 writes at 66 static sites**. Differential checks compare complete
contexts and emitted events. Static counts do not measure calls per frame or establish a separate FPS gain.

## Compiler settings and LTO

The direct build targets ARMv8-A and tunes for Cortex-A57. It uses `-O2`, per-function/data sections, signed wrapping
semantics with `-fwrapv`, and **64-byte function alignment**. See
[tools/build_switch_direct.py](../tools/build_switch_direct.py).

[LTO](glossary.md#lto), link-time optimization, lets the compiler optimize across separately compiled files at
linking. [tools/switch_optimization.py](../tools/switch_optimization.py) includes game, runtime, platform, Aurora
and deko3d groups. The direct Windows path uses serial link transformations, avoiding a dependency on MSYS make for
parallel LTO. `--sin-lto` provides a reference build.

Optimization can expose undefined behavior or change timing enough to reveal a race. The signed-overflow option
did not solve the null-material loading crash: that needed the sky-heap reservation fix. See
[performance-history.md](performance-history.md).

## PGO on the console

[PGO](glossary.md#pgo), profile-guided optimization, has two builds:

1. **Generate:** record branch and function activity during actual play.
2. **Use:** rebuild with those recordings so the compiler can favor common paths and code layout.

The runtime redirects counter writes to `LM-NX/pgo/` on the SD card, periodically and on orderly exit. Copy the
`.gcda` files back unchanged. Keep the identity file and the same generated sources, source layout, compiler and
LTO configuration.

Implementation details that matter:

- `-fno-profile-values` avoids the value-profiling path that does not fit the Horizon thread-local setup used here.
- Generation uses `-fprofile-update=single`. Atomic updates were expensive on Cortex-A57 and slowed training badly.
  Shared updates can lose increments; the use build enables `-fprofile-correction`.
- `-fprofile-partial-training` allows for functions absent from the route. It does not make that route full-game
  coverage.
- Identity checks cover source hashes, compiler version, architecture and LTO state.
- `--pgo-perfil-anterior` explicitly relaxes source-hash matching while retaining the other checks. Changed functions
  may lose their profile; old recordings are not assumed to fit every update.

The final training covered about **54 minutes**, from the beginning to the first miniboss: **214 counter files**,
about **3.9 MB**. This is a short route through the game, not a complete playthrough. The resulting PGO/LTO build
improved the Master Bedroom from about **26.5 to 29.4 FPS average**; the reported 1% low remained **21.5 FPS**.

## Verify semantics and execution separately

Code-generation comparisons run baseline and optimized output on matching states and compare contexts and events.
They help check register and memory transformations. They do not exercise the console GPU, prove working input,
or measure gameplay FPS.

Keep separate evidence for generation, compilation, linkage, startup, image correctness, audio and performance.
Save the ELF matching the tested NRO. See [measuring.md](measuring.md) for measurement overhead and interpretation.
