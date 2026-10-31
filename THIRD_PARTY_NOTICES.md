# Third-party notices

LM-NX is MIT licensed. Imported files retain their own licenses and copyright notices. This table identifies code, build dependencies and installer components; paths under `extern/fuentes/` are local sources supplied by builders.

| Source / authors | Path or use | License |
|---|---|---|
| [lm-decomp](https://github.com/SomeJakeGuy/lm-decomp), SomeJakeGuy and contributors, revision `c347326b12c38281c5528d36bda6334997fa8d3c` | `decomp/` | CC0-1.0; `decomp/LICENSE` |
| [Dusklight](https://github.com/TwilitRealm/dusklight), TwilitRealm, adapted from `2c4960dcdfaedf9b0c96e2410a1cca2bc1657131` | `libs/JSystem/`, `include/global.h`, `include/helpers/`, `src/port/os/OS{Thread,Mutex,Context}.cpp` | CC0-1.0 |
| [Aurora](https://github.com/encounter/aurora), encounter and contributors | Vendored in `extern/aurora/`; LM-NX patches in `patches/aurora/` | MIT; `extern/aurora/LICENSE` |
| [SwitchWaker](https://github.com/centollOS/SwitchWaker), Pulpparty, depende3000 and contributors; [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) contributors | Adapted SDL3 shim in `platforms/switch/sdl3_shim/`, disc reader in `platforms/switch/nod/`, translator in `LMNX-Deko3D/third_party/switchwaker_translate/`, inherited shader compiler patches | MIT / MPL-2.0 per file. The root MIT license does not replace MPL-2.0 notices on SwitchWakerHD-derived files. See [license](THIRD_PARTY/SwitchWaker-LICENSE) and [upstream inventory](THIRD_PARTY/SwitchWaker-THIRD_PARTY.md). |
| banjo-nx; plume by renderbag / hedge-dev and contributors | Adapted backend in `LMNX-Deko3D/third_party/plume/`; PC tools can copy SDL3 and plume/Vulkan from a builder-supplied banjo-nx tree | MIT; `LMNX-Deko3D/third_party/plume/LICENSE`. Copied dependencies retain their own notices. |
| uam, fincs / devkitPro, with Banjo, SwitchWaker and SwitchWakerHD patches; Mesa contributors | `LMNX-Deko3D/third_party/uam/` | zlib for uam; MIT for imported Mesa parts; retain inherited per-file notices and `LICENSE` / `PATCHES.md` |
| deko3d, fincs / devkitPro | Linked from the devkitPro installation | zlib |
| xxHash, Yann Collet, 0.8.3 | `LMNX-Deko3D/third_party/xxhash/`, imported through banjo-nx | BSD-2-Clause; retained `LICENSE` |
| SDL contributors | SDL3 headers in `platforms/switch/sdl3_headers/` | zlib; retained `LICENSE.txt`. These are headers, not the SDL renderer. |
| libnx / devkitPro contributors | Installed Switch runtime and build tools | libnx: zlib. Toolchain components retain their individual licenses, including GCC/binutils GPL terms and applicable runtime exceptions. |
| TP-Aleks contributors | Adapted `cmake/SwitchToolchain.cmake` setup following Dusklight / TP-Aleks | Retain upstream attribution; the devkitPro platform module is supplied by the installed SDK. |
| [LLVM Project](https://llvm.org) | PC compiler tools in release packages, such as `llvm/bin/clang.exe` and `llvm/bin/lld-link.exe`; not vendored here | Apache-2.0 WITH LLVM-exception; `packaging/LLVM-LICENSE.TXT` |
| Dawn / Tint contributors | `patches/tint/`, Aurora data enums/descriptors and local `extern/fuentes/dawn-1155e0ed531126f33a1279afa029349651ca1c93/`; installer `forge/shader_forge.wasm` | BSD-3-Clause. Native enums use only the data schema; the Switch renderer does not use a Dawn device. |
| Abseil contributors, 20240722.0 | Local `extern/fuentes/abseil-cpp-20240722.0/`; Tint/Aurora and installer shader compiler | Apache-2.0 |
| Zstandard contributors, 1.5.7 | Local dependency; decoder in installer `forge/shader_forge.wasm` and `lib/codecs.wasm` | BSD-3-Clause |
| LZMA SDK, Igor Pavlov, 26.05 | Installer `lib/codecs.wasm`, LZMA/LZMA2 decoders | Public domain |
| bzip2, Julian Seward, 1.0.8 | Installer `lib/codecs.wasm` | bzip2 license (BSD-style) |
| LM-NX shader cache code | Installer `forge/shader_forge.wasm`, built from `tools/shader_forge.cpp` | MIT, same as LM-NX |
| Dolphin team | RVZ, WIA and GCZ format descriptions used by `installer/lib/disc_image.js` | The page's reader is written for LM-NX from format descriptions and contains no Dolphin code. |
| Delfino font, PFRA | `installer/fonts/Delfino.ttf`, `platforms/switch/fuentes/Delfino.ttf` | Fan-made font; no explicit license is stated in the supplied installer notice. |

The installer shader compiler combines Aurora's GX shader generation, Tint's WGSL-to-GLSL translation and uam's GLSL-to-DKSH compilation. Its disc codecs read compressed disc images. See [installer notices](installer/NOTICES.md) for the component breakdown.

Additional local libraries keep the licenses shipped with their sources. The Windows comparison path retains notices for volk, Vulkan-Headers and VulkanMemoryAllocator when copied with plume. SDK-only headers are taken from the builder's installation. SQLite and Abseil adaptations are made in build copies rather than the original supplied sources.

lm-decomp does not accept AI-assisted contributions. Contributions made with such assistance must not be submitted upstream to lm-decomp.

The repository contains no game data and no generated game code. Players supply their own disc. The NRO built from that disc contains the game's code translated to aarch64.
