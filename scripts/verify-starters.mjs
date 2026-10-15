// Checks the exercise starter programs under authoring/examples/<track>/starters/.
// A starter is a complete program: the function(s) the reader writes, between the marker comments
//   /* ===== Your code: ... */  and  /* ===== End of your code ===== */
// followed by a main() that runs test cases and prints PASS/FAIL lines and a summary.
// Next to it, NAME.solution.c holds a reference solution for the marked region. For every starter this script checks
//   1. the starter as shipped compiles without warnings (unless it says "// starter-warnings: expected", when the warning is the lesson), runs, and reports at least one FAIL (so the tests test something);
//   2. with the solution spliced in it compiles without warnings and, under AddressSanitizer and UBSan, passes every check;
//   3. every NAME.wrong-*.c (a plausible wrong answer, such as a comparator that subtracts) spliced in reports a FAIL.
// Usage: node scripts/verify-starters.mjs [substring…]      (needs a C compiler as `cc`)
import fs from 'node:fs'; import path from 'node:path'; import os from 'node:os'; import { spawnSync } from 'node:child_process';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const EX = path.join(ROOT, 'authoring/examples');
const BEGIN = /^.*\/\* ===== Your code.*$/m, END = /^.*\/\* ===== End of your code ===== \*\/.*$/m;
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
const only = process.argv.slice(2);
const starters = walk(EX).filter((f) => f.includes('/starters/') && f.endsWith('.c') && !/\.(solution|wrong-[\w-]+)\.c$/.test(f))
  .filter((f) => !only.length || only.some((o) => f.includes(o))).sort();
const templates = walk(path.join(ROOT, 'authoring/templates')).map((f) => fs.readFileSync(f, 'utf8')).join('\n');
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'starters-'));

function build(src, flags) {
  const c = path.join(tmp, 'prog.c'), exe = path.join(tmp, 'prog');
  fs.writeFileSync(c, src);
  const std = (src.match(/^\/\/ std: (\S+)/m) || [, 'gnu17'])[1];
  const r = spawnSync('cc', [`-std=${std}`, '-Wall', '-Wextra', '-g', ...flags, '-o', exe, c, '-lm'], { encoding: 'utf8' });
  if (r.status !== 0) return { error: r.stderr };
  const run = spawnSync(exe, [], { encoding: 'utf8', timeout: 20000 });
  return { warnings: r.stderr.trim(), out: (run.stdout || '') + (run.stderr || ''), code: run.status, timedOut: run.error?.code === 'ETIMEDOUT' };
}

let bad = 0;
for (const f of starters) {
  const rel = path.relative(EX, f), src = fs.readFileSync(f, 'utf8'), problems = [];
  const sol = f.replace(/\.c$/, '.solution.c');
  if (!BEGIN.test(src) || !END.test(src)) problems.push('missing the "Your code" markers');
  if (!fs.existsSync(sol)) problems.push(`missing ${path.basename(sol)}`);
  const ready = !problems.length;
  if (!templates.includes(`{{include ${rel}}}`)) problems.push('not included by any template');
  if (ready) {
    const s = build(src, []);
    if (s.error) problems.push(`starter does not compile:\n${s.error}`);
    else {
      if (s.warnings && !/^\/\/ starter-warnings: expected/m.test(src)) problems.push(`starter compiles with warnings:\n${s.warnings}`);
      if (s.timedOut) problems.push('starter timed out');
      if (!/^FAIL /m.test(s.out)) problems.push('starter reports no FAIL: the tests do not detect the missing solution');
      if (!/checks passed/.test(s.out)) problems.push('starter prints no summary');
    }
    const a = src.search(BEGIN), b = src.search(END);
    const begin = src.slice(a, src.indexOf('\n', a) + 1);
    const splice = (file) => src.slice(0, a) + begin + fs.readFileSync(file, 'utf8').replace(/\n*$/, '\n') + src.slice(b);
    const wrongs = fs.readdirSync(path.dirname(f)).filter((w) => w.startsWith(path.basename(f, '.c') + '.wrong-') && w.endsWith('.c'));
    for (const w of wrongs) {
      const x = build(splice(path.join(path.dirname(f), w)), []);
      if (x.error) problems.push(`${w} does not compile:\n${x.error}`);
      else if (!/^FAIL /m.test(x.out)) problems.push(`the tests accept the wrong answer in ${w}:\n${x.out}`);
    }
    const spliced = splice(sol);
    const r = build(spliced, ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']);
    if (r.error) problems.push(`solution does not compile:\n${r.error}`);
    else {
      if (r.warnings) problems.push(`solution compiles with warnings:\n${r.warnings}`);
      if (r.code !== 0 || /^FAIL /m.test(r.out) || r.timedOut) problems.push(`solution does not pass (exit ${r.code}):\n${r.out}`);
    }
    if (!problems.length) {
      const sum = (r.out.match(/^.*checks passed.*$/m) || [''])[0];
      console.log(`ok   ${rel}  (starter: ${(s.out.match(/^FAIL /gm) || []).length} failing; solution: ${sum}${wrongs.length ? `; ${wrongs.length} wrong answer(s) rejected` : ''})`);
    }
  }
  if (problems.length) { bad++; console.log(`FAIL ${rel}\n  ${problems.join('\n  ').replace(/\n/g, '\n    ')}`); }
}
fs.rmSync(tmp, { recursive: true, force: true });
console.log(`${starters.length - bad} of ${starters.length} starters OK`);
process.exit(bad ? 1 : 0);
