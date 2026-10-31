// Builds the console's shader caches in the browser, so the game starts without compiling shaders.
// The shaders are made here, on the player's computer, from the port's public pipeline list; none are
// downloaded. Several Web Workers share the work (one shader forge each).
//
// Result: the two cache files the game reads, under cache/ in its SD folder:
//   lmnx-tint-<build>/paquete.lmp         translations (WGSL to GLSL)
//   lmnx-f447-gxabi2-<build>/paquete.lmp  compiled shaders (DKSH)

const BATCH = 8;

function recordKey(bytes) {
  // Record: "LMPK", 64-bit key, 32-bit size, data.
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return `${view.getUint32(4, true)}:${view.getUint32(8, true)}`;
}

function concat(parts, total) {
  const out = new Uint8Array(total);
  let at = 0;
  for (const p of parts) {
    out.set(p, at);
    at += p.length;
  }
  return out;
}

// list: the pipeline list (ArrayBuffer of pipelines.bin). urls: { worker, module, wasm }.
// progress(done, total) is called as stages finish; total is known before the first one.
export async function buildShaderCaches(list, urls, progress = () => {}, threads = 0) {
  const count = Math.max(1, Math.min(threads || (navigator.hardwareConcurrency || 4) - 1, 12));
  const workers = Array.from({ length: count }, () => new Worker(urls.worker, { type: 'module' }));
  try {
    const ready = await Promise.all(
      workers.map(
        (w) =>
          new Promise((resolve, reject) => {
            w.onmessage = ({ data }) => (data.type === 'ready' ? resolve(data) : reject(new Error(data.message)));
            w.onerror = (e) => reject(new Error(e.message || 'the shader worker could not start'));
            w.postMessage({ type: 'init', module: urls.module, wasm: urls.wasm, list: list.slice(0) });
          }),
      ),
    );
    const total = ready[0].jobs;
    const buildId = ready[0].buildId;
    if (ready.some((r) => r.jobs !== total || r.buildId !== buildId)) {
      throw new Error('the shader workers disagree on the shader list');
    }
    progress(0, total);

    const tint = [];
    const dksh = [];
    const seen = new Set();
    let tintBytes = 0;
    let dkshBytes = 0;
    let next = 0;
    let done = 0;
    await Promise.all(
      workers.map(
        (w) =>
          new Promise((resolve, reject) => {
            const feed = () => {
              if (next >= total) {
                resolve();
                return;
              }
              const jobs = [];
              while (jobs.length < BATCH && next < total) jobs.push(next++);
              w.postMessage({ type: 'compile', jobs });
            };
            w.onmessage = ({ data }) => {
              if (data.type !== 'done') {
                reject(new Error(data.message));
                return;
              }
              for (const r of data.results) {
                tint.push(r.tint);
                tintBytes += r.tint.length;
                const key = recordKey(r.dksh);
                if (!seen.has(key)) {
                  seen.add(key);
                  dksh.push(r.dksh);
                  dkshBytes += r.dksh.length;
                }
              }
              done += data.results.length;
              progress(done, total);
              feed();
            };
            feed();
          }),
      ),
    );
    return {
      buildId,
      stages: total,
      files: [
        { path: `cache/lmnx-tint-${buildId}/paquete.lmp`, data: concat(tint, tintBytes) },
        { path: `cache/lmnx-f447-gxabi2-${buildId}/paquete.lmp`, data: concat(dksh, dkshBytes) },
      ],
    };
  } finally {
    for (const w of workers) w.terminate();
  }
}
