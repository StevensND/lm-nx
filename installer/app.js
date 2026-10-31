import { identifyDisc, openDisc } from './lib/disc.js';
import { openDiscImage } from './lib/disc_image.js';
import { buildHiddenMansion } from './lib/hidden_mansion.js';
import { packageEntries, packageSize, writePackage } from './lib/package.js';
import { buildShaderCaches } from './lib/shaders.js';
import { FLAGS } from './lib/flags.js';
import { LANGUAGES, formatSize, getLanguage, preferredLanguage, setLanguage, t } from './lib/i18n.js';

const create = document.getElementById('create');
const update = document.getElementById('update');
const work = document.getElementById('work');
const what = document.getElementById('what');
const fill = document.getElementById('fill');
const percent = document.getElementById('percent');
const logBox = document.getElementById('log');
const languages = document.getElementById('languages');
const LANGUAGE_KEY = 'lm-nx.language';
const menu = document.getElementById('menu');
const menuButton = document.getElementById('menuButton');
const menuClose = document.getElementById('menuClose');
const menuBackdrop = document.getElementById('menuBackdrop');

// The side menu: the three-line button opens it; the cross, the dark backdrop, Escape or choosing a link close it.
function setMenuOpen(open) {
  document.body.classList.toggle('menu-open', open);
  menuButton.setAttribute('aria-expanded', String(open));
  if (open) {
    menuClose.focus();
  } else if (menu.contains(document.activeElement)) {
    menuButton.focus();
  }
}
menuButton.addEventListener('click', () => setMenuOpen(true));
menuClose.addEventListener('click', () => setMenuOpen(false));
menuBackdrop.addEventListener('click', () => setMenuOpen(false));
menu.addEventListener('click', (event) => {
  if (event.target.closest('a')) setMenuOpen(false);
});
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && document.body.classList.contains('menu-open')) setMenuOpen(false);
});
// The button sits at the left edge of the steps, not of the window (on narrow screens, 12px from the edge).
function placeMenuButton() {
  const shown = document.querySelector('main > section:not([hidden])');
  menuButton.style.left = Math.max(12, Math.round(shown.getBoundingClientRect().left)) + 'px';
}
window.addEventListener('resize', placeMenuButton);
window.addEventListener('load', placeMenuButton);
// the column settles after the fonts, the language bar and the texts arrive: follow it as it changes size
new ResizeObserver(placeMenuButton).observe(document.querySelector('main'));
placeMenuButton();

// The menu's pages (help, about, Discord, credits: each a section with class "page") take the place of the three
// steps; a step's address brings them back. Other addresses (the legal notice in the footer) leave the page as it is.
const steps = [...document.querySelectorAll('main > section[id^="step"]')];
const pages = [...document.querySelectorAll('main > section.page')];
function showPage() {
  const hash = location.hash;
  const page = pages.find((p) => hash === `#${p.id}`);
  if (!page && hash !== '' && !hash.startsWith('#step')) return;
  for (const p of pages) p.hidden = p !== page;
  for (const step of steps) step.hidden = Boolean(page);
  // the menu marks where the player is (in green): the page shown, or the installer
  const current = page ? `#${page.id}` : '#step1';
  for (const a of menu.querySelectorAll('a[href^="#"]')) {
    if (a.getAttribute('href') === current) a.setAttribute('aria-current', 'page');
    else a.removeAttribute('aria-current');
  }
  placeMenuButton();
  if (hash.startsWith('#step')) document.querySelector(hash)?.scrollIntoView();
}
window.addEventListener('hashchange', showPage);
for (const button of document.querySelectorAll('.back-to-steps')) {
  button.addEventListener('click', () => {
    location.hash = '#step1';
  });
}
showPage();

