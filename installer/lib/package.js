// What goes in the zip, and the zip itself. Everything sits in one folder (the manifest's "folder"), which the player
// copies to sdmc:/switch/ as it is:
//
//   <folder>/<nro>                        the game (downloaded from ./release/ and checked against its SHA-256)
//   <folder>/pipelines.bin                the list of graphics states the game prepares at startup
//   <folder>/orig/GLME01.iso              the player's USA disc
//   <folder>/orig/GLMP01.iso              the player's European disc (at least one of the two)
//   <folder>/guardados/hidden_mansion/    the European Hidden Mansion, made from both discs
//   <folder>/cache/...                    the shaders, compiled in this browser
//   <folder>/install.ini, settings.ini    when the European version must start: a language other than English
//                                         was chosen, or there is no USA disc (the settings menu changes it later)
//
// The update zip has only the game, the list and the shaders: the discs and the saves on the SD card stay as they are.
import { ZipWriter, zipSize } from './zip.js';

const CHUNK = 8 * 1024 * 1024;

async function* fileChunks(file) {
  for (let at = 0; at < file.size; at += CHUNK) {
    yield new Uint8Array(await file.slice(at, Math.min(at + CHUNK, file.size)).arrayBuffer());
  }
}

// The shader caches are about 110 MB: sent in pieces too, so the download never holds a second copy of them.
function* byteChunks(bytes) {
  for (let at = 0; at < bytes.length; at += CHUNK) {
    yield bytes.subarray(at, Math.min(at + CHUNK, bytes.length));
  }
}

// The European game's language numbers (settings.ini [enhancements] pal_language), as the game counts them.
export const PAL_LANGUAGES = { de: 2, fr: 3, es: 4, it: 5 };

// discs: { usa: File | null, pal: File | null }, at least one. hiddenMansion and shaders: [{ path, data }] (paths inside their folders).
// gameLanguage: 'en', or one of PAL_LANGUAGES when the European disc is in the package.
export function packageEntries(manifest, { nro, pipelines, shaders, discs = null, hiddenMansion = [], gameLanguage = 'en' }) {
  const root = `${manifest.folder}/`;
  const entries = [
    { path: `${root}${manifest.nro}`, bytes: nro },
    { path: `${root}pipelines.bin`, bytes: pipelines },
    ...shaders.map((f) => ({ path: `${root}${f.path}`, bytes: f.data })),
  ];
  if (discs) {
    if (discs.usa) entries.push({ path: `${root}orig/GLME01.iso`, file: discs.usa });
    if (discs.pal) {
      entries.push({ path: `${root}orig/GLMP01.iso`, file: discs.pal });
      for (const f of hiddenMansion) {
        entries.push({ path: `${root}guardados/hidden_mansion/${f.path}`, bytes: f.data });
      }
      // with only the European disc, English is the European version's English
      const palLanguage = PAL_LANGUAGES[gameLanguage] ?? (discs.usa ? 0 : 1);
      if (palLanguage) {
        const text = (lines) => new TextEncoder().encode(lines.join('\n') + '\n');
        entries.push({ path: `${root}install.ini`, bytes: text(['[install]', 'base=PAL']) });
        entries.push({ path: `${root}settings.ini`, bytes: text(['[enhancements]', `pal_language=${palLanguage}`]) });
      }
    }
  }
  return entries.map((e) => ({ ...e, size: e.file ? e.file.size : e.bytes.length }));
}

export function packageSize(entries) {
  return zipSize(entries);
}

// sink: { write(Uint8Array), close() }. progress(written, total) follows the bytes of the zip.
export async function writePackage(entries, sink, progress = () => {}) {
  const total = zipSize(entries);
  let written = 0;
  const counted = {
    async write(bytes) {
      await sink.write(bytes);
      written += bytes.length;
      progress(written, total);
    },
    close: () => sink.close(),
  };
  const zip = new ZipWriter(counted);
  for (const e of entries) {
    if (e.file) {
      await zip.addFile(e.path, e.size, fileChunks(e.file));
    } else {
      await zip.addFile(e.path, e.size, byteChunks(e.bytes));
    }
  }
  await zip.finish();
  if (written !== total) {
    throw new Error(`the zip should be ${total} bytes, it is ${written}`);
  }
}
