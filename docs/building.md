# Building the Switch port

Players can use the [installer page](https://stevensnd.github.io/lm-nx/). This guide is for building the Switch
program, an [NRO](glossary.md#nro), from source and your own compatible disc. These commands follow the current
scripts; this documentation pass did not perform a clean build.

The direct scripts currently expect Windows executables from devkitPro. A working non-Windows invocation has not
been established for this path. Retained desktop instructions describe other targets.

## 1. Prepare tools and local sources

Install devkitA64, libnx, deko3d and the required Switch portlibs from [devkitPro](https://devkitpro.org/).
You also need Python with Pillow, CMake, a host C++ compiler and native Git for applying patches to Aurora build
copies. The video-interface table conversion invokes `ffmpeg` and devkitPro's `zstd.exe`.

Set `DEVKITPRO` to your installation. The direct script accepts `--devkitpro` too, but helper scripts independently
use the environment variable. `GIT` and `CMAKE` can select those tools. The host recompiler example below uses the
MinGW compiler from devkitPro's MSYS2 installation. The top-level `tools/build.ps1` is a desktop preset helper,
not the direct Switch build command.

Supply complete dependency sources under `extern/fuentes/`, using the directory names in
[tools/local_dependencies.py](../tools/local_dependencies.py). The direct tools do not download missing sources.

| Dependency | Version or revision |
|---|---|
| Abseil | 20240722.0 |
| fmt | 12.1.0 |
| ImGui | 1.91.9b-docking |
| Dawn / Tint | `1155e0ed531126f33a1279afa029349651ca1c93` |
| SPIRV-Headers | `942fe4b988359a0750b79f0ae7ed735994d3147d` |
| SPIRV-Tools | `5b5a25231a3543559b4f2cb27fe725a08b76b5d0` |
| glslang | `90afccfbd49dff0349d86a41762e9de24e1df811` |
| xxHash | 0.8.3 |
| zlib-ng | 2.3.3 |
| libpng | 1.6.58 |
| FreeType | 2.14.3 |
| Zstandard | 1.5.7 |
| LZMA SDK | 26.05 |
| bzip2 | 1.0.8 |
| RmlUi | `f00a0fee38391b2f927114e11bea18dc0a7dba1e` |
| SQLite amalgamation | 3510300 |

This is the shared source inventory, not a list of libraries all linked into the NRO. Switch rendering uses
deko3d; Tint is taken from Dawn without a Dawn rendering device. Keep local sources and game-derived outputs out
of a public source package.

> [!NOTE]
> Some helpers retain installation-specific assumptions, including a libnx include location in the Tint build.
> Inspect the compiler command and include paths if your installation differs. A top-level path option does not
> establish that every helper is portable.

## 2. Build the host recompiler

Run from the repository root in PowerShell, with `DEVKITPRO` set. This program runs on your computer, rather than
on the Switch, and translates the GameCube executable to C++.

```powershell
New-Item -ItemType Directory -Force build/host-recompiler
$hostCompiler = Join-Path $env:DEVKITPRO 'msys2/mingw64/bin/g++.exe'
& $hostCompiler -std=c++20 -O2 -static -Isrc `
  src/recompiler/recompiler.cpp tools/recomp/lmrecompile.cpp `
  -o build/host-recompiler/lmrecompile.exe
```

Put the host compiler's binary directory on `PATH` if supporting programs are not found. The broader wrapper below
sets up its own host build environment.

## 3. Generate the game code locally

Supply an uncompressed USA ISO as `orig/GLME01.iso`. The tool checks its header and `main.dol` SHA-1 before generation:

```powershell
py tools/prepare_glme01.py --iso orig/GLME01.iso `
  --recompiler build/host-recompiler/lmrecompile.exe
```

The result is `build/glme01-generado/`, including C++, a local DOL and identification files. Generated output must
stay under `build/`. The option and output names are retained from the scripts.

For an optional European module, use your `GLMP01` revision 0 ISO:

```powershell
py tools/prepare_glme01.py --iso orig/GLMP01.iso --version GLMP01 `
  --recompiler build/host-recompiler/lmrecompile.exe --salida build/glmp01-generado
```

That module uses a separate C++ namespace. One NRO can include both modules. The installer separately builds the
Hidden Mansion data from both discs; see [editions.md](editions.md).

For compiler experiments, `--locales-enteros` enables conservative integer-local optimization and
`--locales-todos` enables broader general-register locals. Keep a baseline generation and run the relevant
code-generation comparisons. Successful generation alone proves neither correctness nor frame rate.

## 4. Build Tint for the Switch

```powershell
py tools/build_tint_direct.py --switch --jobs 3
```

This produces `build/tint-switch/liblmnx_tint.a`: the WGSL reader, inspector, GLSL writer and translation support.
It evaluates the pinned source manifests and local Tint preparation without building a Dawn graphics backend.

The direct NRO build invokes Aurora preparation. It copies and patches local sources under `build/`, connecting
native resource types and Switch adaptations. Edit the preparation inputs, not an already generated build copy,
when making a reproducible renderer change.

## 5. Build and package the NRO

```powershell
py tools/build_switch_direct.py --productor-nativo `
  --generados build/glme01-generado --jobs 3 --pgo OFF
```

Add `--generados-pal build/glmp01-generado` if that module was prepared. The script compiles AOT, runtime, HLE,
Aurora, the SDL compatibility layer, deko3d and uam. It links against libnx/deko3d, then packages with `nacptool`
and `elf2nro`. Without `--generados`, it produces diagnostics rather than the game.

The current direct output is `build/direct-switch/ProjectLM-NX.nro`. Install it as **`LM-NX.nro`** under
`sdmc:/switch/LM-NX/`. The historical build-output name does not change the public installation layout.
Keep its matching unstripped ELF for resolving crash addresses.

LTO is enabled by default; `--sin-lto` disables it for a reference comparison. PGO is off unless selected.
See [toolchain.md](toolchain.md) for the compiler settings and their limits.

### The broader wrapper

[tools/build_game_switch.py](../tools/build_game_switch.py) also prepares local overlays, builds the host
recompiler, checks generated code and builds two PC comparison paths before calling the direct script:

```powershell
py tools/build_game_switch.py --iso orig/GLME01.iso --pal orig/GLMP01.iso --jobs 3
```

Omit `--pal` for USA only. This is not a lighter command: its PC prerequisites include builder-supplied banjo-nx,
host tools, a Vulkan SDK and Tracy 0.14.1. See [tools/build_pc.py](../tools/build_pc.py) for `LMNX_BANJO_NX`,
`LMNX_MINGW_BIN`, `LMNX_MAKE`, `CMAKE` and `VULKAN_SDK`.

## 6. Prepare shaders and installation data

The [installer](installer.md) packages the released NRO, pipeline list, matching shaders and the player's discs.
A changed local renderer can have a different shader identity; do not assume the released forge matches it.

Rebuild the forge with `py tools/build_shader_forge.py`; add `--web` for WebAssembly and provide Emscripten for that
target. Compressed-disc tools use `tools/build_disc_codecs_web.py`. See [shaders.md](shaders.md) for cache formats
and [installer.md](installer.md) for synchronized manifest and deployment inputs.

The console can compile missing shaders as a fallback. Test both a warm-cache start and missing-cache preparation.
Successful linkage does not establish a correct image, working sound or performance on the console.

## Recording and using PGO

From a working build, keep sources, generated code and compiler settings fixed:

```powershell
py tools/build_switch_direct.py --productor-nativo `
  --generados build/glme01-generado --pgo GENERATE --pgo-dir build/pgo-switch --jobs 3
```

Play on the Switch. The instrumented runtime saves GCC `.gcda` counters under `LM-NX/pgo/` on the SD card,
periodically and during orderly exit. Use the game's quit command to finish; closing from HOME does not guarantee
a final dump.

Copy those files back to the selected host profile directory, retaining names and the instrumentation build's
`identidad.json`. Rebuild with identical options and `--pgo USE` instead of `--pgo GENERATE`. The script checks
source, compiler, architecture and LTO identity and rejects missing or incompatible profiles. It rejects PC training.
See [toolchain.md](toolchain.md#pgo-on-the-console).

A fresh public checkout is not guaranteed to match a profile from another source layout. Use `--pgo OFF` or train
a matching profile. The explicit `--pgo-perfil-anterior` option permits changed source hashes while retaining other
identity checks; GCC can discard coverage for changed functions.
