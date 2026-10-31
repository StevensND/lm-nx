# How the browser installer works

Open **https://stevensnd.github.io/lm-nx/**, select your own disc images, and choose **Create lm-nx.zip**. Extract
the downloaded zip and copy its `LM-NX` folder into `sdmc:/switch/`. Launch with title takeover (hold **R** while
starting an installed title) or a forwarder. Album applet mode does not provide enough memory.

The installer runs on your computer. It reads your selected discs locally and does not upload them. It downloads
the program and public build tools, prepares the optional European content, compiles shaders and writes the zip.

## Discs and output

- **Required:** USA `GLME01`.
- **Optional:** Europe `GLMP01`, first release, revision 0. With the USA disc, it provides the European Hidden
  Mansion and Spanish, German, French and Italian. See [editions.md](editions.md).
- **Formats:** full ISO, RVZ, WIA, GCZ or CISO. Trimmed `.nkit.iso` images are not supported.

The full GameCube image size is **1,459,978,240 bytes**. Compressed images are read as the full image they represent;
the zip contains plain ISO files because that is what the Switch reads.

```text
LM-NX/
  LM-NX.nro
  pipelines.bin
  orig/GLME01.iso
  orig/GLMP01.iso             optional
  guardados/hidden_mansion/   when built from both discs
  cache/                     two matching shader packages
```

The game creates settings, saves and logs at runtime. The installer does not need an existing save.

## 1. Identify the executable, not just the disc label

The page reads the GameCube header and file table, then locates `main.dol`, the game's executable. It calculates
its SHA-1, a fingerprint of the file's bytes, and compares it with the supported program:

| Disc | SHA-1 of `main.dol` |
|---|---|
| USA `GLME01` | `77d49225e7d28b9777916cdbe5809345c570ecbc` |
| Europe `GLMP01`, revision 0 | `a1e2bd82813787cfe0b11afb5d138f08fdbd94f1` |

Only the header and program need to be read for this check, rather than hashing the entire disc. The disc ID is
checked too. A recognized filename alone is not proof of compatible contents. The console performs its own program
check at startup.

The implementation is in [installer/lib/disc.js](../installer/lib/disc.js) and
[installer/lib/gcm.js](../installer/lib/gcm.js).

## 2. Read compressed images without making a temporary ISO

[installer/lib/disc_image.js](../installer/lib/disc_image.js) implements RVZ, WIA, GCZ and CISO readers written
from the format descriptions. The file-like interface supplies a size and slices, so the disc reader and Hidden
Mansion builder do not need a separate path for each container.

Blocks are decompressed on demand. The reader handles the compression methods used by those formats, including
Zstandard, LZMA, LZMA2, bzip2 and zlib where appropriate. It also reconstructs RVZ's compact representation of unused
disc bytes. WebAssembly supplies several decompressors; the reader itself is JavaScript.

The page neither launches Dolphin nor requires it to convert the image first. Credit goes to the **Dolphin team**
for the formats and their documentation. See [installer/NOTICES.md](../installer/NOTICES.md) for library notices.

## 3. Fetch and verify the release files

[installer/release/manifest.json](../installer/release/manifest.json) names the version, output folder and NRO.
It also supplies the NRO's SHA-256, the pipeline list's SHA-256, and the shader build identity. SHA-256 is another
file fingerprint used here to check downloads.

The page fetches the NRO and `pipelines.bin` from its own `release/` directory and verifies them before packaging.
URLs include the expected hash so an updated file does not accidentally reuse the previous version's cached URL.
The executable is a release asset; it is not stored in the source history by the Pages workflow.

## 4. Build shaders locally

The shader forge is the native Aurora generator, Tint and uam compiled to WebAssembly. Web Workers prepare the
known shader jobs and return the same translation and DKSH records the console cache accepts. The page checks the
forge identity against the manifest, then writes two packed cache files into the zip.

No precompiled shader library is fetched. Moving compilation onto the player's computer removes the roughly
13-minute full-cache console preparation from a normal installation. [shaders.md](shaders.md) explains counts,
identities, workers and the console fallback.

## 5. Build the European Hidden Mansion in JavaScript

When both discs are supplied, [installer/lib/hidden_mansion.js](../installer/lib/hidden_mansion.js) builds the
overlay on the computer. An overlay redirects selected game-file reads to locally prepared replacement files.

The builder reads the European map archive and the required event scripts, combines them with the USA archives,
prepares the lab selection data and writes a redirect manifest. Archive handling, including RARC and Yay0/Yaz0,
lives in [installer/lib/archive.js](../installer/lib/archive.js). The JavaScript builder follows the local Python
tool's transformations; the recorded checks found matching output byte for byte.

All content bytes come from the player's discs. The Switch does not need to manufacture the overlay on first boot.
The retained [HIDDEN_MANSION.md](HIDDEN_MANSION.md) explains the shared game's redirect and selection behavior.

## 6. Stream the zip to the download

[installer/lib/package.js](../installer/lib/package.js) chooses entries and
[installer/lib/zip.js](../installer/lib/zip.js) writes the archive. Disc data is read in pieces instead of collecting
the entire output in one large array.

A service worker, [installer/sw.js](../installer/sw.js), supplies a download response fed through a message port.
The consumer asks for the next piece, keeping the download stream's memory bounded. That matters when the package
contains two full discs. Shader preparation and archive conversion still use working memory; streaming the zip
does not mean the whole page has zero memory cost.

The worker also keeps page assets and used program files for later reuse. Page requests check the network, with a
cached fallback, to avoid mixing an old interface with newly deployed tools. Offline reuse depends on those files
already being cached; the first visit needs a connection. Selected disc images are not placed in that program cache.

## Updating

Choose **Create lm-nx-update.zip**. It contains the NRO, pipeline list and matching shaders. Extract it and copy
the `LM-NX` folder over the existing folder on the SD card. It contains no disc images, Hidden Mansion rebuild or
save replacements, so those existing files remain available.

If a program update changes shader identity, the update package brings the corresponding caches. Copying only a
new NRO can instead trigger the console fallback. Do not rename old cache folders to suppress that check.

## Publishing the page

The installer lives in this repository, under `installer/`. The
[Pages workflow](../.github/workflows/pages.yml) runs for relevant changes on `main` and can also run manually.
It downloads the selected release's NRO, verifies it against the manifest, assembles the page with its tools and
release files, and deploys the result to GitHub Pages.

For a new release, keep the NRO, manifest, pipeline list and forge synchronized. If the shader-related sources
change, rebuild the forge from matching sources and update `shader_build`. A successful page deployment does not
replace testing a generated zip on a real Switch.

For local page work, run this from the repository root:

```sh
python installer/serve.py 8194 installer
```

Open `http://localhost:8194/`. Service workers require localhost or HTTPS; opening the HTML directly as a file is
not sufficient. See [installer/README.md](../installer/README.md) for the page's source layout.