// The page fits in the window when it can: when it is taller than the screen, it is scaled down evenly, but never
// below 80 % so the text stays large (a 1080p screen fits); past that it scrolls. Recomputed when the window or the content changes size.
// Phones just scroll.
const page = document.querySelector('main');
let zoom = 1;
function setZoom(value) {
  zoom = value;
  page.style.zoom = zoom === 1 ? '' : String(zoom);
}
function fitToWindow() {
  if (window.innerWidth < 700) {
    if (zoom !== 1) setZoom(1);
    return;
  }
  // measured again after scaling, up to three times: borders stay 1px wide, so the page does not shrink exactly
  for (let pass = 0; pass < 3; pass++) {
    // the height the blocks need, without the free space that is spread between them
    page.style.minHeight = '0';
    const height = page.getBoundingClientRect().height;
    page.style.minHeight = '';
    const room = window.innerHeight - 2;
    const wanted = Math.max(0.8, Math.min(1, (zoom * room) / height));
    // shrink as soon as it does not fit; grow back to full size, or by a visible step, so it does not flicker
    const grow = wanted === 1 ? zoom < 1 : wanted - zoom > 0.005;
    if (height > room ? wanted >= zoom : !grow) return;
    setZoom(wanted);
  }
}
const resized = new ResizeObserver(fitToWindow);
resized.observe(page);
// the page itself keeps the height of the window while the blocks fit: watch them to see a message or the log grow
for (const block of page.children) resized.observe(block);
window.addEventListener('resize', fitToWindow);
document.fonts.ready.then(fitToWindow);

// Step 3 names the folder and the program as they are in the zip; they come from the release.
const names = { folder: 'LM-NX', nro: 'LM-NX.nro' };

const manifest = fetch('./release/manifest.json', { cache: 'no-cache' }).then((r) => {
  if (!r.ok) throw new Error(`release/manifest.json: HTTP ${r.status}`);
  return r.json();
});
// The folder and the program name come from the release, so step 3 names them as they are in the zip.
manifest.then((m) => {
  names.folder = m.folder;
  names.nro = m.nro;
  applyLanguage(getLanguage());
}, () => {});

// The chosen discs: { file, kind } for each slot.
const discs = { usa: null, pal: null };
let busy = false;

function log(text, error = false) {
  const line = document.createElement('div');
  line.textContent = text;
  if (error) line.className = 'error';
  logBox.appendChild(line);
  logBox.scrollTop = logBox.scrollHeight;
}

// The bar: what is being done above it, the percentage under it.
function show(text, done = 0, total = 0) {
  work.hidden = false;
  what.textContent = text;
  const fraction = total ? Math.min(1, done / total) : 0;
  fill.style.width = `${fraction * 100}%`;
  percent.textContent = total ? `${Math.floor(fraction * 100)} %` : '';
}

// What each disc row shows, kept as a text key so it can be drawn again in another language.
const discViews = { usa: { state: '', key: 'usaNotChosen' }, pal: { state: '', key: 'palNotChosen' } };

function showDisc(slot, state, key, params = {}) {
  discViews[slot] = { state, key, params };
  renderDisc(slot);
}

function renderDisc(slot) {
  const { state, key, params } = discViews[slot];
  const row = document.getElementById(slot);
  row.className = `disc ${state}`;
  row.querySelector('.mark').textContent = state === 'good' ? '✓' : state === 'bad' ? '!' : slot === 'usa' ? '1' : '2';
  // only the texts before a disc is chosen have markup (ours); with a disc, its file name goes in as plain text
  if (state) row.querySelector('.state').textContent = t(key, { ...params, problem: params.problem && t(params.problem) });
  else row.querySelector('.state').innerHTML = t(key);
}

function setButtons() {
  create.disabled = busy || !discs.usa;
  update.disabled = busy;
}

const PROBLEMS = { palRev1: 'palRev1', other: 'otherDisc' };

async function chooseDiscs(files) {
  for (const file of files) {
    let found;
    let image;
    try {
      // an .rvz, .wia, .gcz or .ciso is read as the full .iso it holds, and goes in the zip as that .iso
      image = await openDiscImage(file);
      found = await identifyDisc(image);
    } catch (error) {
      log(error.code ? t(error.code, { name: file.name }) : `${file.name}: ${error.message}.`, true);
      continue;
    }
    if (found.kind === 'usa' || found.kind === 'pal') {
      discs[found.kind] = { file: image };
      showDisc(found.kind, 'good', 'discReady', { name: file.name, size: formatSize(file.size) });
    } else {
      // a disc of the right game that cannot be used is shown in its row, anything else in the log
      const slot = found.id === 'GLME01' ? 'usa' : found.id === 'GLMP01' ? 'pal' : null;
      if (slot) {
        discs[slot] = null;
        showDisc(slot, 'bad', 'discProblem', { name: file.name, problem: PROBLEMS[found.kind] });
      } else {
        log(t('discId', { text: `${file.name}: ${t(PROBLEMS[found.kind])}`, id: found.id }), true);
      }
    }
  }
  setButtons();
}

