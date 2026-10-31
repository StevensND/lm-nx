// One shader forge instance (shader_forge.wasm, built from the port's sources by
// tools/build_shader_forge.py --web). It generates the full job list from the pipeline list, then
// compiles the jobs the page hands it and sends back the cache records.

let forge = null;

function bytesOut(dataFn, sizeFn) {
  const size = forge[sizeFn]();
  return forge.HEAPU8.slice(forge[dataFn](), forge[dataFn]() + size);
}

self.onmessage = async ({ data }) => {
  try {
    if (data.type === 'init') {
      const { default: createForge } = await import(data.module);
      // Aurora's warnings about the game's own states are not errors of the page: they stay out of the console
      forge = await createForge({ locateFile: () => data.wasm, print: () => {}, printErr: () => {} });
      const ptr = forge._malloc(data.list.byteLength);
      forge.HEAPU8.set(new Uint8Array(data.list), ptr);
      const jobs = forge._forge_generate(ptr, data.list.byteLength, 0);
      forge._free(ptr);
      if (jobs < 0) {
        throw new Error(forge.UTF8ToString(forge._forge_error()));
      }
      self.postMessage({ type: 'ready', jobs, buildId: forge.UTF8ToString(forge._forge_build_id()) });
    } else if (data.type === 'compile') {
      const results = [];
      const transfer = [];
      for (const job of data.jobs) {
        if (!forge._forge_compile(job)) {
          throw new Error(`shader ${job}: ${forge.UTF8ToString(forge._forge_error())}`);
        }
        const tint = bytesOut('_forge_tint_data', '_forge_tint_size');
        const dksh = bytesOut('_forge_dksh_data', '_forge_dksh_size');
        results.push({ job, tint, dksh });
        transfer.push(tint.buffer, dksh.buffer);
      }
      self.postMessage({ type: 'done', results }, transfer);
    }
  } catch (error) {
    self.postMessage({ type: 'error', message: String(error && error.message ? error.message : error) });
  }
};
