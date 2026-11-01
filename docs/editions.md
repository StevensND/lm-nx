# Discs, languages and the Hidden Mansion

LM-NX uses **one NRO**. It runs the USA disc, the European (PAL) disc or both; you need at least one. The PAL
disc brings its Hidden Mansion and its languages. You do not choose a separate language-specific Switch executable.

| Disc | Role |
|---|---|
| USA `GLME01` | Runs the USA version |
| Europe `GLMP01`, first release, revision 0 | Runs the PAL version, with its Hidden Mansion and languages; with the USA disc too, it also gives the USA version the PAL Hidden Mansion |

The added languages are **Spanish, German, French and Italian**. The compatibility check uses the SHA-1 of
`main.dol`, the program inside the disc, as well as the disc ID. The hashes and input-format details are in
[installer.md](installer.md#1-identify-the-executable-not-just-the-disc-label).

## What the European disc adds

The European Hidden Mansion differs from the USA version in its map and event behavior. The installer reads both
of the player's discs and prepares a file overlay: selected reads are redirected to the newly built map and event
archives. The base game data stays on the discs.

The browser performs this work before the zip is downloaded. The resulting files are placed under
`LM-NX/guardados/hidden_mansion/`. Keep that folder with the installed game. Copying only the European ISO by hand
does not reproduce all of the installer's preparation.

For the exact shared selection and overlay rules, see [HIDDEN_MANSION.md](HIDDEN_MANSION.md). For the regional
differences and behavior checks, see [PAL_PARITY.md](PAL_PARITY.md). These retained documents describe the shared
recompilation, so desktop installation steps may differ from the Switch installer.

## Why executable checks matter

The recompiler translates a known program and uses known function addresses, symbols and hooks. A different
executable cannot be made compatible by changing a filename or disc label. The build tools and runtime retain
their checks rather than assuming every disc with the same title has the same code.

The build system can include a second statically compiled European module. It has a separate C++ namespace so
function names do not collide with the USA module. This is different from distributing several NROs for players to
select manually. See [toolchain.md](toolchain.md) and [building.md](building.md).

## Regional timing in tests

PAL is the name commonly used for the European video region. A 50 Hz mode refreshes every 20 ms; presenting on every
second refresh produces a 25 FPS ceiling. During development, tests using European behavior needed the 60 Hz mode
to assess the 30 FPS target fairly. That change in presentation timing must not be confused with a CPU optimization.

Use the same edition, save, scene and timing settings for a comparison. Keep settings and saves when updating with
the update zip. Open settings with **L3 + R3**. See [measuring.md](measuring.md) for a repeatable console test.
