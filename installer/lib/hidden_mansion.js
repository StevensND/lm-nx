// Builds the Hidden Mansion overlay from the player's own USA and PAL discs, as
// tools/hidden_mansion_data.py in the port's repository does (docs/HIDDEN_MANSION.md there):
//
//   Map/map2u.szp          the PAL Hidden Mansion map, unchanged; replaces /Map/map2.szp while the
//                          Hidden Mansion flag is set.
//   Event/eventNN.szp      the USA archive with only its event script swapped for the PAL one.
//   Event/event08.szp      E. Gadd's lab with a follow-up choice "The Hidden Mansion (USA)" / "(PAL)".
//   manifest.txt           one "guest path -> overlay file [condition]" line per redirect.
//   usa/...                the reverse: the USA Hidden Mansion for the PAL game.
//
// Nothing here comes from the port's repository: every byte is taken from the player's discs.

import { readGcmFile } from './gcm.js';
import { decompress, rarcFiles, rarcReplace, yay0Store } from './archive.js';

// Event scripts the PAL release changed for the Hidden Mansion.
const EVENTS = ['08', '16', '73'];

// Game flag 8 is the PAL release's own Hidden Mansion save flag; the port reads the variant from it.
const LAB_FLAG_PAL = 8;
// "The Hidden Mansion" in the lab's message table (every language).
const LAB_HIDDEN_LINE = 38;
const LAB_SUFFIX = [bytes(' (USA)'), bytes(' (PAL)')];
const LAB_SUFFIX_JP = [hex('8169827482728260816a'), hex('8169826f8260826b816a')];

const CRLF = bytes('\r\n');

function bytes(s) {
  return Uint8Array.from(s, (c) => c.charCodeAt(0));
}

function hex(s) {
  return Uint8Array.from(s.match(/../g), (h) => parseInt(h, 16));
}

function concat(parts) {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const p of parts) {
    out.set(p, at);
    at += p.length;
  }
  return out;
}

