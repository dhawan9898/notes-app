// Downloads the prebuilt WebAssembly C toolchain used by the in-app runner into public/runner/ (not stored in git: ~60 MB).
// Source: https://github.com/binji/wasm-clang (clang/LLVM 8.0.1, wasm32-wasi, Apache-2.0 with LLVM exception). Files are verified by SHA-256.
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto';
const DIR = path.resolve(new URL('../public/runner/', import.meta.url).pathname);
const BASE = 'https://binji.github.io/wasm-clang/';
const FILES = {
  'clang.wasm': ['clang', 'CLANG_SHA'],
  'lld.wasm': ['lld', 'LLD_SHA'],
  'memfs.wasm': ['memfs', 'MEMFS_SHA'],
  'sysroot.tar': ['sysroot.tar', 'SYSROOT_SHA'],
};
const SHA = { CLANG_SHA: '2a466f0e990329d3230b869d04fc20803eae96a7feb3a3f6c93e25a77b8aed1d', LLD_SHA: '36419ed202011765222098d7701218378b67f634d50f0a4625059ae2c9860f48', MEMFS_SHA: '2c72ee42bd9430029dda8c6bafc9f37143f6fe88d5f1ea950a70259ab748bcfe', SYSROOT_SHA: '2435a7b549af30c2be7ec249c405bc2e911ab0c6003012f0909ec3c131bff867' };
const sha = (f) => crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');
fs.mkdirSync(DIR, { recursive: true });
let failed = false;
for (const [local, [remote, key]] of Object.entries(FILES)) {
  const dest = path.join(DIR, local);
  if (fs.existsSync(dest) && sha(dest) === SHA[key]) { console.log(`ok       ${local}`); continue; }
  process.stdout.write(`download ${local} … `);
  const res = await fetch(BASE + remote);
  if (!res.ok) { console.log(`HTTP ${res.status}`); failed = true; continue; }
  fs.writeFileSync(dest, Buffer.from(await res.arrayBuffer()));
  if (sha(dest) !== SHA[key]) { console.log('CHECKSUM MISMATCH (the upstream file changed); refusing to use it'); fs.unlinkSync(dest); failed = true; } else console.log('done');
}
if (failed) process.exit(1);
