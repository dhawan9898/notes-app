// Unit tests for the framework-free browser modules: storage layer, progress, practice, settings, backup, judge, search.
// Usage: node scripts/test-lib.mjs
import { build } from 'esbuild'; import path from 'node:path'; import os from 'node:os'; import fs from 'node:fs';
const out = fs.mkdtempSync(path.join(process.env.TMPDIR || os.tmpdir(), 'dn-lib-'));
const src = (f) => new URL(`../src/scripts/lib/${f}`, import.meta.url).pathname;
const mods = ['store', 'progress', 'practice', 'settings', 'backup', 'judge', 'search', 'terminal-queue'].filter((m) => fs.existsSync(src(m + '.ts')));
await build({ entryPoints: mods.map(src), bundle: false, format: 'esm', outdir: out, logLevel: 'error', outExtension: { '.js': '.mjs' } });
// bundle each entry separately so the JSON aliases and relative imports resolve
for (const m of mods) await build({ entryPoints: [src(m + '.ts')], bundle: true, format: 'esm', outfile: path.join(out, m + '.bundle.mjs'), logLevel: 'error' });

/* ---- a minimal browser environment */
const mem = new Map(); let full = false, blocked = false;
globalThis.localStorage = { getItem: (k) => { if (blocked) throw new Error('blocked'); return mem.get(k) ?? null; }, setItem: (k, v) => { if (full) throw new Error('quota'); mem.set(k, String(v)); }, removeItem: (k) => mem.delete(k) };
const handlers = {}, events = [];
globalThis.window = { addEventListener: (t, f) => (handlers[t] ||= []).push(f), dispatchEvent: (e) => { events.push(e.type); return true; } };
globalThis.document = { addEventListener: (t, f) => (handlers['doc:' + t] ||= []).push(f), hidden: false };
globalThis.CustomEvent = class { constructor(t, o) { this.type = t; this.detail = o?.detail; } };
const load = (m) => import(path.join(out, m + '.bundle.mjs') + '?' + Math.random());
let bad = 0; const ok = (c, m) => { if (!c) { bad++; console.log('FAIL', m); } else console.log('ok  ', m); };
const today = () => { const d = new Date(); return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`; };

/* ---- store */
{
  const { createStore, isObj } = await load('store');
  const mk = (extra = {}) => createStore({ key: 't:a', version: 2, empty: () => ({ n: 0 }), check: (d) => isObj(d) && typeof d.n === 'number', migrations: { 1: (d) => ({ n: d.count }) }, ...extra });
  let s = mk(); ok(s.get().n === 0, 'a new store is empty');
  s.update((d) => { d.n = 5; }); ok(JSON.parse(mem.get('t:a')).v === 2 && JSON.parse(mem.get('t:a')).data.n === 5, 'writes a versioned envelope');
  mem.set('t:a', JSON.stringify({ v: 1, data: { count: 9 } })); s = mk(); ok(s.get().n === 9, 'migrates data written by an older version');
  mem.set('t:a', JSON.stringify({ v: 3, data: { n: 1 } })); s = mk(); ok(s.get().n === 0 && s.state === 'recovered' && mem.has('t:a:corrupt'), 'data from a newer version is set aside, not destroyed');
  mem.set('t:a', '{oops'); s = mk(); ok(s.get().n === 0 && s.state === 'recovered', 'invalid JSON recovers to empty');
  mem.set('t:a', JSON.stringify({ v: 2, data: { n: 'x' } })); s = mk(); ok(s.get().n === 0 && s.state === 'recovered', 'data failing validation recovers to empty');
  mem.delete('t:a'); mem.set('old', JSON.stringify({ n: 4 })); s = mk({ legacy: { key: 'old', convert: (r) => ({ n: r.n }) } }); ok(s.get().n === 4 && JSON.parse(mem.get('t:a')).data.n === 4, 'imports a legacy key and writes the new one');
  mem.delete('t:a'); full = true; s = mk(); let errs = events.filter((e) => e === 'dn:storage-error').length;
  s.update((d) => { d.n = 7; }); s.update((d) => { d.n = 8; }); ok(s.get().n === 8 && s.state === 'memory-only' && events.filter((e) => e === 'dn:storage-error').length === errs + 1, 'a full storage keeps data in memory and reports once');
  full = false; s.update((d) => { d.n = 9; }); ok(s.state === 'ok' && JSON.parse(mem.get('t:a')).data.n === 9, 'recovers when storage works again');
  blocked = true; s = mk(); ok(s.get().n === 0, 'blocked storage (private mode) still works in memory'); blocked = false;
  s = mk(); s.update((d) => { d.n = 1; }, { defer: true }); const before = JSON.parse(mem.get('t:a')).data.n; s.flush(); ok(before !== 1 && JSON.parse(mem.get('t:a')).data.n === 1, 'deferred writes wait for flush');
  let calls = 0; const un = s.subscribe(() => calls++); s.update((d) => { d.n = 2; }); un(); s.update((d) => { d.n = 3; }); ok(calls === 1, 'subscribers are notified, and unsubscribe works');
  ok(s.replace({ n: 'bad' }) === false && s.get().n === 3, 'replace rejects invalid data and changes nothing');
  mem.set('t:a', JSON.stringify({ v: 2, data: { n: 42 } })); handlers.storage.forEach((f) => f({ key: 't:a' })); ok(s.get().n === 42, 'another tab’s write is picked up');
  s.reset(); ok(s.get().n === 0, 'reset');
}
/* ---- progress */
{
  mem.clear(); const aliases = JSON.parse(fs.readFileSync(new URL('../src/data/id-aliases.json', import.meta.url)));
  const [oldId, newId] = Object.entries(aliases).find(([, v]) => !v.startsWith('lab:'));
  mem.set('deepnotes:progress:v1', JSON.stringify({ v: 1, done: { [oldId]: 1000 }, scroll: { [oldId]: 0.5 }, days: { '2020-01-01': 60 } }));
  const p = await load('progress');
  ok(p.isDone(newId) && !p.isDone(oldId) && p.progress.get().scroll[newId] === 0.5, 'legacy progress is imported and renamed notes keep their completion');
  p.setDone('x', true); ok(p.isDone('x'), 'mark done'); p.setDone('x', false); ok(!p.isDone('x'), 'unmark');
  p.progress.reset(); p.setDone('a', true); ok(p.streak() === 1, 'a completion today is a 1-day streak');
  const d = (n) => { const x = new Date(); x.setDate(x.getDate() - n); return `${x.getFullYear()}-${String(x.getMonth() + 1).padStart(2, '0')}-${String(x.getDate()).padStart(2, '0')}`; };
  p.progress.update((s) => { s.days[d(1)] = 600; s.days[d(2)] = 60; s.days[d(4)] = 600; s.days[d(3)] = 10; });
  ok(p.streak() === 3, 'streak counts consecutive days and stops at a day under 30 s (' + p.streak() + ')');
  p.progress.reset(); p.addActive(120); p.addActive(60); ok(p.totalMinutes() === 3, 'active minutes add up'); p.setScroll('n', 1.7); ok(p.progress.get().scroll.n === 1, 'scroll is clamped');
  p.setDone('a', true); p.setDone('b', true); ok(p.countDone(['a', 'b', 'c']) === 2, 'countDone');
}
/* ---- practice */
{
  mem.clear(); const q = await load('practice');
  ok(q.status('c/x') === 'todo', 'new problem is todo'); q.setDraft('c/x', 'A'); ok(q.status('c/x') === 'attempted' && q.getDraft('c/x') === 'A', 'draft marks attempted');
  q.addSubmission('c/x', { verdict: 'Wrong Answer', passed: 1, total: 4, ms: 3, code: 'B' }); ok(q.getDraft('c/x') === 'B' && q.status('c/x') === 'attempted', 'a submission becomes the draft');
  q.addSubmission('c/x', { verdict: 'Accepted', passed: 4, total: 4, ms: 3, code: 'C' }); ok(q.status('c/x') === 'solved' && q.lastSubmission('c/x').code === 'C' && q.submissions('c/x').length === 2, 'accepted solves it; newest first');
  q.addSubmission('c/x', { verdict: 'Wrong Answer', passed: 0, total: 4, ms: 3, code: 'D' }); ok(q.status('c/x') === 'solved', 'a later failure does not unsolve');
  for (let i = 0; i < 40; i++) q.addSubmission('c/y', { verdict: 'Wrong Answer', passed: 0, total: 1, ms: 1, code: 'v' + i }); ok(q.submissions('c/y').length === q.MAX_SUBMISSIONS && q.submissions('c/y')[0].code === 'v39', 'history capped, newest kept');
  q.setSolved('c/z', true); ok(q.status('c/z') === 'solved', 'questions can be marked solved'); q.setSolved('c/z', false); ok(q.status('c/z') !== 'solved', 'and unmarked');
  ok(q.solvedCount(['c/x', 'c/y', 'c/z']) === 1, 'solvedCount');
  mem.clear(); mem.set('deepnotes:practice:v1', JSON.stringify({ v: 1, p: { 'c/l': { subs: [], solved: 5 } } })); const q2 = await load('practice'); ok(q2.status('c/l') === 'solved', 'legacy practice data is imported');
}
/* ---- settings and backup */
{
  mem.clear(); mem.set('theme', 'dark'); const st = await load('settings'); ok(st.settings.get().theme === 'dark', 'legacy theme key is imported');
  const b = await load('backup'), p = await load('progress'), q = await load('practice'); const s2 = await load('settings');
  // each bundle has its own store instance over the same storage; use the backup module's own instances
  const lib = await import(path.join(out, 'backup.bundle.mjs'));
  ok(typeof lib.exportAll() === 'string' && JSON.parse(lib.exportAll()).format === 'deepnotes-backup', 'export produces a backup envelope');
  ok(!lib.importAll('nope').ok && !lib.importAll('{"format":"x"}').ok, 'import rejects garbage');
  const good = JSON.stringify({ format: 'deepnotes-backup', v: 1, progress: { done: { a: 1 }, scroll: {}, days: {} }, practice: { p: {} }, settings: { theme: 'dark', text: 'm', motion: 'system' } });
  ok(lib.importAll(good).ok, 'import accepts a valid backup');
  const before = JSON.parse(lib.exportAll());
  ok(!lib.importAll(JSON.stringify({ format: 'deepnotes-backup', v: 1, progress: { done: { a: 1 }, scroll: {}, days: {} }, practice: { p: 'bad' } })).ok && JSON.parse(lib.exportAll()).progress.done.a === 1, 'a partly invalid backup is refused as a whole');
  void p; void q; void s2; void before;
}
/* ---- judge (see src/scripts/lib/judge.ts) */
{
  const J = await load('judge'); const run = (stdout, x = {}) => ({ phase: 'run', diagnostics: '', stdout, stderr: '', exitCode: 0, ...x });
  let j = J.judge(run('PASS a = 1\nPASS b = 2\n2 of 2 checks passed. Well done!\n')); ok(j.verdict === 'Accepted' && j.passed === 2 && j.cases[1].got === '2', 'all PASS is Accepted');
  j = J.judge(run('PASS a = 1\nFAIL f(3): expected 9, got 6\n     order: x y\n1 of 2 checks passed\n', { exitCode: 1 })); ok(j.verdict === 'Wrong Answer' && j.cases[1].expected === '9' && j.cases[1].got === '6' && j.cases[1].label === 'f(3)' && j.stdout.includes('order'), 'FAIL lines are parsed');
  j = J.judge(run('FAIL x: no function pointer in the table\n0 of 1 checks passed\n', { exitCode: 1 })); ok(j.verdict === 'Wrong Answer' && j.cases[0].expected === undefined, 'FAIL without expected/got is kept as text');
  j = J.judge(run('PASS a = 1\n', { exitCode: 139, stderr: 'segmentation fault' })); ok(j.verdict === 'Runtime Error' && j.passed === 1, 'no summary is a runtime error');
  ok(J.judge({ phase: 'compile', diagnostics: 'main.c:3:1: error: x', stdout: '', stderr: '', exitCode: null }).verdict === 'Compile Error', 'compile phase is a compile error');
  ok(J.judge(null, 'Stopped: the program ran longer than 20 seconds').verdict === 'Time Limit Exceeded' && J.judge(null, 'oom').verdict === 'Runtime Error', 'timeout and worker failures');
  ok(J.judge(run('0 of 0 checks passed. Well done!\n')).verdict === 'Wrong Answer', 'zero tests is never accepted');
  ok(J.relocateDiagnostics('main.c:12:5: error: oops\nmain.c:3:1: warning: w', 10, 20) === 'line 2, col 5: error: oops\nmain.c:3:1 (outside your code): warning: w', 'diagnostics map to editor lines');
  ok(J.assemble('P\n', 'code\n\n', 'S\n') === 'P\ncode\nS\n' && J.countLines('a\nb\n') === 2, 'assemble and countLines');
  const h = J.highlightC('#include <stdio.h>\nint main(void) { return 0x1F; } // hi\n/* c */ char *s = "a<b";');
  ok(h.includes('<span class="tk-p">#include</span>') && h.includes('<span class="tk-t">int</span>') && h.includes('<span class="tk-k">return</span>') && h.includes('<span class="tk-n">0x1F</span>') && h.includes('&lt;b'), 'highlighter');
  let i = J.indentAfterEnter('    if (x) {', 12); ok(i.insert === '\n        ' && i.caretBack === 0, 'Enter after { indents'); i = J.indentAfterEnter('  {}', 3); ok(i.insert === '\n      \n  ' && i.caretBack === 3, 'Enter between {} opens a block'); i = J.indentAfterEnter('  x;', 4); ok(i.insert === '\n  ', 'Enter keeps indentation');
}
/* ---- search (src/scripts/lib/search.ts) */
if (mods.includes('search')) {
  const S = await load('search'); const idx = JSON.parse(fs.readFileSync(new URL('../src/data/generated/search-index.json', import.meta.url)));
  const top = (q, n = 5) => S.search(idx, q).slice(0, n).map((r) => r.t);
  ok(top('spanning tree')[0].toLowerCase().includes('spanning'), 'finds a lesson by words in its title: ' + top('spanning tree')[0]);
  ok(S.search(idx, 'ospf').some((r) => r.k === 'l'), 'finds lessons for an acronym');
  ok(S.search(idx, 'binary search').slice(0, 5).some((r) => r.k === 'p' || r.k === 'l'), 'mixes lessons and problems');
  ok(S.search(idx, 'zzzzqqq').length === 0, 'no match, no results');
  ok(S.search(idx, '  ').length === 0 && S.search(idx, 'a').length === 0, 'blank or one-letter queries return nothing');
  ok(S.search(idx, 'mtu')[0], 'headings are searchable (mtu)');
  ok(top('tcp').length > 0 && S.search(idx, 'TCP')[0].t === S.search(idx, 'tcp')[0].t, 'case-insensitive');
}
/* ---- terminal queue (src/scripts/lib/terminal-queue.ts) */
{
  const Q = await load('terminal-queue');
  const block = 'intro line\n$ ls -l /tmp   # list it\nfile1 file2\n$ echo "a b"\na b\n$   \n$ cat f';
  ok(JSON.stringify(Q.commandsFromBlock(block)) === JSON.stringify(['ls -l /tmp', 'echo "a b"', 'cat f']), 'commands are the "$ " lines; output, blank prompts and trailing comments are dropped');
  const q = Q.queueQuery(['echo "a&b"', 'ls /tmp']);
  ok(JSON.stringify(Q.commandsFromQuery(q)) === JSON.stringify(['echo "a&b"', 'ls /tmp']), 'commands survive the round trip through a URL, including & and quotes');
  ok(Q.commandsFromQuery('?c=' + encodeURIComponent('a\nb')).join('|') === 'a b', 'newlines in a command are flattened to spaces');
  ok(Q.commandsFromQuery('?c=' + 'x'.repeat(500)).length === 0, 'an absurdly long command is ignored');
  ok(Q.commandsFromQuery('?' + Array.from({ length: 50 }, (_, i) => 'c=cmd' + i).join('&')).length === 30, 'at most 30 commands are accepted from a link');
  ok(Q.queueQuery(Array.from({ length: 50 }, (_, i) => 'c' + i)).split('&').length === 30, 'and at most 30 are put into a link');
  ok(Q.commandsFromQuery('').length === 0 && Q.commandsFromQuery('?x=1').length === 0, 'no commands in the URL, no queue');
  ok(Q.ctrlOf('c') === '\x03' && Q.ctrlOf('D') === '\x04' && Q.ctrlOf('1') === null && Q.ctrlOf('ab') === null, 'Ctrl plus a letter gives the control character');
  ok(Q.KEY_CODES.Esc === '\x1b' && Q.KEY_CODES.Up === '\x1b[A' && Q.KEY_CODES.Tab === '\t', 'key row codes');
}
process.exit(bad ? 1 : 0);
