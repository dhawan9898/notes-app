// Finds every complete C program (a ```c block with main) in the notes, compiles and runs it with the same WebAssembly clang
// that ships in the app, and records which ones work in src/data/runnable.json. Only those get a "Run" button.
// Usage: node scripts/validate-snippets.mjs [--force]      (results are cached by program hash)
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const OUT = path.join(ROOT, 'src/data/runnable.json');
// ---- local headers: programs in the notes come from authoring/examples; find the quoted includes they need ----
const EX = path.join(ROOT, 'authoring/examples');
const exampleFiles = [];
(function scan(d) { for (const e of fs.readdirSync(d, { withFileTypes: true })) { const p = path.join(d, e.name); if (e.isDirectory()) scan(p); else if (/\.(c|h)$/.test(e.name)) exampleFiles.push(p); } })(EX);
const byBase = new Map(); for (const f of exampleFiles) { const b = path.basename(f); byBase.set(b, [...(byBase.get(b) || []), f]); }
const norm = (t) => t.trim();
const byContent = new Map(exampleFiles.map((f) => [norm(fs.readFileSync(f, 'utf8')), f]));
function collectFiles(code, startDir) {
  const files = {}; const queue = [[code, startDir]];
  while (queue.length) {
    const [text, dir] = queue.pop();
    for (const m of text.matchAll(/^\s*#\s*include\s+"([^"]+)"/gm)) {
      const base = path.basename(m[1]); if (base in files) continue;
      let f = dir && fs.existsSync(path.resolve(dir, m[1])) ? path.resolve(dir, m[1]) : null;
      if (!f) { const c = byBase.get(base) || []; if (c.length >= 1 && new Set(c.map((x) => fs.readFileSync(x, 'utf8'))).size === 1) f = c[0]; }
      if (!f) return null;                                              // cannot resolve: leave the program non-runnable
      const content = fs.readFileSync(f, 'utf8'); files[base] = content; queue.push([content, path.dirname(f)]);
    }
  }
  return files;
}
const flatten = (code) => code.replace(/^(\s*#\s*include\s+")(?:[^"]*\/)?([^"\/]+)(")/gm, '$1$2$3');
export const hashOf = (code) => crypto.createHash('sha1').update(code.trim()).digest('hex').slice(0, 16);

if (!isMainThread) {
  await import(path.join(ROOT, 'public/runner/cc.js'));
  const dir = path.join(ROOT, 'public/runner/');
  const runner = new globalThis.CRunner({
    loadBytes: async (n) => { const b = fs.readFileSync(dir + n); return b.buffer.slice(b.byteOffset, b.byteOffset + b.length); },
    compileModule: (b) => WebAssembly.compile(b), now: () => performance.now(),
  });
  parentPort.on('message', async ({ i, code, files }) => {
    try { const r = await runner.runC(code, { stdin: '', files }); parentPort.postMessage({ i, r }); }
    catch (e) { parentPort.postMessage({ i, error: String(e.message || e) }); }
  });
} else {
  const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
  const programs = new Map();                                          // hash -> { code, where }
  for (const f of walk(path.join(ROOT, 'src/content/notes')).filter((f) => f.endsWith('.md'))) {
    const text = fs.readFileSync(f, 'utf8');
    for (const m of text.matchAll(/^```c\n([\s\S]*?)^```/gm)) {
      if (!/\bmain\s*\(/.test(m[1])) continue;
      const src = byContent.get(norm(m[1]));
      const files = /^\s*#\s*include\s+"/m.test(m[1]) ? collectFiles(m[1], src ? path.dirname(src) : null) : {};
      programs.set(hashOf(m[1]), { code: m[1], files, where: path.relative(path.join(ROOT, 'src/content/notes'), f) });
    }
  }
  const prev = fs.existsSync(OUT) && !process.argv.includes('--force') ? JSON.parse(fs.readFileSync(OUT, 'utf8')) : {};
  const todo = [...programs].filter(([h]) => !(h in prev));
  console.log(`${programs.size} complete C programs, ${todo.length} to check`);
  const result = { ...prev };
  for (const h of Object.keys(result)) if (!programs.has(h)) delete result[h];            // drop entries for removed programs

  let worker = null, i = 0;
  const start = () => { worker = new Worker(new URL(import.meta.url), { resourceLimits: { maxOldGenerationSizeMb: 2048 } }); };
  const run = (code, files) => new Promise((resolve) => {
    if (!worker) start();
    const timer = setTimeout(() => { worker.terminate(); worker = null; resolve({ timeout: true }); }, 45000);
    worker.once('message', (m) => { clearTimeout(timer); resolve(m); });
    worker.once('error', (e) => { clearTimeout(timer); worker = null; resolve({ error: String(e) }); });
    worker.postMessage({ i, code: flatten(code), files });
  });
  const bundles = {};
  for (const [h, { code, files, where }] of todo) {
    i++;
    if (files === null) { result[h] = { ok: false, where, note: 'unresolved local header' }; console.log(`${String(i).padStart(3)}/${todo.length} FAIL ${where} unresolved local header`); continue; }
    const m = await run(code, files);
    let status, note = '';
    if (m.timeout) { status = false; note = 'timeout'; }
    else if (m.error) { status = false; note = m.error.slice(0, 120); }
    else {
      const r = m.r;
      status = r.phase === 'run' && !/\[runtime error\]/.test(r.stderr);
      if (!status) note = (r.diagnostics || r.stderr).replace(/\x1b\[[0-9;]*m/g, '').split('\n').find((l) => /error|undefined symbol|runtime/.test(l))?.slice(0, 140) || r.phase;
    }
    result[h] = { ok: status, where, ...(note ? { note } : {}), ...(status && Object.keys(files).length ? { files: Object.keys(files) } : {}) };
    if (status && Object.keys(files).length) bundles[h] = files;
    console.log(`${String(i).padStart(3)}/${todo.length} ${status ? 'ok  ' : 'FAIL'} ${where} ${note}`);
  }
  if (worker) worker.terminate();
  const BUNDLES = path.join(ROOT, 'public/runner/bundles.json');
  const oldB = fs.existsSync(BUNDLES) ? JSON.parse(fs.readFileSync(BUNDLES, 'utf8')) : {};
  const allB = { ...oldB, ...bundles }; for (const h of Object.keys(allB)) if (!(h in result) || !result[h].ok) delete allB[h];
  fs.writeFileSync(BUNDLES, JSON.stringify(allB));
  fs.mkdirSync(path.dirname(OUT), { recursive: true });
  fs.writeFileSync(OUT, JSON.stringify(result, null, 1) + '\n');
  const n = Object.values(result).filter((x) => x.ok).length;
  console.log(`runnable: ${n} of ${programs.size}`);
}
