// Compressed GameCube disc images (RVZ, WIA, GCZ and CISO, the formats of the Dolphin emulator) read as if they were
// the full .iso: the rest of the page reads them through the same size/slice() calls as a file, and each block is
// decompressed when it is read. Nothing is converted or saved first. Written from the format descriptions (for WIA
// and RVZ, docs/WiaAndRvz.md in Dolphin's repository), with every compression Dolphin offers for them.

// A GameCube disc. CISO does not store the size of the disc it was made from.
const GAMECUBE_DISC_SIZE = 1459978240;

const be32 = (b, at) => ((b[at] << 24) | (b[at + 1] << 16) | (b[at + 2] << 8) | b[at + 3]) >>> 0;
const be64 = (b, at) => be32(b, at) * 2 ** 32 + be32(b, at + 4);
const le32 = (b, at) => (b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (b[at + 3] << 24)) >>> 0;
const le64 = (b, at) => le32(b, at) + le32(b, at + 4) * 2 ** 32;

async function readBytes(blob, offset, length) {
  return new Uint8Array(await blob.slice(offset, offset + length).arrayBuffer());
}

function failure(code, message) {
  const error = new Error(message);
  error.code = code;
  return error;
}

// The decompressors (lib/codecs.wasm: zstd 1.5.7, the LZMA SDK 26.05 decoders and bzip2 1.0.8, built by
// tools/build_disc_codecs_web.py of lm-nx). One buffer each for the input and the output, grown when needed.
let codecModule;
function codecs() {
  if (!codecModule) {
    codecModule = (async () => {
      const bytes = await (await fetch(new URL('./codecs.wasm', import.meta.url))).arrayBuffer();
      const { instance } = await WebAssembly.instantiate(bytes, { env: { emscripten_notify_memory_growth() {} } });
      instance.exports._initialize();
      const props = instance.exports.zw_alloc(8);
      return { exports: instance.exports, props, src: 0, srcSize: 0, dst: 0, dstSize: 0 };
    })();
  }
  return codecModule;
}

// method: 'zstd', 'bzip2', 'LZMA' (props: its 5 property bytes) or 'LZMA2' (props: its dictionary byte).
async function decompress(method, data, capacity, props) {
  const c = await codecs();
  const e = c.exports;
  if (c.srcSize < data.length) {
    if (c.src) e.zw_free(c.src);
    c.srcSize = data.length;
    c.src = e.zw_alloc(c.srcSize);
  }
  if (c.dstSize < capacity) {
    if (c.dst) e.zw_free(c.dst);
    c.dstSize = capacity;
    c.dst = e.zw_alloc(c.dstSize);
  }
  if (!c.src || !c.dst) throw failure('badImage', 'out of memory');
  new Uint8Array(e.memory.buffer, c.src, data.length).set(data);
  let size;
  if (method === 'zstd') {
    size = e.zw_zstd(c.dst, capacity, c.src, data.length);
  } else if (method === 'bzip2') {
    size = e.zw_bzip2(c.dst, capacity, c.src, data.length);
  } else if (method === 'LZMA') {
    new Uint8Array(e.memory.buffer, c.props, 5).set(props.subarray(0, 5));
    size = e.zw_lzma(c.dst, capacity, c.src, data.length, c.props);
  } else {
    size = e.zw_lzma2(c.dst, capacity, c.src, data.length, props[0]);
  }
  if (size < 0) throw failure('badImage', `a block could not be decompressed (${method})`);
  return new Uint8Array(e.memory.buffer, c.dst, size).slice();
}

async function inflate(data) {
  try {
    const stream = new Blob([data]).stream().pipeThrough(new DecompressionStream('deflate'));
    return new Uint8Array(await new Response(stream).arrayBuffer());
  } catch {
    throw failure('badImage', 'a block could not be decompressed (zlib)');
  }
}