function equal(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

function startsWith(a, prefix) {
  return a.length >= prefix.length && equal(a.subarray(0, prefix.length), prefix);
}

function split(data, sep) {
  const parts = [];
  let start = 0;
  for (let i = 0; i + sep.length <= data.length; ) {
    if (equal(data.subarray(i, i + sep.length), sep)) {
      parts.push(data.subarray(start, i));
      i += sep.length;
      start = i;
    } else {
      i++;
    }
  }
  parts.push(data.subarray(start));
  return parts;
}

function join(parts, sep) {
  const out = [];
  parts.forEach((p, i) => {
    if (i) out.push(sep);
    out.push(p);
  });
  return concat(out);
}

function tableEntries(table) {
  const entries = split(table, CRLF);
  while (entries.length && entries[entries.length - 1].length === 0) entries.pop();
  return entries;
}

// A message table with entries n and n+1 = "<Hidden Mansion line> (USA)" / "(PAL)" appended;
// shorter tables are padded with empty entries so the indices match in every language.
function labTable(table, n, japanese) {
  const entries = tableEntries(table);
  if (entries.length > n || entries.length <= LAB_HIDDEN_LINE) {
    throw new Error('unexpected message table in event08');
  }
  let base = entries[LAB_HIDDEN_LINE];
  let end = base.length;
  while (end > 0 && (base[end - 1] === 0x2e || base[end - 1] === 0x20)) end--;
  base = base.subarray(0, end);
  while (entries.length < n) entries.push(new Uint8Array(0));
  for (const suffix of japanese ? LAB_SUFFIX_JP : LAB_SUFFIX) entries.push(concat([base, suffix]));
  return concat([join(entries, CRLF), CRLF, CRLF]);
}

// The event08 script with the "urayasiki" case turned into a USA/PAL follow-up choice.
function labScript(script, n) {
  const lines = split(script, CRLF);
  const caseLine = bytes('<CASE>"urayasiki"');
  const start = lines.findIndex((l) => equal(l, caseLine));
  if (start < 0) throw new Error('unexpected event08 script');
  let end = start + 1;
  while (!startsWith(lines[end], bytes('<END>'))) end++;
  const body = lines
    .slice(start + 1, end + 1)
    .filter((l) => !startsWith(l, bytes('<FLAGON>(8)')) && !startsWith(l, bytes('<FLAGOFF>(8)')));
  const ura = body.findIndex((l) => equal(l, bytes('<URALUIGI>')));
  const branch = (label, pal) => [
    bytes(`<CASE>"${label}"`),
    ...body.slice(0, ura),
    bytes(`${pal ? '<FLAGON>' : '<FLAGOFF>'}(${LAB_FLAG_PAL})`),
    ...body.slice(ura),
  ];
  const choice = [caseLine, bytes(`<CHOICE>"ura_us"(${n})"ura_pal"(${n + 1})<LISTEND>`), bytes('<ANYKEY>')];
  return join([...lines.slice(0, start), ...choice, ...branch('ura_us', false), ...branch('ura_pal', true), ...lines.slice(end + 1)], CRLF);
}

function labArchive(arc, palScript) {
  const files = rarcFiles(arc);
  const tables = [...files.keys()].filter((k) => k.startsWith('message/'));
  const n = Math.max(...tables.map((k) => tableEntries(files.get(k)).length));
  let out = rarcReplace(arc, 'text/event08.txt', labScript(palScript, n));
  for (const k of tables) {
    out = rarcReplace(out, k, labTable(files.get(k), n, k.includes('japanese')));
  }
  const check = rarcFiles(out);
  for (const [k, v] of files) {
    if (!check.has(k) || (!tables.includes(k) && k !== 'text/event08.txt' && !equal(check.get(k), v))) {
      throw new Error('event08 could not be rebuilt');
    }
  }
  return out;
}

// us / pal: { blob, files } of each disc (listGcmFiles). Returns [{ path, data }] relative to the
// overlay folder (hidden_mansion/).
export async function buildHiddenMansion(us, pal, log = () => {}) {
  const read = (disc, path) => readGcmFile(disc.blob, disc.files, path);
  const out = [];

  const swapScripts = async (base, from, prefix, manifest) => {
    for (const ev of EVENTS) {
      const name = `event${ev}.szp`;
      const member = `text/event${ev}.txt`;
      const u = decompress(await read(base, `/Event/${name}`));
      const p = rarcFiles(decompress(await read(from, `/Event/${name}`)));
      const cur = rarcFiles(u);
      if (ev === '08') {
        // The lab menu is served whichever version is chosen, so it can ask. Its script is the
        // European one (which keeps flag 8 in step) on both games.
        const palScript = (prefix === '' ? p : cur).get(member);
        out.push({ path: `${prefix}Event/${name}`, data: yay0Store(labArchive(u, palScript)) });
        manifest.push(`/Event/${name} -> ${prefix}Event/${name} [always]`);
        continue;
      }
      if (equal(cur.get(member), p.get(member))) continue;
      const patched = rarcReplace(u, member, p.get(member));
      out.push({ path: `${prefix}Event/${name}`, data: yay0Store(patched) });
      manifest.push(`/Event/${name} -> ${prefix}Event/${name}`);
    }
  };

  const manifest = ['/Map/map2.szp -> Map/map2u.szp [hidden]'];
  log('Hidden Mansion: PAL map');
  out.push({ path: 'Map/map2u.szp', data: await read(pal, '/Map/map2u.szp') });
  await swapScripts(us, pal, '', manifest);
  out.push({ path: 'manifest.txt', data: bytes(manifest.join('\r\n') + '\r\n') });

  // Reverse: the USA Hidden Mansion on the European game.
  const reverse = ['/Map/map2u.szp -> usa/Map/map2.szp [hidden]'];
  log('Hidden Mansion: USA map');
  out.push({ path: 'usa/Map/map2.szp', data: await read(us, '/Map/map2.szp') });
  await swapScripts(pal, us, 'usa/', reverse);
  out.push({ path: 'usa/manifest.txt', data: bytes(reverse.join('\r\n') + '\r\n') });
  return out;
}
