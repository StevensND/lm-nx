# Documentation

Start with [Building for Switch](BUILDING_SWITCH.md) for this port. PC documentation is retained as background and may describe features or tools that differ on Switch.

- [ACCURACY](ACCURACY.md): Compatibility checks and known differences from the original game.
- [AUDIO](AUDIO.md): Audio emulation, mixing and output timing.
- [BUILDING_SWITCH](BUILDING_SWITCH.md): Switch toolchain, local sources, game code generation and PGO.
- [CONTROLS](CONTROLS.md): Controller mappings and input behavior.
- [ENHANCEMENTS](ENHANCEMENTS.md): Optional rendering and gameplay improvements.
- [GAMEPLAY_OPTIONS](GAMEPLAY_OPTIONS.md): Gameplay settings and their effects.
- [HIDDEN_MANSION](HIDDEN_MANSION.md): European Hidden Mansion support.
- [INTERPOLATION](INTERPOLATION.md): Frame interpolation and object tracking.
- [LINUX](LINUX.md): Original PC port setup on Linux.
- [MACOS](MACOS.md): Original PC port setup on macOS.
- [OPTIONS_AUDIT](OPTIONS_AUDIT.md): Settings review and implementation details.
- [PAL_PARITY](PAL_PARITY.md): European version behavior and compatibility.
- [PC_ORIGINAL](PC_ORIGINAL.md): README of the original PC port.
- [PORTING](PORTING.md): Port architecture and platform integration.
- [README_PC_REFERENCIA](README_PC_REFERENCIA.md): Additional retained PC README reference.
- [RECOMPILER](RECOMPILER.md): PowerPC translation and runtime interfaces.
- [RENDERING](RENDERING.md): GX graphics translation and rendering details.
- [ROADMAP](ROADMAP.md): Development roadmap and tracked work.
- [SETUP](SETUP.md): Original PC setup and development instructions.
- [UNUSED_CONTENT](UNUSED_CONTENT.md): Restored unused content and related options.

## Glossary

| Term | Meaning |
|---|---|
| AOT | Ahead-of-time compilation: translating game code before running it. |
| HLE | High-level emulation: implementing a console service through host code. |
| NRO | The executable format used by Switch homebrew. |
| deko3d | A graphics API for the Switch GPU. |
| PGO | Profile-guided optimization: compiling with measurements from actual execution. |
| GX | The GameCube graphics interface. |
| EFB | Embedded frame buffer: the GameCube's color and depth rendering memory. |
| Title takeover | Starting the Homebrew Menu by holding R while launching a game, giving homebrew application resources. |
