# How the native renderer draws the game

LM-NX uses [deko3d](glossary.md#deko3d) on the Switch. The original game's graphics calls pass through Aurora,
then through LM-NX's native resource adapter, into commands for the Switch GPU. This was the Switch rendering path
from the beginning. For the shared GX behavior, see [RENDERING.md](RENDERING.md).

## The path of a frame

```text
Recompiled game
  -> GameCube GX calls and command bytes
  -> Aurora recording and GX FIFO worker
  -> native graphics resources and render passes
  -> LMNX-Deko3D / plume command encoding
  -> deko3d queue
  -> Switch display
```

GX is the GameCube graphics interface. Its FIFO is a first-in, first-out stream of drawing commands. Aurora reads
those commands, tracks graphics state, and prepares draws, copies and effects. A draw is one request to render
geometry. The native adapter gives those operations Switch-side buffers, textures, samplers and shaders.

The Switch program does not create a Dawn rendering device. Some adapter types retain names familiar from WebGPU,
but they are LM-NX's own types. Tint, taken from Dawn, is used as a shader translator. That does not make Dawn the
renderer. The separate PC comparison path uses plume with Vulkan and is useful for checking shared behavior.

## Where the work lives

| Source | Responsibility |
|---|---|
| [prepare_aurora_native.py](../tools/prepare_aurora_native.py) | Prepare Aurora with native resources |
| [LMNX-Deko3D/src/aurora_gpu.cpp](../LMNX-Deko3D/src/aurora_gpu.cpp) | Resources, passes and recording operations |
| [encoding_lote_directo.inc](../platforms/switch/encoding_lote_directo.inc) | Encode a prepared batch directly |
| [LMNX-Deko3D/src/native_gfx.cpp](../LMNX-Deko3D/src/native_gfx.cpp) | Native submission, resources and presentation |
| [LMNX-Deko3D/third_party/plume/](../LMNX-Deko3D/third_party/plume/) | deko3d memory, descriptors and commands |
| [LMNX-Deko3D/src/shader_cache.cpp](../LMNX-Deko3D/src/shader_cache.cpp) | Validate, store and load compiled shaders |

## Record a batch, keep it alive, then retire it

Aurora's recording preserves the order of render passes, copies, uploads and readbacks. A readback copies GPU
results into memory the CPU can inspect. The consumer cannot reorder an operation across a dependency just because
it looks like a simple copy.

The direct batch path avoids preparing generic resource bindings again for every draw. It reuses unchanged state,
shader selections, texture tables and immediate values within the scope where they remain valid. Small per-draw
values use a uniform buffer at binding **15**. A binding is the numbered slot through which a shader finds data;
the producer, Tint output and native backend must agree on its meaning.

GPU execution is asynchronous: recording a command does not mean it has finished. Submitted work owns the buffers,
textures, descriptor tables and staging data that its commands reference. A fence marks completion. Resources may
be reused or freed only after the corresponding fence passes. More segments in flight can reduce waiting, but
only if those ownership rules still hold.

The backend suballocates resources from larger blocks: it splits one GPU memory block into smaller aligned ranges.
This avoids creating a system allocation for every small buffer or texture table. Memory reports distinguish live
bytes, peak live bytes and total bytes ever allocated; the last value is not current memory use.

## Textures, descriptors and barriers

A texture is an image that a shader reads. A sampler describes how to read it, including filtering and edge rules.
A descriptor records where that resource lives. Reusing a descriptor table is safe only while all the resources and
their interpretation still match. Table changes must not overwrite entries still used by submitted work.

A barrier orders GPU accesses and makes previous writes visible to later reads. Repeated reads of the same texture
do not need the same synchronization as rendering into it and then sampling it. LM-NX tracks these transitions to
avoid redundant work, while retaining ordering around copies and writes. The earliest conservative implementation
used too many broad barriers and cache invalidations; removing them indiscriminately would be equally wrong.

Full barriers remain important when coordinating different GPU engines. A narrower graphics dependency must be
proved sufficient for the actual producer and consumer, including readbacks and compressed surfaces.

## Preserve GameCube rendering rules

The GameCube EFB, its embedded frame buffer, stores the color and depth being drawn. LM-NX reproduces that behavior
with Switch images and conversion passes. Copies can include filtering, format conversion and reduced precision.
Their order matters: combining two rounding operations into one can change the image.

Two changes were especially useful:

- **Six-bit blending with alternating surfaces.** Rendering uses separate surfaces where the compatibility path
  needs the previous color, avoiding unsafe feedback from sampling the active destination.
- **Merged early-depth behavior.** Eligible alpha-tested draws use one pass instead of a split depth/color path.
  The rules account for multiple color targets and preserved alpha. Unsupported cases keep the conservative path.

The second change reduced measured GPU time from **34.4 to 23.8 ms** in the foyer comparison. Its first version
matched no useful draws because it assumed one color target. Counters of eligible and actually merged draws were
needed to establish that it was running. See [performance-history.md](performance-history.md).

RGB565 copy conversion also has specialized shader variants for filtering and quantization, with redundant clears
removed when the pass overwrites the entire destination. The physical target remains suitable for all modes;
calling a conversion “RGB565” does not establish its physical storage format or the cost of its arithmetic.

## Showing the finished image

The native presentation path uses the system window and a deko3d swapchain, a set of images that take turns on
screen. At startup the port chooses **1280x720 handheld** or **1920x1080 docked**. Restart after changing mode to
select the other size.

The loading screen and settings use a native ImGui renderer. Its vertex attributes must be supplied just like any
other draw: the early invisible progress bar came from missing attributes, not from a lack of presentation calls.
Open the settings menu with **L3 + R3**.

For the shader translation path, cache identities and browser compilation, see [shaders.md](shaders.md).
For implementation order and reusable checks, see [HowToDeko3d.md](HowToDeko3d.md).
