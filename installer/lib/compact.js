// A smaller copy of a GameCube disc image: the same system area (header, apploader, main.dol and file table) and the
// same files, one after the other, without the filler data the original disc puts between them (most of a Luigi's
// Mansion disc). Only the file table changes: each file gets its new position. The game finds every file through
// that table, so it reads this copy like the original one; main.dol is unchanged, so the game still recognises it.
//
// The copy is never held in memory: it is a list of pieces of the original image and of new bytes, read through the
// same size/slice() calls as a File, so the zip takes it like any other file.

// Files start on 32-byte boundaries, the alignment the GameCube's disc reads ask for. A real disc uses 32 KiB, which
// here would add about 14 MB of padding to each disc; the port reads the image from the SD card, not from a drive.
const ALIGN = 32;

function be32(bytes, at) {
  return ((bytes[at] << 24) | (bytes[at + 1] << 16) | (bytes[at + 2] << 8) | bytes[at + 3]) >>> 0;
}

function setBe32(bytes, at, value) {
  bytes[at] = value >>> 24;
  bytes[at + 1] = (value >>> 16) & 0xff;
  bytes[at + 2] = (value >>> 8) & 0xff;
  bytes[at + 3] = value & 0xff;
}

async function readBytes(blob, offset, length) {
  return new Uint8Array(await blob.slice(offset, offset + length).arrayBuffer());
}

const alignUp = (value) => Math.ceil(value / ALIGN) * ALIGN;

class CompactImage {
  // pieces: [{ at, size, from }] (a piece of the original image) or [{ at, size, bytes }], sorted by `at`; the gaps
  // between them are zeros.
  constructor(source, pieces, size) {
    this.name = source.name;
    this.source = source;
    this.pieces = pieces;
    this.size = size;
  }

  async read(offset, length) {
    const out = new Uint8Array(length);
    const end = offset + length;
    // the first piece that ends after `offset`
    let lo = 0;
    let hi = this.pieces.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      const p = this.pieces[mid];
      if (p.at + p.size <= offset) lo = mid + 1;
      else hi = mid;
    }
    for (let i = lo; i < this.pieces.length && this.pieces[i].at < end; i++) {
      const p = this.pieces[i];
      const from = Math.max(offset, p.at);
      const to = Math.min(end, p.at + p.size);
      const data = p.bytes
        ? p.bytes.subarray(from - p.at, to - p.at)
        : await readBytes(this.source, p.from + (from - p.at), to - from);
      out.set(data, from - offset);
    }
    return out;
  }

  slice(start = 0, end = this.size) {
    start = Math.max(0, Math.min(start, this.size));
    end = Math.max(start, Math.min(end, this.size));
    return { size: end - start, arrayBuffer: async () => (await this.read(start, end - start)).buffer };
  }
}

// The smaller copy of `image` (a File or an image read by disc_image.js), or `image` itself if its layout is not the
// usual one (the system area must come before every file).
export async function compactDisc(image) {
  const header = await readBytes(image, 0x420, 12);
  const dolOffset = be32(header, 0);
  const fstOffset = be32(header, 4);
  const fstSize = be32(header, 8);
  const fst = await readBytes(image, fstOffset, fstSize);
  const count = be32(fst, 8);
  if (count * 12 > fst.length) return image;

  // main.dol's size: the end of its last section
  const dolHeader = await readBytes(image, dolOffset, 0x100);
  let dolEnd = 0x100;
  for (let i = 0; i < 18; i++) {
    const size = be32(dolHeader, 0x90 + i * 4);
    if (size) dolEnd = Math.max(dolEnd, be32(dolHeader, i * 4) + size);
  }
  const systemEnd = Math.max(dolOffset + dolEnd, fstOffset + fstSize);

  const files = [];
  for (let i = 1; i < count; i++) {
    if (!(be32(fst, i * 12) >>> 24)) {
      files.push({ entry: i, offset: be32(fst, i * 12 + 4), size: be32(fst, i * 12 + 8) });
    }
  }
  if (files.some((f) => f.size && f.offset < systemEnd)) return image;

  // the files in their original order, each on the next boundary; the table is rewritten with their new positions
  const newFst = fst.slice();
  const pieces = [];
  let at = alignUp(systemEnd);
  for (const f of [...files].sort((a, b) => a.offset - b.offset)) {
    setBe32(newFst, f.entry * 12 + 4, f.size ? at : 0);
    if (f.size) {
      pieces.push({ at, size: f.size, from: f.offset });
      at = alignUp(at + f.size);
    }
  }
  const size = at;
  if (size >= image.size) return image;

  // the system area as it was, except the file table
  const system = [];
  if (fstOffset > 0) system.push({ at: 0, size: fstOffset, from: 0 });
  system.push({ at: fstOffset, size: fstSize, bytes: newFst });
  if (systemEnd > fstOffset + fstSize) {
    system.push({ at: fstOffset + fstSize, size: systemEnd - fstOffset - fstSize, from: fstOffset + fstSize });
  }
  return new CompactImage(image, [...system, ...pieces], size);
}
