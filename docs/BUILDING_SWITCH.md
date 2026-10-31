# Building for Switch

The direct build is driven by `tools/build_switch_direct.py`. The current script expects Windows-style devkitPro executables and paths: install devkitA64, libnx, deko3d and the required Switch portlibs, set `DEVKITPRO` to that installation and pass `--devkitpro` when selecting a different location. Tools default to `DEVKITPRO`, with `/opt/devkitpro` as a fallback. A working non-Windows invocation has not been established from these scripts.

Use Python with Pillow for the embedded loading image, a host C++ compiler for the recompiler/Tint tools, and native Git for applying patches to Aurora build copies (`GIT` can select its executable; otherwise it is found on `PATH`). The VI table conversion also calls `ffmpeg` and devkitPro's `zstd.exe`. No dependencies are downloaded by the direct tools.

Supply local dependency sources under `extern/fuentes/`. `tools/local_dependencies.py` defines the exact directory names and is the version reference:

| Dependency | Version / revision |
|---|---|
| Abseil | 20240722.0 |
| fmt | 12.1.0 |
| ImGui | 1.91.9b-docking |
| Dawn / Tint | 1155e0ed531126f33a1279afa029349651ca1c93 |
| SPIRV-Headers | 942fe4b988359a0750b79f0ae7ed735994d3147d |
| SPIRV-Tools | 5b5a25231a3543559b4f2cb27fe725a08b76b5d0 |
| glslang | 90afccfbd49dff0349d86a41762e9de24e1df811 |
| xxHash | 0.8.3 |
| zlib-ng | 2.3.3 |
| libpng | 1.6.58 |
| FreeType | 2.14.3 |
| Zstandard | 1.5.7 |
| LZMA SDK | 26.05 |
| bzip2 | 1.0.8 |
| RmlUi | f00a0fee38391b2f927114e11bea18dc0a7dba1e |
| SQLite amalgamation | 3510300 |

This is the shared dependency inventory, not a claim that every component links into the NRO. Native Switch rendering uses deko3d; Tint is used as a shader translator. The PC comparison tool additionally expects Tracy 0.14.1 and builder-supplied banjo-nx, Vulkan SDK and host compiler tools; use `LMNX_BANJO_NX`, `LMNX_MINGW_BIN`, `LMNX_MAKE`, `CMAKE` and `VULKAN_SDK` as documented in `tools/build_pc.py`.

## Game code and NRO

`tools/prepare_glme01.py` reads the player's uncompressed USA GLME01 revision 0 ISO, checks the DOL SHA-1 and invokes the host recompiler with `decomp/config/` symbols, splits and hooks. It writes generated C++ under `build/`. Optional GLMP01 revision 0 sources use a separate namespace. The repository does not include these generated sources or game data.

After building the host recompiler and Tint library, the direct game build has this form (replace the paths with your local paths):

```powershell
py tools/prepare_glme01.py --iso orig/GLME01.iso --recompiler build/host-recompiler/lmrecompile.exe
py tools/build_switch_direct.py --devkitpro <devkitPro-path> --productor-nativo --generados build/glme01-generado
```

These commands alone do not prepare every prerequisite. `tools/build_tint_direct.py --switch` builds the Tint library; Aurora preparation copies and patches `extern/aurora/` into `build/`. The direct script compiles AOT, HLE/runtime, Aurora, the SDL shim and LMNX-Deko3D/uam, links against libnx/deko3d, then uses `nacptool` and `elf2nro`. Output is `build/direct-switch/ProjectLM-NX.nro`; the installer packages it as `LM-NX.nro`. Without `--generados`, the script produces diagnostics rather than the game.

`tools/build_game_switch.py` is a broader wrapper that also prepares disc overlays, builds the host recompiler, checks generated code and builds two PC comparison paths before calling the direct script. Its extra PC prerequisites are required if using that wrapper. These scripts have not been run as part of this documentation cleanup.

## PGO

LTO is enabled by default. `--pgo GENERATE` instruments selected runtime, AOT, Aurora and renderer groups. Train on the actual Switch; runtime code redirects GCC profile writes to the SD card. Copy the resulting `.gcda` files into the matching host profile directory selected by `--pgo-dir` (default `build/pgo-switch`), retaining the `identidad.json` created during instrumentation. `--pgo USE` checks the source, compiler, architecture and LTO identity before using that profile. PC training is rejected.

`tools/build_switch_direct.py --pgo-perfil-anterior` permits changed source hashes while still checking compiler and other settings; GCC can discard coverage for changed functions. Keep function, file and symbol names stable when reusing profiles. The export does not establish whether a matching trained profile is available; a normal build can use `--pgo OFF`.