// The junk that fills the unused space of a disc, as the console's mastering tools make it: a lagged Fibonacci
// generator (k = 521, j = 32) started from a 17-word seed. RVZ stores the seed instead of the junk.
class Junk {
  constructor() {
    this.words = new Uint32Array(521);
    this.bytes = new Uint8Array(this.words.buffer);
    this.position = 0;
  }

  seed(seed) {
    const w = this.words;
    for (let i = 0; i < 17; i++) w[i] = be32(seed, i * 4);
    for (let i = 17; i < 521; i++) w[i] = (w[i - 17] << 23) ^ (w[i - 16] >>> 9) ^ w[i - 1];
    // the output takes bits 18-25 of each word for its second byte, and the words go out in big-endian order
    const view = new DataView(this.words.buffer);
    for (let i = 0; i < 521; i++) view.setUint32(i * 4, ((w[i] & 0xff00ffff) | ((w[i] >>> 2) & 0x00ff0000)) >>> 0);
    for (let i = 0; i < 4; i++) this.forward();
    this.position = 0;
  }

  forward() {
    const w = this.words;
    for (let i = 0; i < 32; i++) w[i] ^= w[i + 489];
    for (let i = 32; i < 521; i++) w[i] ^= w[i - 32];
  }

  skip(count) {
    this.position += count;
    while (this.position >= 2084) {
      this.forward();
      this.position -= 2084;
    }
  }

  fill(out, at, count) {
    while (count > 0) {
      const length = Math.min(count, 2084 - this.position);
      out.set(this.bytes.subarray(this.position, this.position + length), at);
      this.position += length;
      at += length;
      count -= length;
      if (this.position === 2084) {
        this.forward();
        this.position = 0;
      }
    }
  }
}

// A disc read in pieces: piece(at) tells which piece holds the byte at `at` ({ key, start, end, load }, where load()
// gives at least end - start bytes, or no load for a run of zeros). The last few pieces are kept, as the page reads the
// disc in pieces that rarely line up with them. A piece that comes back short is a damaged image, never padded.
class PieceImage {
  constructor(file, format, size) {
    this.name = file.name;
    this.format = format;
    this.size = size;
    this.cache = new Map();
    this.header = null;  // WIA and RVZ keep the first 0x80 bytes of the disc apart
  }

  cached(key, load) {
    let entry = this.cache.get(key);
    if (entry) {
      this.cache.delete(key);
    } else {
      entry = load();
      if (this.cache.size >= 8) this.cache.delete(this.cache.keys().next().value);
    }
    this.cache.set(key, entry);
    return entry;
  }

  async read(offset, length) {
    const out = new Uint8Array(length);
    let done = 0;
    while (done < length) {
      const at = offset + done;
      const p = this.piece(at);
      const count = Math.min(length - done, p.end - at);
      if (count <= 0) throw failure('badImage', `nothing describes the disc at ${at}`);
      if (p.load) {
        const data = await this.cached(p.key, p.load);
        if (data.length < p.end - p.start) throw failure('badImage', `a block at ${p.start} is incomplete`);
        out.set(data.subarray(at - p.start, at - p.start + count), done);
      }
      done += count;
    }
    if (this.header && offset < this.header.length) {
      out.set(this.header.subarray(offset, Math.min(this.header.length, offset + length)));
    }
    return out;
  }

  // the part of the File interface the page uses; a damaged image says which file it is
  slice(start = 0, end = this.size) {
    start = Math.max(0, Math.min(start, this.size));
    end = Math.max(start, Math.min(end, this.size));
    const read = async () => {
      try {
        return (await this.read(start, end - start)).buffer;
      } catch (error) {
        error.disc ??= this.name;
        throw error;
      }
    };
    return { size: end - start, arrayBuffer: read };
  }
}

// Fixed-size blocks: readBlock(index) gives the function that loads the block, or null for an empty (zero) one.
function fixedBlocks(image, blockSize, readBlock) {
  image.piece = (at) => {
    const index = Math.floor(at / blockSize);
    const start = index * blockSize;
    return { key: index, start, end: Math.min(start + blockSize, image.size), load: readBlock(index) };
  };
  return image;
}

