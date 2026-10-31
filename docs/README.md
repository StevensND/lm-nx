# Documentation

These guides explain how LM-NX works, how to build it, and what can be reused for another GameCube port. Start
with the player instructions in the [main README](../README.md) or the [installer guide](installer.md).
If a technical word is new, use the [glossary](glossary.md).

## A few words before you start

| Term | Plain meaning |
|---|---|
| AOT | Ahead-of-time compilation: translate the game before playing. |
| HLE | Native implementations of the GameCube services the translated game expects. |
| GX | The GameCube graphics interface. |
| deko3d | The native Switch graphics interface used by this port. |
| Shader | A small GPU program used to draw the image. |
| NRO | The Switch homebrew program, `LM-NX.nro`. |
| PGO | Compiler optimization using a recording of actual console play. |

The [full glossary](glossary.md) also explains memory, audio, browser tools and measurement terms.

## How the port fits together

1. The owner's `main.dol` is translated from PowerPC into C++ and compiled for Switch ARM.
   See [toolchain.md](toolchain.md).
2. The runtime provides GameCube memory, scheduling, files, sound and controllers.
   See [platform-notes.md](platform-notes.md) and [switch-audio.md](switch-audio.md).
3. Aurora turns GX graphics into native resources and commands. LMNX-Deko3D submits them through deko3d.
   See [native-renderer.md](native-renderer.md).
4. Aurora generates WGSL shaders; Tint translates them to GLSL; uam compiles them for the Switch GPU.
   The installer performs this work in the browser. See [shaders.md](shaders.md).
5. The installer checks the player's discs, builds optional European content and packages everything locally.
   See [installer.md](installer.md) and [editions.md](editions.md).

## Choose a starting point

| You want to... | Read |
|---|---|
| Install or update | [Installer](installer.md), [editions](editions.md) |
| Understand a long first start | [Shaders](shaders.md) |
| Build the NRO | [Building](building.md), then [toolchain](toolchain.md) |
| Understand performance | [Measuring](measuring.md), then [performance history](performance-history.md) |
| Follow the thunder crackling fix | [Switch audio](switch-audio.md) |
| Study the renderer | [Native renderer](native-renderer.md), then [deko3d guide](HowToDeko3d.md) |
| Start another GameCube port | [Porting another game](porting-another-game.md) |

## Switch guides

| Document | What it explains |
|---|---|
| [glossary.md](glossary.md) | Technical terms in plain words |
| [installer.md](installer.md) | Disc readers, browser builds, downloads, updates and GitHub Pages |
| [editions.md](editions.md) | Required USA disc and optional European content and languages |
| [building.md](building.md) | Dependencies, code generation, NRO packaging and PGO commands |
| [toolchain.md](toolchain.md) | Translation, compiler options, LTO and console profiles |
| [native-renderer.md](native-renderer.md) | Aurora's native resource path and deko3d drawing |
| [HowToDeko3d.md](HowToDeko3d.md) | A practical implementation and validation guide |
| [shaders.md](shaders.md) | WGSL, Tint, GLSL, uam, cache identities and fallback compilation |
| [platform-notes.md](platform-notes.md) | Horizon memory, threads, reads, presentation and input |
| [switch-audio.md](switch-audio.md) | Sound output and the full thunder crackling investigation |
| [measuring.md](measuring.md) | Repeatable tests and misleading counters |
| [performance-history.md](performance-history.md) | Dated progress toward 30 FPS, failed trials and measured limits |
| [deko3d-devlog.md](deko3d-devlog.md) | Renderer development, correctness fixes and console results |
| [porting-another-game.md](porting-another-game.md) | Reusable work and game-specific checks |
| [BUILDING_SWITCH.md](BUILDING_SWITCH.md) | Compatibility link to the new build guide |

The Switch audio guide has a distinct name so the retained upstream `AUDIO.md` remains a separate file on
case-insensitive filesystems.

## Retained shared and upstream documentation

These documents describe the common recompilation and its desktop origins. They remain useful references, but
can describe desktop tools, shortcuts, installation steps or features that differ on Switch. Use the Switch
guides above for the public NRO, installer and console behavior. The Switch menu opens with **L3 + R3 only**.

| Document | Shared subject |
|---|---|
| [RECOMPILER.md](RECOMPILER.md) | PowerPC translation and runtime interfaces |
| [RENDERING.md](RENDERING.md) | GX state, graphics accuracy and rendering features |
| [AUDIO.md](AUDIO.md) | Sound decoding, mixing, channel order and output timing |
| [CONTROLS.md](CONTROLS.md) | Input mapping and control settings |
| [HIDDEN_MANSION.md](HIDDEN_MANSION.md) | European Hidden Mansion overlays and selection |
| [PAL_PARITY.md](PAL_PARITY.md) | Regional behavior and compatibility |
| [PORTING.md](PORTING.md) | Shared architecture and platform integration |
| [ACCURACY.md](ACCURACY.md) | Accuracy checks and known differences |
| [ENHANCEMENTS.md](ENHANCEMENTS.md) | Optional visual and gameplay enhancements |
| [GAMEPLAY_OPTIONS.md](GAMEPLAY_OPTIONS.md) | Game options and effects |
| [INTERPOLATION.md](INTERPOLATION.md) | Interpolation and tracked game objects |
| [OPTIONS_AUDIT.md](OPTIONS_AUDIT.md) | Settings implementation review |
| [UNUSED_CONTENT.md](UNUSED_CONTENT.md) | Restored content and related options |
| [SETUP.md](SETUP.md) | Original desktop setup and development |
| [LINUX.md](LINUX.md) | Original Linux target |
| [MACOS.md](MACOS.md) | Original macOS target |
| [PC_ORIGINAL.md](PC_ORIGINAL.md) | Original PC README |
| [README_PC_REFERENCIA.md](README_PC_REFERENCIA.md) | Additional retained PC reference |
| [ROADMAP.md](ROADMAP.md) | Shared development roadmap and tracked work |

## Credits

The port builds on **rob646464** (Luigi's Mansion Recompiled), **encounter** (Aurora), **TwilitRealm** (Dusklight),
**centollOS** (SwitchWaker and SwitchWakerHD), **TP-Aleks**, **fincs**, **devkitPro**, **switchbrew**, the **Dolphin
team** and **banjo-nx**. See the [full credits](../README.md#credits) and
[third-party notices](../THIRD_PARTY_NOTICES.md).