document.getElementById('discs').addEventListener('change', async (event) => {
  const files = [...event.target.files];
  event.target.value = '';
  if (files.length && !busy) await chooseDiscs(files);
});

// The zip is served by sw.js as a normal browser download, streamed while it is being written.
let reloading = false;
async function downloadHelper() {
  if (!('serviceWorker' in navigator)) throw new Error('noStreaming');
  await navigator.serviceWorker.register('./sw.js', { updateViaCache: 'none' });
  const registration = await navigator.serviceWorker.ready;
  if (navigator.serviceWorker.controller) {
    // a new version of the page takes over while it is open: load it again once, unless the player is busy
    navigator.serviceWorker.addEventListener('controllerchange', () => {
      if (!busy && !discs.usa && !discs.pal && !reloading) {
        reloading = true;
        location.reload();
      }
    });
  }
  if (!navigator.serviceWorker.controller) {
    const claimed = new Promise((resolve) => navigator.serviceWorker.addEventListener('controllerchange', resolve, { once: true }));
    // a forced reload (Ctrl+F5) opens the page without its service worker, which then has to take it back
    registration.active.postMessage({ type: 'claim' });
    await claimed;
  }
  navigator.serviceWorker.controller.postMessage({ type: 'keep-page' });
  return navigator.serviceWorker.controller;
}
if ('serviceWorker' in navigator) {
  // sw.js asks the open pages which version they are when a new version takes over: this one answers
  navigator.serviceWorker.addEventListener('message', (event) => {
    if (event.data?.type === 'version' && event.ports[0]) event.ports[0].postMessage('current');
  });
  navigator.serviceWorker.startMessages();
}
const helper = downloadHelper();
helper.catch(() => {});

// Sends every write as one chunk, and only when the download has asked for more ("pull").
function portSink(port) {
  let credits = 0;
  let cancelled = false;
  let waiting = null;
  port.onmessage = ({ data }) => {
    if (data.type === 'pull') credits++;
    else if (data.type === 'cancel') cancelled = true;
    if (waiting) {
      const wake = waiting;
      waiting = null;
      wake();
    }
  };
  const stop = () => new Error(t('cancelled'));
  return {
    async write(bytes) {
      while (!credits) {
        if (cancelled) throw stop();
        await new Promise((resolve) => (waiting = resolve));
      }
      if (cancelled) throw stop();
      credits--;
      const copy = bytes.slice();
      port.postMessage({ type: 'chunk', buffer: copy.buffer }, [copy.buffer]);
    },
    async close() {
      port.postMessage({ type: 'end' });
    },
    fail(message) {
      port.postMessage({ type: 'error', message });
    },
  };
}

async function sha256(bytes) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))].map((b) => b.toString(16).padStart(2, '0')).join('');
}

// A file of the release, checked against the SHA-256 the manifest gives for it. Its address names the hash, so
// sw.js keeps a copy and the next package does not download it again.
async function fetchRelease(name, hash, label) {
  const response = await fetch(`./release/${name}?sha256=${hash}`);
  if (!response.ok) throw new Error(t('downloadFailed', { name, status: response.status }));
  const total = Number(response.headers.get('content-length')) || 0;
  const reader = response.body.getReader();
  const parts = [];
  let received = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    parts.push(value);
    received += value.length;
    show(label, received, total);
  }
  const bytes = new Uint8Array(received);
  let at = 0;
  for (const p of parts) {
    bytes.set(p, at);
    at += p.length;
  }
  if ((await sha256(bytes)) !== hash) throw new Error(t('notExpected', { name }));
  return bytes;
}

let downloadFrame = null;
async function startDownload(controller, filename, size) {
  const id = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
  const channel = new MessageChannel();
  const ack = new MessageChannel();
  const registered = new Promise((resolve) => (ack.port1.onmessage = resolve));
  controller.postMessage({ type: 'download', id, filename, size }, [channel.port2, ack.port2]);
  await registered;
  // a hidden frame opens the download: its request is a navigation, which always reaches the service worker
  downloadFrame?.remove();
  downloadFrame = document.createElement('iframe');
  downloadFrame.hidden = true;
  downloadFrame.src = `./download/${id}/${filename}`;
  document.body.appendChild(downloadFrame);
  return portSink(channel.port1);
}

