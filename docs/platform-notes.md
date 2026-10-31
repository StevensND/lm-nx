# Horizon and libnx: platform notes

Horizon is the Switch operating system; homebrew reaches its services through libnx. This page explains LM-NX's
memory, scheduling, SD reads, presentation and input. The shared GameCube runtime is documented in
[PORTING.md](PORTING.md) and [RECOMPILER.md](RECOMPILER.md).

## Application mode and memory

Launch by holding **R** while starting an installed title, or use a suitable forwarder. Album applet mode does not
supply enough memory. This is a launch requirement, not a shader-cache problem.

Original GameCube MEM1 is **24 MiB**, but that is not the port's memory budget. LM-NX also needs its native program,
runtime, stacks, shader translations and GPU code, textures, staging and display images. The guest-memory backing
window is **64 MiB**, including locked-cache mapping and host-only space. Accessors reproduce address mirrors over
that storage; they do not implement the Xbox 360 virtual-memory model.

The backend limits its tracked blocks to **1,792 MiB**. This is not all process memory, nor a claim about what applet
mode supplies. SDK allocations may sit outside the count. Reports distinguish live, peak and cumulative allocation;
a cumulative threshold does not mean one small resource allocated that whole amount. Process heap reservation also
does not describe free space within that heap.

## GPU memory and ownership

Small buffers and descriptor tables share larger aligned blocks; images use a separate heap. Ranges remain alive
until their submitted work finishes. CPU-cached and uncached mappings have different visibility rules; readback
requires the appropriate cache maintenance after GPU completion.

A fence signals completion of previous GPU work. It must be attached to submitted work before being waited on.
More in-flight segments need more retained resources. A timeout or queue error does not permit recycling storage as
if the GPU had finished. See [native-renderer.md](native-renderer.md).

## Threads and priorities

A thread is an independent stream of CPU work. Guest GameCube threads have a scheduler inside the port; host workers
handle graphics, compilation, sound and logging. Releasing the guest CPU during a host operation can let a guest
callback enter before its caller finishes bookkeeping. The thunder fix in [switch-audio.md](switch-audio.md)
shows why those two levels of scheduling matter.

The normal policy intersects the allowed core mask with **`0x7`**, the three application cores. A preferred core is
selected within that mask; it is not necessarily exclusive pinning. FIFO experiments on core 3 did not produce a
useful FPS gain and are not a release requirement.

Lower numeric priorities run first. The explicit runtime roles are:

| Role | Priority |
|---|---|
| Audio | `0x2B` |
| Ordinary game/render runtime roles | `0x2C` |
| Compilation and background log writing | `0x3B` |

Not all threads use the same creation API. SDL threads can apply their own mapping, and the sampler raises its
priority during collection. Read actual reports before treating this role table as the priority of every worker.
Increasing priority does not remove expensive work and can leave another worker without enough CPU time.

The pthread wrapper raises ordinary stack requests to at least **1 MiB**, aligned to **4 KiB**. It retains larger
requests and explicit caller-owned stacks. Compiler work can require larger stacks; track the total allocation.

Workers need a stop condition, wakeup and owner that joins them. Do not assume detached desktop thread patterns work
unchanged on Horizon. Queue notifications must be synchronized with their wait conditions. Add concurrency only
when measurements show it removes a real wait without starving other work.

## Disc reads

The runtime reads plain ISOs under `LM-NX/orig/`. The browser expands compressed inputs while packaging; the Switch
does not need RVZ or WIA decompression during play.

File-service reads use host-owned staging before copying into guest memory. Direct-to-guest reads failed because
the target pages were unsuitable for the system operation. A valid C++ pointer is not by itself proof that a region
can be used as a system-service transfer buffer.

DVD HLE arbitrates reads: large ordinary reads proceed in pieces so waiting small or high-priority reads can go
first. Asynchronous requests of **32 KiB or less** retain the guest CPU until the request returns. Completion still
arrives later. This protects stream loaders that advance their offset only after issuing the request. Larger reads
may release the CPU when interrupts are enabled. See [switch-audio.md](switch-audio.md) for the demonstrated race.

## Logging and saves

Synchronous SD logging disturbed frame pacing. The buffered logger copies messages into memory queues and lets a
low-priority worker write them. Formatting, allocator statistics and stack sampling can still cost time before the
write, so disable detailed diagnostics for final FPS comparisons.

Save replacement also needed adaptation to SD filesystem rename behavior. Keep the existing `guardados/` folder
when updating. The update zip contains no save replacements.

## Clocks and presentation

The project's stock settings are:

| Mode | CPU | GPU | Memory | Internal and output size |
|---|---|---|---|---|
| Handheld | 1020 MHz | 460.8 MHz | 1331.2 MHz | 1280x720 |
| Docked | 1020 MHz | 768 MHz | 1600 MHz | 1920x1080 |

The program requests a system performance configuration. Read actual clocks during tests: external profiles can
change them. A successful request is not a measurement.

Size is selected at startup, before the native swapchain is created. Restart after docking or undocking to select
the other mode. The menu scales with display height. The measured docked foyer GPU time was about **30.5 ms**,
close to the **33.3 ms** budget for 30 FPS.

## Input and overlays

Open and close settings with **L3 + R3 only**. Minus alone is not its shortcut. See the
[main controls table](../README.md#controls) and retained [CONTROLS.md](CONTROLS.md).

The port publishes presentation data through its SaltyNX integration: homebrew cannot rely on ordinary commercial
game hooks. Treat the displayed FPS as presentation rate. Loading, menu and gameplay images are different workloads.
See [measuring.md](measuring.md) before using an overlay reading as a gameplay performance claim.
