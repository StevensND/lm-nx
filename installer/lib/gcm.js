// Reads the file system (FST) of a GameCube disc image (.iso / .gcm) through a Blob/File, without
// loading the image into memory.

async function readBytes(blob, offset, length) {
  return new Uint8Array(await blob.slice(offset, offset + length).arrayBuffer());
}

function be32(bytes, at) {
  return ((bytes[at] << 24) | (bytes[at + 1] << 16) | (bytes[at + 2] << 8) | bytes[at + 3]) >>> 0;
}

// Game id (e.g. GLME01) and disc revision from the disc header.
export async function readDiscHeader(blob) {
  if (blob.size < 0x440) {
    throw new Error('this file is too small to be a GameCube disc image');
  }
  const header = await readBytes(blob, 0, 0x440);
  if (be32(header, 0x1c) !== 0xc2339f3d) {
    throw new Error('this file is not a GameCube disc image (.iso / .gcm)');
  }
  let id = '';
  for (let i = 0; i < 6; i++) {
    id += String.fromCharCode(header[i]);
  }
  return { id, revision: header[7] };
}

// Map of "/Path/name" -> { offset, size } for every file on the disc.
export async function listGcmFiles(blob) {
  const header = await readBytes(blob, 0x424, 8);
  const fstOffset = be32(header, 0);
  const fstSize = be32(header, 4);
  if (fstOffset + fstSize > blob.size || fstSize < 12) {
    throw new Error('the disc image is incomplete (file table out of range)');
  }
  const fst = await readBytes(blob, fstOffset, fstSize);
  const count = be32(fst, 8);
  const strings = count * 12;
  const name = (offset) => {
    let s = '';
    for (let i = strings + offset; i < fst.length && fst[i] !== 0; i++) {
      s += String.fromCharCode(fst[i]);
    }
    return s;
  };
  const files = new Map();
  const walk = (first, end, prefix) => {
    let i = first;
    while (i < end) {
      const word = be32(fst, i * 12);
      const a = be32(fst, i * 12 + 4);
      const b = be32(fst, i * 12 + 8);
      const entryName = name(word & 0xffffff);
      if (word >>> 24) {
        walk(i + 1, b, `${prefix}${entryName}/`);
        i = b;
      } else {
        files.set(`${prefix}${entryName}`, { offset: a, size: b });
        i++;
      }
    }
  };
  walk(1, count, '/');
  return files;
}

export async function readGcmFile(blob, files, path) {
  const entry = files.get(path);
  if (!entry) {
    throw new Error(`${path} is missing from the disc image`);
  }
  return readBytes(blob, entry.offset, entry.size);
}