function adler32(data) {
  let a = 1;
  let b = 0;
  for (let i = 0; i < data.length; ) {
    const end = Math.min(data.length, i + 5552);  // the most bytes before the sums can overflow
    for (; i < end; i++) {
      a += data[i];
      b += a;
    }
    a %= 65521;
    b %= 65521;
  }
  return ((b << 16) | a) >>> 0;
}

// GCZ: zlib blocks with a table of where each one is and an Adler-32 of each stored block (little-endian).
async function openGcz(file, head) {
  const size = le64(head, 16);
  const blockSize = le32(head, 24);
  const count = le32(head, 28);
  const table = await readBytes(file, 32, count * 12);
  if (!blockSize || table.length < count * 12 || count * blockSize < size) throw failure('badImage', 'bad GCZ header');
  const dataStart = 32 + count * 12;
  const compressedSize = le64(head, 8);
  const at = (i) => le32(table, i * 8) + (le32(table, i * 8 + 4) & 0x7fffffff) * 2 ** 32;
  return fixedBlocks(new PieceImage(file, 'GCZ', size), blockSize, (i) => async () => {
    const stored = (table[i * 8 + 7] & 0x80) !== 0;  // a block that did not get smaller is kept as it is
    const end = i + 1 < count ? at(i + 1) : compressedSize;
    const data = await readBytes(file, dataStart + at(i), end - at(i));
    if (data.length !== end - at(i)) throw failure('badImage', `GCZ block ${i} is cut short`);
    if (adler32(data) !== le32(table, count * 8 + i * 4)) throw failure('badImage', `GCZ block ${i} is damaged`);
    return stored ? data : inflate(data);
  });
}

// CISO: a map of which blocks are stored; the ones that are not are empty.
async function openCiso(file, head) {
  const blockSize = le32(head, 4);
  if (!blockSize) throw failure('badImage', 'bad CISO header');
  const map = await readBytes(file, 8, 0x8000 - 8);
  const where = new Int32Array(map.length).fill(-1);
  let stored = 0;
  for (let i = 0; i < map.length; i++) if (map[i]) where[i] = stored++;
  return fixedBlocks(new PieceImage(file, 'CISO', GAMECUBE_DISC_SIZE), blockSize,
    (i) => (i < where.length && where[i] >= 0 ? () => readBytes(file, 0x8000 + where[i] * blockSize, blockSize) : null));
}

// WIA and RVZ (big-endian): the disc in groups of chunk_size bytes, each group compressed on its own. RVZ also
// replaces the junk filler with the seed that regenerates it.
const COMPRESSION = ['none', 'purge', 'bzip2', 'LZMA', 'LZMA2', 'zstd'];

