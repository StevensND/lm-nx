# Third-party notices

`forge/shader_forge.wasm` is the shader compiler of the page. It is built from `tools/shader_forge.cpp` of the
[lm-nx](https://github.com/StevensND/lm-nx) repository with `tools/build_shader_forge.py --web`, and contains:

| Component | What it does there | License |
|---|---|---|
| [Aurora](https://github.com/encounter/aurora), with the changes of lm-nx | Turns each graphics state of the game into a WGSL shader | MIT |
| Tint, from [Dawn](https://dawn.googlesource.com/dawn) (commit `1155e0ed`) | Translates WGSL to GLSL | BSD-3-Clause |
| [Abseil](https://github.com/abseil/abseil-cpp) 20240722.0 | Used by Tint and Aurora | Apache-2.0 |
| [uam](https://github.com/devkitPro/uam) by fincs, with the Mesa code it imports | Compiles GLSL to the Switch GPU's shader format (DKSH) | zlib; Mesa: MIT |
| [Zstandard](https://github.com/facebook/zstd) 1.5.7 (decoder only) | Reads the pipeline list | BSD-3-Clause |
| The shader cache code of lm-nx (LMNX-Deko3D) | Writes the cache files the game reads | Same as lm-nx |

`lib/codecs.wasm` holds the decompressors that read compressed disc images (RVZ, WIA, GCZ). It is built from
`tools/disc_codecs_web.c` of the [lm-nx](https://github.com/StevensND/lm-nx) repository with
`tools/build_disc_codecs_web.py`, and contains:

| Component | What it does there | License |
|---|---|---|
| [Zstandard](https://github.com/facebook/zstd) 1.5.7 (decoder only) | Reads RVZ images compressed with zstd | BSD-3-Clause |
| [LZMA SDK](https://www.7-zip.org/sdk.html) 26.05 by Igor Pavlov (LZMA and LZMA2 decoders) | Reads RVZ and WIA images compressed with LZMA or LZMA2 | Public domain |
| [bzip2](https://sourceware.org/bzip2/) 1.0.8 by Julian Seward | Reads RVZ and WIA images compressed with bzip2 | bzip2 license (BSD-style) |

`lib/disc_image.js` reads those formats, which come from the [Dolphin](https://dolphin-emu.org) emulator. It is
written for this page from the formats' descriptions and contains no code from Dolphin.

`fonts/Delfino.ttf`, the font of the page title, is Delfino by PFRA (2009), a fan-made font in the style of
Nintendo's lettering.

The licenses of the software inside the package (the NRO and its libraries) and the source code are at
https://github.com/StevensND/lm-nx.
