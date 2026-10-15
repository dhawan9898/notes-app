// Unit tests for the practice store and judge (src/scripts/practice-store.ts, practice-judge.ts).   Usage: node scripts/test-practice.mjs
import { build } from 'esbuild'; import path from 'node:path'; import os from 'node:os'; import fs from 'node:fs';
const out = fs.mkdtempSync(path.join(process.env.TMPDIR || os.tmpdir(), 'dn-practice-'));
const src = (f) => new URL(`../src/scripts/${f}`, import.meta.url).pathname;
for (const f of ['practice-store', 'practice-judge']) await build({ entryPoints: [src(f + '.ts')], bundle: true, format: 'esm', outfile: path.join(out, f + '.mjs'), logLevel: 'error' });
const mem = new Map(); let quotaFull = false;
globalThis.localStorage = { getItem: (k) => mem.get(k) ?? null, setItem: (k, v) => { if (quotaFull) throw new Error('quota'); mem.set(k, String(v)); } };
const events = []; globalThis.window = { addEventListener() {}, dispatchEvent: (e) => events.push(e.type) }; globalThis.CustomEvent = class { constructor(t) { this.type = t; } };
const S = await import(path.join(out, 'practice-store.mjs')), J = await import(path.join(out, 'practice-judge.mjs'));
let bad = 0; const ok = (c, m) => { if (!c) { bad++; console.log('FAIL', m); } else console.log('ok  ', m); };

// ---- store
ok(S.status('c/x') === 'todo', 'a new problem is todo');
S.setDraft('c/x', 'int f(void){return 1;}'); ok(S.status('c/x') === 'attempted' && S.getDraft('c/x').includes('return 1'), 'a draft marks the problem attempted and is read back');
const wrong = S.addSubmission('c/x', { verdict: 'Wrong Answer', passed: 2, total: 5, ms: 40, code: 'A' });
ok(S.status('c/x') === 'attempted' && S.getDraft('c/x') === 'A', 'a wrong submission keeps it attempted and becomes the draft');
S.addSubmission('c/x', { verdict: 'Accepted', passed: 5, total: 5, ms: 35, code: 'B' });
ok(S.status('c/x') === 'solved' && S.lastSubmission('c/x').code === 'B' && S.submissions('c/x').length === 2, 'an accepted submission solves it; newest first');
S.addSubmission('c/x', { verdict: 'Wrong Answer', passed: 1, total: 5, ms: 1, code: 'C' });
ok(S.status('c/x') === 'solved', 'a later failure does not un-solve it');
for (let i = 0; i < 40; i++) S.addSubmission('c/y', { verdict: 'Wrong Answer', passed: 0, total: 1, ms: 1, code: 'v' + i });
ok(S.submissions('c/y').length === S.MAX_SUBMISSIONS && S.submissions('c/y')[0].code === 'v39', 'history is capped, newest kept');
ok(events.includes('dn:practice'), 'a change event is fired');
const json = S.exportJson(); S.reset(); ok(S.status('c/x') === 'todo', 'reset clears'); ok(S.importJson(json) && S.status('c/x') === 'solved', 'export/import round trip');
ok(!S.importJson('{"v":2}') && !S.importJson('nope'), 'invalid imports are rejected');
quotaFull = true; ok(S.setDraft('c/z', 'q') === false && S.getDraft('c/z') === 'q', 'when storage is full the draft stays in memory');
quotaFull = false; mem.set('deepnotes:practice:v1', '{broken'); S._forTests(); ok(S.status('c/x') === 'todo', 'corrupt storage falls back to empty');

// ---- judge
const run = (stdout, extra = {}) => ({ phase: 'run', diagnostics: '', stdout, stderr: '', exitCode: 0, ...extra });
let j = J.judge(run('PASS a = 1\nPASS b = 2\n2 of 2 checks passed. Well done!\n'));
ok(j.verdict === 'Accepted' && j.passed === 2 && j.total === 2 && j.cases[1].got === '2', 'all PASS is Accepted');
j = J.judge(run('PASS a = 1\nFAIL f(3): expected 9, got 6\n     order: x y\n1 of 2 checks passed\n', { exitCode: 1 }));
ok(j.verdict === 'Wrong Answer' && j.cases[1].expected === '9' && j.cases[1].got === '6' && j.cases[1].label === 'f(3)' && j.stdout.includes('order'), 'a FAIL line is parsed into label, expected and got; other output is kept');
j = J.judge(run('FAIL x: no function pointer in the table\n0 of 1 checks passed\n', { exitCode: 1 }));
ok(j.verdict === 'Wrong Answer' && j.cases[0].expected === undefined, 'a FAIL line without expected/got is kept as text');
j = J.judge(run('PASS a = 1\n', { exitCode: 139, stderr: 'segmentation fault' }));
ok(j.verdict === 'Runtime Error' && j.passed === 1 && /segmentation/.test(j.message), 'no summary line means a runtime error');
j = J.judge({ phase: 'compile', diagnostics: 'main.c:3:1: error: x', stdout: '', stderr: '', exitCode: null });
ok(j.verdict === 'Compile Error' && /error/.test(j.message), 'compile phase is a compile error');
ok(J.judge(null, 'Stopped: the program ran longer than 20 seconds (infinite loop?)').verdict === 'Time Limit Exceeded', 'timeout is Time Limit Exceeded');
ok(J.judge(null, 'out of memory').verdict === 'Runtime Error', 'other worker failures are runtime errors');
ok(J.judge(run('0 of 0 checks passed. Well done!\n')).verdict === 'Wrong Answer', 'zero tests never counts as accepted');
ok(J.relocateDiagnostics('main.c:12:5: error: oops\nmain.c:3:1: warning: w', 10, 20) === 'line 2, col 5: error: oops\nmain.c:3:1 (outside your code): warning: w', 'diagnostics are mapped to editor lines');
const a = J.assemble('P\n', 'code\n\n', 'S\n'); ok(a === 'P\ncode\nS\n', 'assemble puts the reader code between prefix and suffix');
ok(J.countLines('a\nb\n') === 2, 'countLines');
const h = J.highlightC('#include <stdio.h>\nint main(void) { return 0x1F; } // hi "x"\n/* c */ char *s = "a<b";');
ok(h.includes('<span class="tk-p">#include</span>') && h.includes('<span class="tk-t">int</span>') && h.includes('<span class="tk-k">return</span>') && h.includes('<span class="tk-n">0x1F</span>') && h.includes('<span class="tk-c">// hi &quot;x&quot;'.replace('&quot;', '"').replace('&quot;', '"')) && h.includes('&lt;b'), 'highlighter marks directives, types, keywords, numbers, comments and escapes HTML');
ok(J.highlightC('a < b && c > d').includes('a &lt; b &amp;&amp; c &gt; d'), 'plain code is escaped');
let ind = J.indentAfterEnter('    if (x) {', 12); ok(ind.insert === '\n        ' && ind.caretBack === 0, 'Enter after { indents one more level');
ind = J.indentAfterEnter('  {}', 3); ok(ind.insert === '\n      \n  ' && ind.caretBack === 3, 'Enter between {} opens a block');
ind = J.indentAfterEnter('  x;', 4); ok(ind.insert === '\n  ', 'Enter keeps the indentation');
process.exit(bad ? 1 : 0);
