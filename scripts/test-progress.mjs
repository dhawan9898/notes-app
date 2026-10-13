import { build } from 'esbuild';
await build({ entryPoints: [new URL('../src/scripts/progress.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: '' + (process.env.TMPDIR || '/tmp') + '/dn-progress.mjs', logLevel: 'error' });
const store = new Map();
globalThis.localStorage = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, String(v)) };
const ev = []; globalThis.window = { addEventListener() {}, dispatchEvent: (e) => ev.push(e.type) }; globalThis.CustomEvent = class { constructor(t) { this.type = t; } };
const p = await import('' + (process.env.TMPDIR || '/tmp') + '/dn-progress.mjs');
const assert = (c, m) => { if (!c) { console.log('FAIL', m); process.exitCode = 1; } else console.log('ok  ', m); };
assert(!p.isDone('a'), 'starts empty');
p.setDone('a', true); assert(p.isDone('a'), 'mark done'); assert(ev.includes('dn:change'), 'change event fired');
p.setDone('a', false); assert(!p.isDone('a'), 'unmark');
p.setDone('b', true); assert(p.streak() === 1, 'streak is 1 after a completion today');
// activity on previous days builds the streak
const s = JSON.parse(p.exportJson()); const d = (n) => { const x = new Date(); x.setDate(x.getDate() - n); return `${x.getFullYear()}-${String(x.getMonth()+1).padStart(2,'0')}-${String(x.getDate()).padStart(2,'0')}`; };
s.days[d(1)] = 600; s.days[d(2)] = 60; s.days[d(4)] = 600; assert(p.importJson(JSON.stringify(s)), 'import valid json');
assert(p.streak() === 3, 'streak counts consecutive days (today, -1, -2) and stops at the gap on -3: ' + p.streak());
assert(!p.importJson('{"v":2}') && !p.importJson('nope'), 'rejects invalid imports');
p.addActive(120); assert(p.totalMinutes() === Math.round((600 + 60 + 600 + 120) / 60), 'minutes learned adds up: ' + p.totalMinutes());
p.setScroll('b', 0.5); assert(JSON.parse(p.exportJson()).scroll.b === 0.5, 'scroll position stored');
p.reset(); assert(!p.isDone('b') && p.streak() === 0, 'reset clears everything');
