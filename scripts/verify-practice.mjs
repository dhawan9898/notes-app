// End-to-end check of the practice problems with the same WebAssembly clang the app uses and the same judge code the page uses:
// the starter as shipped must NOT be Accepted, and the reference solution spliced into the editable region must be Accepted.
// Usage: node scripts/verify-practice.mjs [substring…]      (run `node scripts/build-practice.mjs` first)
import fs from 'node:fs'; import path from 'node:path'; import os from 'node:os';
import { Worker, isMainThread, parentPort } from 'node:worker_threads';
import { build } from 'esbuild';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
if (!isMainThread) {
  await import(path.join(ROOT, 'public/runner/cc.js'));
  const dir = path.join(ROOT, 'public/runner/');
  const runner = new globalThis.CRunner({
    loadBytes: async (n) => { const b = fs.readFileSync(dir + n); return b.buffer.slice(b.byteOffset, b.byteOffset + b.length); },
    compileModule: (b) => WebAssembly.compile(b), now: () => performance.now(),
  });
  parentPort.on('message', async ({ code }) => {
    try { parentPort.postMessage({ r: await runner.runC(code, { stdin: '', files: {} }) }); } catch (e) { parentPort.postMessage({ error: String(e.message || e) }); }
  });
} else {
  const tmp = fs.mkdtempSync(path.join(process.env.TMPDIR || os.tmpdir(), 'dn-verify-'));
  await build({ entryPoints: [path.join(ROOT, 'src/scripts/practice-judge.ts')], bundle: true, format: 'esm', outfile: path.join(tmp, 'judge.mjs'), logLevel: 'error' });
  const J = await import(path.join(tmp, 'judge.mjs'));
  const only = process.argv.slice(2);
  const problems = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/practice.json'), 'utf8')).filter((p) => !only.length || only.some((o) => p.id.includes(o)));
  let worker = null;
  const run = (code) => new Promise((resolve) => {
    worker ||= new Worker(new URL(import.meta.url), { resourceLimits: { maxOldGenerationSizeMb: 2048 } });
    const w = worker, timer = setTimeout(() => { w.terminate(); worker = null; resolve({ error: 'Stopped: the program ran longer than 45 seconds' }); }, 45000);
    w.once('message', (m) => { clearTimeout(timer); resolve(m); });
    w.once('error', (e) => { clearTimeout(timer); worker = null; resolve({ error: String(e) }); });
    w.postMessage({ code: code.replace(/^(\s*#\s*include\s+")(?:[^"]*\/)?([^"\/]+)(")/gm, '$1$2$3') });
  });
  let bad = 0;
  for (const p of problems) {
    const problem = [];
    const s = J.judge(...((m) => (m.error ? [null, m.error] : [m.r]))(await run(J.assemble(p.prefix, p.starter, p.suffix))));
    if (s.verdict === 'Accepted') problem.push('the starter as shipped is Accepted: the tests do not detect the missing solution');
    if (s.verdict === 'Compile Error') problem.push(`the starter does not compile:\n${s.message}`);
    const a = J.judge(...((m) => (m.error ? [null, m.error] : [m.r]))(await run(J.assemble(p.prefix, p.solution, p.suffix))));
    if (a.verdict !== 'Accepted') problem.push(`the solution is ${a.verdict} (${a.passed}/${a.total}):\n${a.cases.filter((c) => !c.ok).map((c) => c.line).join('\n') || a.message}`);
    if (problem.length) { bad++; console.log(`FAIL ${p.id}\n  ${problem.join('\n  ').replace(/\n/g, '\n    ')}`); }
    else console.log(`ok   ${p.id}  (starter: ${s.verdict} ${s.passed}/${s.total}; solution: ${a.verdict} ${a.passed}/${a.total})`);
  }
  worker?.terminate(); fs.rmSync(tmp, { recursive: true, force: true });
  console.log(`${problems.length - bad} of ${problems.length} problems OK`);
  process.exit(bad ? 1 : 0);
}
