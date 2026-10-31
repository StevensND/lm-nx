// Tells which Luigi's Mansion disc a .iso is, the same way the game does: by the SHA-1 of its main.dol.
// Only the header and the program are read (a few megabytes), never the whole image.
import { listGcmFiles, readDiscHeader } from './gcm.js';

// A full GameCube disc image. Compressed images (.rvz, .wia, .gcz, .ciso) reach this file already read as one
// (lib/disc_image.js); a trimmed image cannot be used.
export const DISC_SIZE = 1459978240;

// SHA-1 of each supported main.dol (src/port/dol.cpp).
const DOL_SHA1 = {
  '77d49225e7d28b9777916cdbe5809345c570ecbc': 'usa',
  'a1e2bd82813787cfe0b11afb5d138f08fdbd94f1': 'pal',
};

async function readBytes(blob, offset, length) {
  return new Uint8Array(await blob.slice(offset, offset + length).arrayBuffer());
}

function be32(bytes, at) {
  return ((bytes[at] << 24) | (bytes[at + 1] << 16) | (bytes[at + 2] << 8) | bytes[at + 3]) >>> 0;
}

function hex(buffer) {
  return [...new Uint8Array(buffer)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

// The program of the disc: its offset is in the header, its size is the end of its last section.
async function readDol(file) {
  const offset = be32(await readBytes(file, 0x420, 4), 0);
  const header = await readBytes(file, offset, 0x100);
  let end = 0x100;
  for (let i = 0; i < 18; i++) {
    const at = be32(header, i * 4);
    const size = be32(header, 0x90 + i * 4);
    if (size) end = Math.max(end, at + size);
  }
  if (offset + end > file.size) {
    const error = new Error('the disc image is incomplete (its program is cut short)');
    error.code = 'cutShort';
    throw error;
  }
  return readBytes(file, offset, end);
}

// Returns { kind, id, revision }, where kind is:
//   usa      Luigi's Mansion (USA), GLME01: the game the port runs
//   pal      Luigi's Mansion (Europe), GLMP01 first release: adds the European Hidden Mansion
//   palRev1  the European revision 1, which the port cannot run
//   other    another disc, or a modified copy of the game
// Throws when the file is not a GameCube disc image.
export async function identifyDisc(file) {
  const { id, revision } = await readDiscHeader(file);
  if (file.size !== DISC_SIZE) {
    const error = new Error('trimmed or compressed disc image');
    error.code = 'notFullSize';
    throw error;
  }
  const kind = DOL_SHA1[hex(await crypto.subtle.digest('SHA-1', await readDol(file)))];
  if (kind === 'usa' && id === 'GLME01') return { kind, id, revision };
  if (kind === 'pal' && id === 'GLMP01') return { kind, id, revision };
  if (id === 'GLMP01' && revision === 1) return { kind: 'palRev1', id, revision };
  return { kind: 'other', id, revision };
}

// The disc and its file table, as the Hidden Mansion builder reads them.
export async function openDisc(file) {
  return { blob: file, files: await listGcmFiles(file) };
}
