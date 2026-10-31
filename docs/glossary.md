# Glossary

These are the technical words used in the Switch guides. The retained upstream documents may use additional
terms for desktop tools and the shared game implementation.

## CPU and recompilation

### CPU and GPU

The CPU runs the game's logic and prepares drawing work. The GPU draws images and runs shaders. Making one faster
does not help if the other is the current limit.

### PowerPC and Gekko

PowerPC is the processor family used by the GameCube. Gekko is the GameCube's processor. The Switch uses ARM and
cannot directly execute the GameCube program.

### DOL

The GameCube executable format. `main.dol` contains the game's program. The installer and build tools fingerprint
it to check compatibility.

### Static recompilation and AOT

Translating a program before running it. AOT means ahead of time. Here, PowerPC instructions become C++, which is
compiled into a native Switch program. See [toolchain.md](toolchain.md).

### Guest and host

The guest is the original game's world: its code, memory, addresses and threads. The host is the machine running
the port, such as the Switch, and the code implementing its services.

### HLE

High-level emulation: implementing a console service in host code rather than reproducing every hardware operation.
LM-NX uses HLE for GameCube system functions around the recompiled game.

### Hook and native replacement

A hook redirects a known game function into port code. A native replacement implements that function directly.
Its addresses and behavior must match the supported game program.

### Register and context

A register is a processor's working slot. The context stores the guest processor's state. Keeping values in local
C++ variables can be faster, but code must synchronize values when interrupts, calls or hooks need to observe them.

### MMIO and byte order

MMIO means memory-mapped input/output: special addresses perform hardware operations instead of accessing ordinary
RAM. Byte order describes how multibyte values are stored. GameCube is big endian; Switch ARM is little endian.

## Switch

### Homebrew

Unofficial console software, run on a Switch configured to launch it.

### Horizon and libnx

Horizon is the Switch operating system. libnx is the homebrew library for its files, threads, controllers, audio
and other services. See [platform-notes.md](platform-notes.md).

### devkitPro and devkitA64

devkitPro supplies console development tools and libraries. devkitA64 is its compiler toolchain for Switch ARM.

### NRO

The homebrew executable format. The installed program is `LM-NX.nro`. An ELF is the linked program used during
building; retaining its symbols helps translate crash addresses into function names.

### Title takeover, forwarder and applet mode

Title takeover opens the Homebrew Menu with application resources by holding **R** while launching an installed
title. A forwarder starts a homebrew program through an installed launcher. Album applet mode has fewer resources
and does not provide enough memory for LM-NX.

### Thread and priority

A thread is a stream of CPU work that can run alongside others. Priority affects which runnable thread goes first.
Horizon uses smaller numbers for higher priority. A core mask lists which processor cores a thread may use.

### Race

A bug whose result depends on the order or timing of concurrent operations. The thunder crackle came from one
callback entering before another call had advanced the audio stream offset.

### Stock clocks

Normal supported CPU, GPU and memory speeds without overclocking. Report all three actual clocks when comparing
performance. LM-NX requests its handheld performance configuration through the system.

## Graphics

### Aurora

The compatibility library by encounter used for GameCube graphics and platform support. LM-NX adapts its graphics
producer to native deko3d resources.

### GX and FIFO

GX is the GameCube graphics interface. A FIFO is a first-in, first-out stream of commands. The graphics worker reads
the game's GX commands and prepares the corresponding native draws.

### deko3d

The low-level Switch graphics interface from fincs and devkitPro. LM-NX records its native GPU commands through
this interface. See [native-renderer.md](native-renderer.md).

### Draw and render pass

A draw asks the GPU to render geometry. A render pass groups work drawing into a set of images. A render target is
one of those images. A texture is an image read by a shader, and a sampler controls how it is read.

### EFB and readback

EFB means embedded frame buffer: the GameCube's color and depth rendering storage. A readback transfers GPU results
to memory the CPU can read. It is distinct from sampling a rendered image in another GPU pass.

### Shader

