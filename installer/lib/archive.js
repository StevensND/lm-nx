// Nintendo archive formats: Yay0 / Yaz0 compression and RARC archives (as tools/rarc.py and
// tools/hidden_mansion_data.py in the port's repository).

const sjis = new TextDecoder('shift_jis');

function be16(b, at) {
  return (b[at] << 8) | b[at + 1];
}

function be32(b, at) {
  return ((b[at] << 24) | (b[at + 1] << 16) | (b[at + 2] << 8) | b[at + 3]) >>> 0;
}

function put32(b, at, v) {
  b[at] = v >>> 24;
  b[at + 1] = (v >>> 16) & 0xff;
  b[at + 2] = (v >>> 8) & 0xff;
  b[at + 3] = v & 0xff;
}

function magic(b) {
  return String.fromCharCode(b[0], b[1], b[2], b[3]);
}

export function yay0(data) {
  const size = be32(data, 4);
  let linkOff = be32(data, 8);
  let chunkOff = be32(data, 12);
  const out = new Uint8Array(size);
  let o = 0;
  let maskOff = 16;
  let bits = 0;
  let mask = 0;
  while (o < size) {
    if (bits === 0) {
      mask = be32(data, maskOff);
      maskOff += 4;
      bits = 32;
    }
    if (mask & 0x80000000) {
      out[o++] = data[chunkOff++];
    } else {
      const link = be16(data, linkOff);
      linkOff += 2;
      const dist = (link & 0xfff) + 1;
      let n = link >> 12;
      n = n === 0 ? data[chunkOff++] + 0x12 : n + 2;
      for (let i = 0; i < n && o < size; i++, o++) {
        out[o] = out[o - dist];
      }
    }
    mask = (mask << 1) >>> 0;
    bits--;
  }
  return out;
}

export function yaz0(data) {
  const size = be32(data, 4);
  const out = new Uint8Array(size);
  let o = 0;
  let src = 16;
  while (o < size) {
    const code = data[src++];
    for (let bit = 0; bit < 8 && o < size; bit++) {
      if (code & (0x80 >> bit)) {
        out[o++] = data[src++];
      } else {
        const b1 = data[src];
        const b2 = data[src + 1];
        src += 2;
        const dist = (((b1 & 0xf) << 8) | b2) + 1;
        let n = b1 >> 4;
        n = n === 0 ? data[src++] + 0x12 : n + 2;
        for (let i = 0; i < n && o < size; i++, o++) {
          out[o] = out[o - dist];
        }
      }
    }
  }
  return out;
}

export function decompress(data) {
  const m = magic(data);
  if (m === 'Yay0') return yay0(data);
  if (m === 'Yaz0') return yaz0(data);
  return data;
}

// Wraps data in a valid Yay0 stream made only of literal bytes (no back-references), so the
// game's loader sees the same container format as the original file.
export function yay0Store(data) {
  const words = Math.ceil(data.length / 32);
  const linkOff = 16 + 4 * words;
  const out = new Uint8Array(linkOff + data.length);
  out.set([0x59, 0x61, 0x79, 0x30]);
  put32(out, 4, data.length);
  put32(out, 8, linkOff);
  put32(out, 12, linkOff);
  out.fill(0xff, 16, linkOff);
  out.set(data, linkOff);
  return out;
}

function rarcLayout(arc) {
  if (magic(arc) !== 'RARC') {
    throw new Error('not a RARC archive');
  }
  const info = 0x20;
  return {
    dataOff: be32(arc, 0x0c) + 0x20,
    nodeOff: be32(arc, info + 4) + info,
    entryOff: be32(arc, info + 12) + info,
    strOff: be32(arc, info + 20) + info,
  };
}

// Calls visit(path, entryOffset, flags, dataOffset, size) for every file, in archive order.
function rarcWalk(arc, visit) {
  const { nodeOff, entryOff, strOff } = rarcLayout(arc);
  const name = (off) => {
    let end = strOff + off;
    while (arc[end] !== 0) end++;
    return sjis.decode(arc.subarray(strOff + off, end));
  };
  const walk = (node, prefix) => {
    const count = be16(arc, nodeOff + node * 16 + 10);
    const first = be32(arc, nodeOff + node * 16 + 12);
    for (let i = first; i < first + count; i++) {
      const e = entryOff + i * 20;
      const flagsName = be32(arc, e + 4);
      const entryName = name(flagsName & 0xffffff);
      if (entryName === '.' || entryName === '..') continue;
      if ((flagsName >>> 24) & 0x02) {
        walk(be32(arc, e + 8), `${prefix}${entryName}/`);
      } else {
        visit(`${prefix}${entryName}`, e, flagsName >>> 24, be32(arc, e + 8), be32(arc, e + 12));
      }
    }
  };
  walk(0, '');
}

// Map of path -> bytes (views into the archive) for every file.
export function rarcFiles(arc) {
  const { dataOff } = rarcLayout(arc);
  const files = new Map();
  rarcWalk(arc, (path, e, flags, a, size) => files.set(path, arc.subarray(dataOff + a, dataOff + a + size)));
  return files;
}

// A copy of an (uncompressed) RARC archive with one file's contents replaced. The new data is
// appended to the data section and the entry pointed at it; the tables are left untouched, so
// ids and names stay the same.
export function rarcReplace(arc, member, data) {
  const { dataOff } = rarcLayout(arc);
  const dataLen = be32(arc, 0x10);
  const mram = be32(arc, 0x14);
  const aram = be32(arc, 0x18);
  let target = null;
  rarcWalk(arc, (path, e, flags, a, size) => {
    if (path === member) target = { e, flags };
  });
  if (!target) {
    throw new Error(`${member} is missing from the archive`);
  }
  const base = dataOff + dataLen;
  const start = base + ((32 - (base % 32)) % 32);
  const end = start + data.length;
  const total = end + ((32 - (end % 32)) % 32);
  const out = new Uint8Array(total);
  out.set(arc.subarray(0, base));
  out.set(data, start);
  const grow = total - base;
  put32(out, target.e + 8, start - dataOff);
  put32(out, target.e + 12, data.length);
  put32(out, 0x04, total);
  put32(out, 0x10, dataLen + grow);
  if (target.flags & 0x10) {
    put32(out, 0x14, mram + grow); // preloaded to main RAM
  } else if (target.flags & 0x20) {
    put32(out, 0x18, aram + grow); // preloaded to ARAM
  }
  return out;
}