async function makePackage(full) {
  if (busy || (full && !discs.usa)) return;
  busy = true;
  setButtons();
  logBox.textContent = '';
  const started = Date.now();
  let sink = null;
  let keepAlive = 0;
  try {
    let controller;
    try {
      controller = await helper;
    } catch (error) {
      throw new Error(error.message === 'noStreaming' ? t('noStreaming') : error.message);
    }
    const m = await manifest;
    const nro = await fetchRelease(m.nro, m.nro_sha256, t('downloading', { name: m.nro }));
    const pipelines = await fetchRelease('pipelines.bin', m.pipelines_sha256, t('downloadingPipelines'));
    log(t('downloaded', { name: m.nro, version: m.version }));

    const shaderStart = Date.now();
    const caches = await buildShaderCaches(
      pipelines.buffer,
      {
        worker: new URL('./lib/shader_worker.js', import.meta.url).href,
        module: new URL('./forge/shader_forge.mjs', import.meta.url).href,
        wasm: new URL('./forge/shader_forge.wasm', import.meta.url).href,
      },
      (done, total) => show(t('compiling', { done, total }), done, total),
    );
    if (caches.buildId !== m.shader_build) {
      throw new Error(t('compilerMismatch', { forge: caches.buildId, game: m.shader_build }));
    }
    log(t('compiled', { count: caches.stages, seconds: Math.round((Date.now() - shaderStart) / 1000) }));

    let hiddenMansion = [];
    if (full && discs.pal) {
      show(t('buildingHM'));
      hiddenMansion = await buildHiddenMansion(await openDisc(discs.usa.file), await openDisc(discs.pal.file));
      log(t('builtHM'));
    }

    const entries = packageEntries(m, {
      nro,
      pipelines,
      shaders: caches.files,
      discs: full ? { usa: discs.usa.file, pal: discs.pal?.file ?? null } : null,
      hiddenMansion,
    });
    const filename = full ? 'lm-nx.zip' : 'lm-nx-update.zip';
    const size = packageSize(entries);
    keepAlive = setInterval(() => controller.postMessage({ type: 'ping' }), 10000);
    sink = await startDownload(controller, filename, size);
    log(t('saving', { name: filename, size: formatSize(size) }));
    await writePackage(entries, sink, (written, total) => show(t('writing', { name: filename }), written, total));
    show(t('ready', { name: filename }), 1, 1);
    log(t('done', { seconds: Math.round((Date.now() - started) / 1000) }));
  } catch (error) {
    // a compressed disc found damaged while the zip was written says which one, in the player's language
    const message = error.code && error.disc ? t(error.code, { name: error.disc }) : error.message || String(error);
    sink?.fail(message);
    what.textContent = t('failed');
    log(t('error', { message }), true);
  } finally {
    clearInterval(keepAlive);
    busy = false;
    setButtons();
  }
}

function applyLanguage(code) {
  setLanguage(code);
  document.documentElement.lang = getLanguage();
  for (const el of document.querySelectorAll('[data-i18n]')) el.textContent = t(el.dataset.i18n);
  // our own texts, with <strong>, <code> and the Sphaira link
  for (const el of document.querySelectorAll('[data-i18n-html]')) el.innerHTML = t(el.dataset.i18nHtml, names);
  languages.setAttribute('aria-label', t('languageBar'));
  menuButton.setAttribute('aria-label', t('menuOpen'));
  menuClose.setAttribute('aria-label', t('menuClose'));
  menu.setAttribute('aria-label', t('menuTitle'));
  for (const button of languages.querySelectorAll('button')) {
    button.setAttribute('aria-pressed', String(button.dataset.lang === getLanguage()));
  }
  renderDisc('usa');
  renderDisc('pal');
}

for (const { code, name, flag } of LANGUAGES) {
  const button = document.createElement('button');
  button.type = 'button';
  button.dataset.lang = code;
  button.title = name;
  button.setAttribute('aria-label', name);
  button.setAttribute('lang', code);
  button.innerHTML = FLAGS[flag];
  button.addEventListener('click', () => {
    applyLanguage(code);
    try {
      localStorage.setItem(LANGUAGE_KEY, code);
    } catch {
      // private windows and blocked storage: the choice just lasts for this visit
    }
  });
  languages.appendChild(button);
}
let saved = null;
try {
  saved = localStorage.getItem(LANGUAGE_KEY);
} catch {
  saved = null;
}
applyLanguage(preferredLanguage(saved));

create.addEventListener('click', () => makePackage(true));
update.addEventListener('click', () => makePackage(false));
setButtons();
