// Checks that the NRO and the pipeline list named in release/manifest.json are in release/ with the SHA-256 the
// manifest gives for them.
// Usage: node ../.github/verify_release.mjs (from installer/)
import fs from 'node:fs';
import { createHash } from 'node:crypto';

const manifest = JSON.parse(fs.readFileSync('release/manifest.json', 'utf8'));
let bad = 0;
for (const [name, expected] of [[manifest.nro, manifest.nro_sha256], ['pipelines.bin', manifest.pipelines_sha256]]) {
  const path = `release/${name}`;
  if (!fs.existsSync(path)) {
    console.error(`missing: ${name}`);
    bad++;
    continue;
  }
  const hash = createHash('sha256').update(fs.readFileSync(path)).digest('hex');
  if (hash !== expected) {
    console.error(`wrong SHA-256: ${name} is ${hash}, the manifest expects ${expected}`);
    bad++;
  } else {
    console.log(`ok: ${name}`);
  }
}
if (bad) {
  process.exit(1);
}
