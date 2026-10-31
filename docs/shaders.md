# Shaders, caches and first startup

A shader is a small program that runs on the GPU. LM-NX needs about **2,000 compiled shaders** for the known
graphics configurations. The [installer page](https://stevensnd.github.io/lm-nx/) builds them on your computer
and puts them in the installation zip. With matching caches, the Switch loads them in seconds.

## For players

Use **Create lm-nx.zip** for a new installation, or **Create lm-nx-update.zip** to update. Both include shaders that
match the NRO. Copy the complete `LM-NX` folder into `sdmc:/switch/`.

The two cache files look like this:

```text
LM-NX/cache/
  lmnx-tint-<id>/paquete.lmp
  lmnx-f447-gxabi2-<id>/paquete.lmp
```

`<id>` identifies the shader-related code of that build. The first package stores translations; the second stores
compiled GPU programs. Keep the folder names unchanged. Renaming an old folder to a new identity does not make its
contents compatible.

The console can compile missing shaders itself. This happens if the cache is missing, damaged or does not match
the game, or if a changed graphics option needs variants that are not already stored. Updating only the NRO by hand
can also leave it without its matching shaders. The update zip avoids that mismatch.

### The console fallback

A full cold-cache preparation was recorded at **2,028-2,051 compiled shaders**, for about **6,110 pipeline
requests**, in **772-778 seconds** with three preparation threads. That is about **13 minutes at stock clocks**.
Individual counts vary with the settings and cache contents.

The loading bar says **“Compiling shaders... N of about 2050”**. It counts actual compiled shader entries, not
every graphics-state request. Many states share a shader, so counting states made the old bar advance unevenly.
The total stays approximate until all requests have been translated and duplicates are known.

The bar also shows a percentage and estimated minutes left. It first says it is estimating; after enough work,
it uses the console's measured rate so far. With pending work, the percentage is kept below completion. Normally
the game continues when required preparation finishes. Saved entries are reused on the next start, including
entries completed before an interrupted preparation.

There is also a stall safeguard: after **90 seconds without preparation progress**, the runtime logs the problem
and releases the startup hold. A draw needing an unfinished pipeline still waits for it. This is recovery from
stalled work, not the normal way a full cache is prepared.

> [!NOTE]
> A thirteen-minute compilation on an empty cache is expected. Repeating it at every start is not: keep the session
> logs and check that the cache files can be read and written. Reinstalling the matching update package is the
> simplest way to restore the release caches.

## How graphics state becomes a shader

The GameCube's GX hardware combines textures, colors and lighting through configurable stages. Aurora turns the
required state into shader source. LM-NX's path is:

```text
Aurora GX configuration
  -> WGSL source
  -> Tint translation
  -> GLSL source and binding information
  -> patched uam compiler
  -> DKSH binary
  -> deko3d shader object
```

WGSL and GLSL are languages for GPU programs. Tint is a translator taken from Dawn. uam compiles GLSL into DKSH,
the native shader container loaded by deko3d. Tint's presence does not mean a Dawn graphics device runs on Switch.

`pipelines.bin` is a list of known graphics configurations. Here a pipeline request combines shaders with drawing
state such as blending, depth tests and vertex layout. Two requests can share the same vertex shader, fragment
shader, or both. Consequently these counts are different:

- configurations in the source list;
- expanded pipeline requests for the chosen accuracy and enhancement settings;
- translated shader stages;
- distinct compiled shader entries.

Do not interpret a pipeline count as a count of separate GPU programs or multiply it by a shader compile time.

## Browser compilation

The installer uses [tools/shader_forge.cpp](../tools/shader_forge.cpp), built to WebAssembly. It runs the native
Aurora generator, the same Tint translation and the patched uam compiler. Additional built-in renderer shaders are
listed in [tools/shader_forge_builtin.json](../tools/shader_forge_builtin.json).

Web Workers divide the jobs. A worker is a browser background thread with its own compiler instance. The current
dispatcher chooses from the computer's reported concurrency, leaves one thread free where possible, and caps the
worker count at **12**. It validates agreement on job count and build identity before collecting the results.
Compiled entries are deduplicated and packed in the console's format.

The recorded browser check with **11 workers** took about **20 seconds** and produced output matching the console's
cache entries byte for byte. The time depends on the computer, browser and number of workers; it is not a console
compile time. No precompiled shader library is downloaded by the page.

## Cache identities and validation

[tools/shader_build_id.py](../tools/shader_build_id.py) computes the identity from the shader-related sources and
options. The NRO, shader forge and installer manifest must agree. The page refuses to package shaders if the forge's
reported identity differs from `shader_build` in the manifest.

The directory names also state their purpose:

| Prefix | Contents |
|---|---|
| `lmnx-tint-` | WGSL-to-GLSL translations and metadata |
| `lmnx-f447-gxabi2-` | DKSH built with the selected compiler flags and graphics binding contract |

The binding contract, also called an ABI, tells the shader and renderer where each input lives. The immediate-data
uniform slot is **15** in this path. A valid shader with the wrong bindings can compile successfully and draw the
wrong image.

The cache implementation checks record sizes, source identity and compiled shader structure, including stage and
checksums where the format requires them. Packed files reduce the cost of opening thousands of SD-card files.
Validation still matters: packing entries together does not make damaged contents safe to load.

## Concurrency and correctness

The console uses three pipeline-preparation workers. uam itself is not reentrant in this integration: one serialized
compiler worker owns it. Three preparation threads therefore do not mean three simultaneous uam compiles.
Browser workers use separate instances, which avoid sharing that compiler state.

When changing translation, check coordinate orientation, depth convention, vertex locations, uniform layout and
texture bindings. Then check the real picture on Switch. The one-pass early-depth optimization also needs alpha
preservation and multiple-render-target rules; accepting its shader text is not enough.

The source map is [LMNX-Deko3D/README.md](../LMNX-Deko3D/README.md). For the chronology and measured GPU gains, see
[deko3d-devlog.md](deko3d-devlog.md) and [performance-history.md](performance-history.md).
