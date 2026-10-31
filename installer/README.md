# lm-nx Installer

The page lives in `installer/` of this repository and is published by [`.github/workflows/pages.yml`](../.github/workflows/pages.yml).

The installer page of [lm-nx](https://github.com/StevensND/lm-nx), the native Nintendo Switch port of Luigi's
Mansion: **https://stevensnd.github.io/lm-nx/**

You give it your own disc images of the GameCube game and it gives you a zip with one folder. Copy that folder into
`sdmc:/switch/` and the game is ready. Everything runs in your browser: your disc images never leave your computer.

## What it needs

- **Luigi's Mansion (USA)**, `GLME01`. Required: it is the game the port runs.
- **Luigi's Mansion (Europe)**, `GLMP01`, first release (revision 0). Optional: with both discs the page also builds
  the European **Hidden Mansion**, and the game gets the other languages of the European disc: Spanish, German, French
  and Italian.

Each disc can be a full `.iso` (1,459,978,240 bytes) or a compressed image made from one: `.rvz`, `.wia`, `.gcz` or
`.ciso`, with any compression Dolphin offers for them (zstd, LZMA, LZMA2, bzip2 or none). A compressed image is read
as the `.iso` it holds, block by block, and goes into the zip as that full `.iso`: the game on the Switch always gets
a plain `.iso`. Trimmed images (`.nkit.iso`) are not supported.

## What it does

1. Identifies each disc by the SHA-1 of its `main.dol`, the same check the game makes when it starts. Only the header
   and the program are read, a few megabytes (decompressed on the fly for a compressed image).
2. Downloads the NRO and the pipeline list from `release/` and checks them against the SHA-256 in
   `release/manifest.json`.
3. Compiles the shaders in your browser, on several threads, from the pipeline list: no shader is downloaded. The
   result is the two cache files the game reads, byte for byte what the console would write after compiling them
   itself, so the first start on the Switch only loads them.
4. Builds the European Hidden Mansion from your two discs (only when you give it both).
5. Streams the zip to disk through the service worker, so a package of 3 GB never has to fit in memory.

The update zip skips steps 1 and 4: it has only the game, the pipeline list and the shaders. Your discs and your
saves on the SD card stay as they are.

## Layout

| Path | Contents |
|---|---|
| `index.html`, `app.js` | The page and its interface, with the side menu's pages (help, changelog, Discord, credits) |
| `sw.js` | Offline cache and the download stream |
| `lib/i18n.js`, `lib/flags.js` | The page in the five languages of the European disc, and their flags |
| `lib/disc.js`, `lib/gcm.js` | GameCube disc reader and disc identification |
| `lib/disc_image.js`, `lib/codecs.wasm` | Reader of compressed disc images (RVZ, WIA, GCZ, CISO) and its decompressors (see `NOTICES.md`) |
| `lib/archive.js`, `lib/hidden_mansion.js` | Yay0/Yaz0 and RARC archives, and the Hidden Mansion builder |
| `lib/shaders.js`, `lib/shader_worker.js` | The shader compilation, shared between Web Workers |
| `lib/package.js`, `lib/zip.js` | What goes in the zip, and the streaming zip writer |
| `forge/` | The shader compiler in WebAssembly (see `NOTICES.md`) |
| `fonts/`, `icons/` | The Delfino font of the titles and the side menu, and the page icons |
| `release/` | `manifest.json` (version, folder, NRO and their SHA-256) and `pipelines.bin` |

## Releasing a new version

1. Publish the release of [lm-nx](https://github.com/StevensND/lm-nx) with its `.nro`.
2. Update `installer/release/manifest.json` from the repository root: version, NRO name and SHA-256, and `shader_build` (the cache id the NRO uses).
   If the shader code of the NRO changed, rebuild `forge/` from the same commit; the page refuses to make a package
   when the compiler and the game disagree.
3. Push the installer changes to `main`, or run the Pages workflow manually. The workflow downloads the release NRO, checks it and publishes the page.

## Testing it locally

From `installer/`, run:

```
python serve.py 8194 .
```

Then open `http://localhost:8194/`. The page needs a service worker, so it has to be served over `http://localhost`
or `https://`; opening `index.html` as a file does not work.