async function openWiaRvz(file, head, rvz) {
  const header2 = await readBytes(file, 0x48, be32(head, 0x0c));
  const size = be64(head, 0x24);
  const compression = be32(header2, 4);
  const chunkSize = be32(header2, 0x0c);
  if (be32(header2, 0) !== 1) throw failure('notGameCube', 'not a GameCube disc');
  // purge (WIA only) is a format of its own that Dolphin does not write
  if (compression === 1 || !COMPRESSION[compression]) {
    throw failure('unsupportedCompression', `compressed with ${COMPRESSION[compression] || compression}`);
  }
  if (!chunkSize) throw failure('badImage', 'bad WIA/RVZ header');
  const props = header2.subarray(0xd5, 0xd5 + header2[0xd4]);
  const unpack = (data, capacity) => (compression ? decompress(COMPRESSION[compression], data, capacity, props) : data);

  const rawCount = be32(header2, 0xb4);
  const raw = await unpack(await readBytes(file, be64(header2, 0xb8), be32(header2, 0xc0)), rawCount * 0x18);
  const groupCount = be32(header2, 0xc4);
  const entrySize = rvz ? 12 : 8;
  const groups = await unpack(await readBytes(file, be64(header2, 0xc8), be32(header2, 0xd0)), groupCount * entrySize);
  if (raw.length < rawCount * 0x18 || groups.length < groupCount * entrySize) throw failure('badImage', 'bad WIA/RVZ tables');
  const entries = [];
  for (let i = 0; i < rawCount; i++) {
    const offset = be64(raw, i * 0x18);
    // a raw data entry's groups start at a multiple of 0x8000: its first group also holds the bytes before it
    const start = offset - (offset % 0x8000);
    entries.push({ start, end: offset + be64(raw, i * 0x18 + 8), group: be32(raw, i * 0x18 + 0x10) });
  }
  entries.sort((a, b) => a.start - b.start);

  const junk = new Junk();
  // group g of the image, which holds `length` bytes of the disc from `start`
  const loadGroup = async (g, start, length) => {
    if (g >= groupCount) throw failure('badImage', `group ${g} is missing`);
    const offset = be32(groups, g * entrySize) * 4;
    const sizeField = be32(groups, g * entrySize + 4);
    const stored = rvz ? sizeField & 0x7fffffff : sizeField;
    const out = new Uint8Array(length);
    if (!stored) return out;  // a group of zeros
    let data = await readBytes(file, offset, stored);
    if (data.length !== stored) throw failure('badImage', `group ${g} is cut short`);
    const packed = rvz ? be32(groups, g * entrySize + 8) : 0;
    if (!rvz || sizeField & 0x80000000) data = await unpack(data, packed || chunkSize);
    if (!packed) {
      if (data.length < length) throw failure('badImage', `group ${g} is incomplete`);
      out.set(data.subarray(0, length));
      return out;
    }
    // RVZ packing: runs of data and runs of junk, each with its size (the top bit marks junk, then its seed)
    let at = 0;
    let pos = 0;
    while (at < length) {
      if (pos + 4 > data.length) throw failure('badImage', `group ${g} is incomplete`);
      let run = be32(data, pos);
      pos += 4;
      const isJunk = run & 0x80000000;
      run &= 0x7fffffff;
      const count = Math.min(run, length - at);
      if (isJunk) {
        if (pos + 68 > data.length) throw failure('badImage', `group ${g} is incomplete`);
        junk.seed(data.subarray(pos, pos + 68));
        pos += 68;
        junk.skip((start + at) % 0x8000);
        junk.fill(out, at, count);
      } else {
        if (pos + count > data.length) throw failure('badImage', `group ${g} is incomplete`);
        out.set(data.subarray(pos, pos + count), at);
        pos += run;
      }
      at += run;
    }
    return out;
  };

  const image = new PieceImage(file, rvz ? 'RVZ' : 'WIA', size);
  image.header = header2.slice(0x10, 0x90);
  image.piece = (at) => {
    const i = entries.findIndex((e) => at >= e.start && at < e.end);
    if (i < 0) {
      // between raw data entries (or after the last one): zeros up to the next one
      const next = entries.find((e) => e.start > at);
      return { start: at, end: next ? next.start : size };
    }
    const e = entries[i];
    const n = Math.floor((at - e.start) / chunkSize);
    const start = e.start + n * chunkSize;
    const end = Math.min(start + chunkSize, e.end, size);
    return { key: `${i}:${n}`, start, end, load: () => loadGroup(e.group + n, start, end - start) };
  };
  return image;
}

// The file itself when it is a plain disc image, or a reader of the compressed one.
export async function openDiscImage(file) {
  const head = await readBytes(file, 0, 0x48);
  if (head.length >= 8) {
    if (le32(head, 0) === 0xb10bc001) return openGcz(file, head);
    if (head[0] === 0x43 && head[1] === 0x49 && head[2] === 0x53 && head[3] === 0x4f) return openCiso(file, head);
    const magic = be32(head, 0);
    if (magic === 0x57494101) return openWiaRvz(file, head, false);
    if (magic === 0x52565a01) return openWiaRvz(file, head, true);
  }
  return file;
}
