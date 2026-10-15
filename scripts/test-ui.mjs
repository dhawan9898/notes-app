import fs from 'node:fs'; import path from 'node:path';
import { Worker, isMainThread, parentPort } from 'node:worker_threads';
const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const HERE = path.join(ROOT, 'scripts/lib');
// UI behaviour tests: loads the built pages (dist/) in jsdom with the real page scripts and, for code problems, the real in-app C compiler.
// Usage: npm run build && node scripts/test-ui.mjs
if (!isMainThread) {
  await import(path.join(ROOT, 'public/runner/cc.js'));
  const dir = path.join(ROOT, 'public/runner/');
  const runner = new globalThis.CRunner({ loadBytes: async (n) => { const b = fs.readFileSync(dir + n); return b.buffer.slice(b.byteOffset, b.byteOffset + b.length); }, compileModule: (b) => WebAssembly.compile(b), now: () => performance.now() });
  parentPort.on('message', async ({ code }) => { try { parentPort.postMessage({ r: await runner.runC(code, { stdin: '', files: {} }) }); } catch (e) { parentPort.postMessage({ error: String(e) }); } });
} else {
  const { JSDOM } = await import('jsdom');
  const { build } = await import('esbuild');
  const bundles = {};
  const bundle = async (name) => bundles[name] ??= (await build({ entryPoints: [path.join(ROOT, 'src/scripts', name + '.ts')], bundle: true, format: 'iife', write: false, logLevel: 'error', external: ['mermaid'],
    plugins: [{ name: 'stub', setup(b) { b.onResolve({ filter: /^\.\/runner$/ }, () => ({ path: path.join(HERE, 'stub-runner.ts') })); } }] })).outputFiles[0].text;
  const worker = new Worker(new URL(import.meta.url));
  const hostRun = (code) => new Promise((res, rej) => { worker.once('message', (m) => (m.error ? rej(new Error(m.error)) : res(m.r))); worker.postMessage({ code }); });
  const problems = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/generated/practice.json'), 'utf8'));
  const searchIndex = fs.readFileSync(path.join(ROOT, 'dist/search-index.json'), 'utf8');
  let bad = 0; const ok = (c, m) => { if (!c) { bad++; console.log('FAIL', m); } else console.log('ok  ', m); };
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const page = (p) => fs.readFileSync(path.join(ROOT, 'dist', p, 'index.html'), 'utf8').replace(/<script[^>]*\bsrc=[^>]*><\/script>/g, '').replace(/<script type="module"[\s\S]*?<\/script>/g, '');
  const open = async (p, scripts, storage = {}, url = `http://localhost/${p}/`) => {
    const dom = new JSDOM(page(p), { url, runScripts: 'outside-only', pretendToBeVisual: true });
    const w = dom.window; w.__run = hostRun; w.scrollTo = () => {}; w.matchMedia = (q) => ({ matches: /max-width: 900px/.test(q), addEventListener() {}, removeEventListener() {} });
    w.fetch = async () => ({ ok: true, json: async () => JSON.parse(searchIndex) }); w.HTMLDialogElement.prototype.showModal = function () { this.setAttribute('open', ''); }; w.HTMLDialogElement.prototype.close = function () { this.removeAttribute('open'); };
    for (const [k, v] of Object.entries(storage)) w.localStorage.setItem(k, v);
    for (const s of scripts) w.eval(await bundle(s));
    return w;
  };
  const $ = (w, s) => w.document.querySelector(s), $$ = (w, s) => [...w.document.querySelectorAll(s)];
  const until = async (w, f, ms = 60000) => { const t0 = Date.now(); while (Date.now() - t0 < ms) { const v = f(); if (v) return v; await sleep(50); } throw new Error('timeout: ' + f); };
  const env = (obj) => JSON.stringify({ v: 1, data: obj });

  /* ---- home / track: meters, toggles, filters */
  const manifest = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/generated/manifest.json'), 'utf8'));
  const firstC = manifest.lessons.filter((l) => l.r === 'c');
  let w = await open('', ['shell'], { 'dn:progress': env({ done: { [firstC[0].i]: Date.now(), [firstC[1].i]: Date.now(), 'removed/stale-note': 1 }, scroll: {}, days: {} }) });
  await until(w, () => $(w, '[data-stat="done"]').textContent !== '0');
  ok($(w, '[data-stat="done"]').textContent === '2', 'home counts only existing lessons (stale ids ignored): ' + $(w, '[data-stat="done"]').textContent);
  const cCard = $$(w, '.track-card').find((c) => c.dataset.prefix === 'c/');
  ok(cCard.querySelector('[data-n-done]').textContent === '2' && cCard.style.getPropertyValue('--p') !== '0', 'track card meter shows 2 done');
  ok($(w, '[data-continue="all"]').href.includes(firstC[2].u) && $(w, '[data-continue-label]').textContent === 'Up next', 'continue points at the next unfinished lesson: ' + $(w, '[data-continue="all"]').href);
  w = await open('c', ['shell']);
  await until(w, () => $(w, '[data-continue="c"]') && !$(w, '[data-continue="c"]').hidden);
  const row = $(w, `[data-item="${firstC[0].i}"]`); $(w, `[data-toggle="${firstC[0].i}"]`).click();
  ok(row.hasAttribute('data-done') && JSON.parse(w.localStorage.getItem('dn:progress')).data.done[firstC[0].i], 'toggle marks a lesson done and persists it');
  $$(w, '[data-filter-status]').find((b) => b.dataset.filterStatus === 'done').click();
  ok($$(w, '[data-filterable]').filter((r) => !r.hidden).length === 1, 'Done filter shows only completed lessons');
  $$(w, '[data-filter-status]').find((b) => b.dataset.filterStatus === 'todo').click();
  ok($$(w, '[data-filterable]').filter((r) => r.hidden).length === 1, 'To do filter hides it');
  ok(w.document.querySelectorAll('.stage').length === 3 && w.document.querySelectorAll('.module').length > 5, 'track page has stages and modules');

  /* ---- practice list filters */
  w = await open('practice', ['shell', 'practice-list']);
  const rows = $$(w, '[data-pr]'); ok(rows.length === problems.length, 'all problems listed');
  $$(w, '[data-pl-track]').find((b) => b.dataset.plTrack === 'dsa').click(); ok(rows.filter((r) => !r.hidden).every((r) => r.dataset.track === 'dsa'), 'track filter');
  $$(w, '[data-pl-diff]').find((b) => b.dataset.plDiff === 'Hard').click(); const hard = rows.filter((r) => !r.hidden); ok(hard.length > 0 && hard.every((r) => r.dataset.diff === 'Hard' && r.dataset.track === 'dsa'), 'difficulty filter combines with track');
  $$(w, '[data-pl-kind]').find((b) => b.dataset.plKind === 'question').click(); ok(rows.filter((r) => !r.hidden).every((r) => r.dataset.kind === 'question'), 'kind filter');
  w = await open('practice', ['shell', 'practice-list'], {}, 'http://localhost/practice/?track=c&module=Memory%20and%20Pointers');
  ok(rows.length && $$(w, '[data-pr]').filter((r) => !r.hidden).length === problems.filter((p) => p.track === 'c' && p.module === 'Memory and Pointers').length, '?track=&module= preselects');

  /* ---- code problem end to end */
  const P = problems.find((p) => p.id === 'c/swap-ints');
  w = await open('practice/c/swap-ints', ['shell', 'problem']);
  const ta = $(w, '[data-ed-ta]');
  ok(ta.value === P.code.starter.replace(/\n*$/, '\n') && !ta.value.includes('main('), 'editor holds only the function stub');
  ok($(w, '[data-ed-gutter]').textContent.split('\n').length === ta.value.split('\n').length, 'gutter lines');
  const type = (text) => { ta.value = text; ta.dispatchEvent(new w.Event('input', { bubbles: true })); };
  $(w, '[data-pp-run]').click(); await until(w, () => $(w, '.verdict'));
  ok(/Wrong Answer/.test($(w, '.verdict').textContent) && w.document.querySelectorAll('.case').length === 4, 'stub: Wrong Answer with 4 case chips');
  ok(!JSON.parse(w.localStorage.getItem('dn:practice') || '{"data":{"p":{}}}').data.p['c/swap-ints']?.subs?.length, 'Run records nothing');
  type('void swap_ints(int *a, int *b) {\n    int t = *a\n    *a = *b; *b = t;\n}\n'); $(w, '[data-pp-run]').click(); await until(w, () => /Compile Error/.test($(w, '.verdict')?.textContent || ''));
  ok(/line 3/.test($(w, '.out.err').textContent), 'compile error mapped to the editor line: ' + $(w, '.out.err').textContent.split('\n')[0]);
  type(P.code.solution); $(w, '[data-pp-submit]').click(); await until(w, () => $(w, '.verdict.ok'));
  const st = JSON.parse(w.localStorage.getItem('dn:practice')).data.p['c/swap-ints'];
  ok(/Accepted/.test($(w, '.verdict').textContent) && st.solved && st.subs[0].code.includes('*a'), 'solution is Accepted and saved');
  ok(!$(w, '[data-pp-solved]').hidden && $(w, '[data-pp-subcount]').textContent === '1', 'solved badge and submissions count');
  const st2 = w.localStorage.getItem('dn:practice'); type(P.code.solution + '// edited\n'); await sleep(900);
  const w2 = await open('practice/c/swap-ints', ['shell', 'problem'], { 'dn:practice': w.localStorage.getItem('dn:practice') });
  ok($(w2, '[data-ed-ta]').value.includes('// edited'), 'unsubmitted draft restored on reopen'); void st2;
  $(w2, '[data-pp-reset]').click(); ok($(w2, '[data-ed-ta]').value.includes('// edited'), 'reset needs a second tap'); $(w2, '[data-pp-reset]').click(); ok($(w2, '[data-ed-ta]').value === P.code.starter.replace(/\n*$/, '\n'), 'reset restores the stub');
  $(w2, '[data-tab="hints"]').click(); $(w2, '[data-pp-hint-btn]').click(); ok($$(w2, '[data-pp-hints] li').length === 1, 'hints one at a time');
  $(w2, '[data-tab="solution"]').click(); ok($(w2, '[data-pp-sol]').hidden, 'solution gated'); $(w2, '[data-pp-sol-btn]').click(); ok(!$(w2, '[data-pp-sol]').hidden && /swap_ints/.test($(w2, '[data-pp-sol-code]').textContent), 'solution revealed with code');

  /* ---- question problem */
  const Q = problems.find((p) => p.kind === 'question');
  w = await open(`practice/${Q.track}/${Q.slug}`, ['shell', 'problem']);
  const notes = $(w, '[data-notes]'); notes.value = 'my reasoning'; notes.dispatchEvent(new w.Event('input', { bubbles: true })); await sleep(900);
  ok(JSON.parse(w.localStorage.getItem('dn:practice')).data.p[Q.id].draft === 'my reasoning', 'question notes are saved');
  $(w, '[data-q-got]').click(); ok(JSON.parse(w.localStorage.getItem('dn:practice')).data.p[Q.id].solved && !$(w, '[data-pp-solved]').hidden, '"I got it" marks it solved');
  $(w, '[data-q-review]').click(); ok(!JSON.parse(w.localStorage.getItem('dn:practice')).data.p[Q.id].solved, '"Review again" unmarks it');

  /* ---- lesson */
  const L = manifest.lessons[5];
  w = await open(L.u.replace(/^\/|\/$/g, ''), ['shell', 'lesson']);
  ok($(w, '[data-toggle][data-complete]'), 'lesson has a complete button');
  $(w, '[data-complete]').click(); await sleep(50);
  ok(JSON.parse(w.localStorage.getItem('dn:progress')).data.done[L.i], 'completing a lesson persists it');
  ok($(w, '[data-complete]').getAttribute('aria-pressed') === 'true', 'button reflects the state');
  await sleep(700); ok(JSON.parse(w.localStorage.getItem('dn:progress')).data.last.id === L.i, 'last-read lesson recorded');
  ok($$(w, '.prose pre .copy').length > 0 || !$(w, '.prose pre'), 'code blocks get copy buttons');

  /* ---- search */
  w = await open('search', ['shell', 'search'], {}, 'http://localhost/search/?q=spanning');
  await until(w, () => $$(w, '.result').length > 0);
  ok(/pann?ing/i.test($(w, '.result strong').textContent), 'search finds a lesson: ' + $(w, '.result strong').textContent);
  const inp = $(w, '[data-search-input]'); inp.value = 'qqqzzz'; inp.dispatchEvent(new w.Event('input')); await sleep(100);
  ok($$(w, '.result').length === 0 && /Nothing found/.test($(w, '[data-search-hint]').textContent), 'no results message');

  /* ---- you: settings and backup */
  w = await open('you', ['shell', 'you']);
  $$(w, '[data-set][data-value="dark"]')[0].click(); ok(w.document.documentElement.dataset.theme === 'dark' && JSON.parse(w.localStorage.getItem('dn:settings')).data.theme === 'dark', 'theme setting applies and persists');
  $$(w, '[data-set][data-value="l"]')[0].click(); ok(w.document.documentElement.dataset.text === 'l', 'text size applies');
  $(w, '[data-theme-toggle]').click(); ok(w.document.documentElement.dataset.theme === 'light', 'header toggle flips the theme');
  worker.terminate();
  console.log(bad ? `${bad} FAILED` : 'all UI checks passed'); process.exit(bad ? 1 : 0);
}