A small GPU program. A vertex shader handles geometry inputs; a fragment shader calculates pixel results.
See [shaders.md](shaders.md).

### WGSL, Tint, GLSL, uam and DKSH

WGSL is the language Aurora generates. Tint translates it into GLSL, another shader language. uam compiles GLSL
into DKSH, the binary shader container that deko3d loads.

### Pipeline

In these guides, a prepared combination of shaders and graphics state used for drawing. Multiple pipeline requests
may share one compiled shader. LM-NX's objects are not Vulkan pipeline objects inside a separate Switch driver.

### Uniform, binding, descriptor and ABI

A uniform supplies values a shader reads. A binding is its resource slot. A descriptor identifies a texture or
buffer for the GPU. An ABI is an agreed layout and calling or binding contract between components.

### Fence and barrier

A fence signals that submitted GPU work is complete. A barrier orders GPU operations and visibility, such as
rendering an image before reading it as a texture. They solve different synchronization needs.

### Early depth and alpha test

Depth describes how far a surface is from the camera. Early depth testing can reject hidden pixels before shading.
An alpha test rejects pixels based on their transparency value. Their ordering can affect correctness.

### Swapchain and presentation

A swapchain is the set of images that take turns on screen. Presentation hands a finished image to the display
system. A count of presented images does not by itself prove that each is a new gameplay frame.

## Sound and data

### JAudio, DSP and ARAM

JAudio is the game's sound library. DSP means digital signal processor, the GameCube's sound processor. ARAM is
the audio memory used by the original sound system.

### ADPCM and stream

ADPCM compresses sound using differences and prior sample history. A stream reads sound in chunks during playback.
The wrong chunk order can corrupt later output because that decoder history matters.

### Sample rate, resampling and underrun

Sample rate is the number of audio samples per second. Resampling changes that rate. An underrun happens when
the output needs samples before more are ready. It can make gaps, but not every crackle is an underrun.

### ISO, RVZ, WIA, GCZ and CISO

An ISO here is a plain GameCube disc image. The others are compressed image containers accepted by the installer.
It reads their blocks and writes plain ISOs for the Switch.

### Hash, SHA-1, SHA-256 and checksum

A hash is a fingerprint computed from bytes. SHA-1 identifies supported game executables; SHA-256 verifies release
downloads. A checksum detects accidental damage. None makes two different programs compatible.

### Overlay

Two meanings: a tool displaying information over a game, or replacement files redirecting selected game-data
reads. The Hidden Mansion uses the second meaning.

### WebAssembly, Web Worker and service worker

WebAssembly lets a browser run compiled tools. A Web Worker performs background computation, such as shader jobs.
A service worker handles page requests, caching and the streamed zip download. See [installer.md](installer.md).

## Building and measurement

### LTO

Link-time optimization. The compiler optimizes across source files when linking the program.

### PGO

Profile-guided optimization. A special build records real execution; a later build uses that profile to improve
common paths. LM-NX trains on the Switch, not on the PC. See [toolchain.md](toolchain.md#pgo-on-the-console).

### Inlining and function alignment

Inlining copies a called function into its caller. Alignment places functions at chosen address boundaries.
Either can change code size and cache behavior; more inlining does not always mean faster execution.

### FPS, frame time and pacing

FPS is frames per second. Frame time is the duration of one frame: about 33.3 ms at 30 FPS. Pacing is how evenly
frames reach the screen. Slow-frame counts and percentiles describe variation hidden by an average.

### Vblank and refresh

A display refresh updates the screen. Vblank is the interval between refreshes. At 60 Hz, one refresh takes about
16.7 ms. Two refreshes per frame give 30 FPS; missing that deadline can leave a frame on screen longer.

### Bottleneck and profiling

A bottleneck is the part currently limiting progress. Profiling measures where time is spent. CPU stack sampling
records where threads are at intervals; GPU timestamps measure positions in submitted graphics work.
See [measuring.md](measuring.md) for the limitations of both.

### A/B comparison

Comparing two versions under matching conditions, changing one thing at a time. Keep the scene, settings, clocks
and cache state consistent.
