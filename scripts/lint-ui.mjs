// Mechanical UI checks that do not need a browser:
//  1. every class used in markup (.astro, and class="…" strings inside scripts) is defined in a stylesheet
//  2. every class defined in a stylesheet is mentioned somewhere in the sources (dead CSS)
//  3. every data-* hook a script reads exists in some markup, and every JS-read selector hook is written somewhere
// Usage: node scripts/lint-ui.mjs
import fs from 'node:fs'; import path from 'node:path';
const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
const read = (f) => fs.readFileSync(f, 'utf8');
const src = walk(path.join(ROOT, 'src')).filter((f) => /\.(astro|ts|mjs)$/.test(f));
const css = walk(path.join(ROOT, 'src/styles')).filter((f) => f.endsWith('.css'));

// ---- classes defined in CSS
const defined = new Set();
for (const f of css) {
  const text = read(f).replace(/\/\*[\s\S]*?\*\//g, '').replace(/\{[^{}]*\}/g, (m) => (m.includes('{') ? m : '{}')).replace(/"[^"]*"|'[^']*'/g, '""');
  for (const m of text.matchAll(/\.([a-zA-Z_][\w-]*)/g)) defined.add(m[1]);
}
// classes written by plugins/scripts at run time or build time, and by Astro/Shiki/third parties
const EXTRA = new Set(['xterm', 'xterm-viewport', 'astro-code', 'astro-code-themes', 'github-light', 'github-dark', 'line', 'icon', 'on', 'in', 'armed', 'running', 'lit', 'ok', 'bad', 'warn', 'err', 'sub', 'off', 'has-copy', 'copy', 'anchor', 'gl', 'diagram', 'runbox']);
const DYNAMIC = [/^k\d$/, /^tk-/, /^lvl-/, /^d-/, /^k-/, /^kind-/];
const dyn = (c) => EXTRA.has(c) || DYNAMIC.some((r) => r.test(c));

// ---- classes used in sources
const used = new Map();                                            // class -> file
const addClasses = (str, file) => { for (const c of str.split(/\s+/)) if (/^[a-z][\w-]*$/i.test(c) && !/^(is|as)$/.test(c)) used.set(c, used.get(c) ?? file); };
for (const f of src) {
  const t = read(f), rel = path.relative(ROOT, f);
  for (const m of t.matchAll(/\bclass="([^"{}]*)"/g)) addClasses(m[1], rel);
  for (const m of t.matchAll(/\bclass=\{`([^`$]*)`\}/g)) addClasses(m[1], rel);
  for (const m of t.matchAll(/class:list=\{\[([^\]]*)\]\}/g)) for (const s of m[1].matchAll(/'([^']+)'/g)) addClasses(s[1], rel);
  for (const m of t.matchAll(/classList\.(?:add|toggle|remove|contains)\('([^']+)'/g)) addClasses(m[1], rel);
  for (const m of t.matchAll(/className = '([^']+)'/g)) addClasses(m[1], rel);
}
const errors = [];
for (const [c, f] of used) if (!defined.has(c) && !dyn(c)) errors.push(`class "${c}" is used in ${f} but defined in no stylesheet`);

// ---- dead CSS: a defined class must appear as a word in some source file (or be dynamic)
const corpus = src.map(read).join('\n') + '\n' + walk(path.join(ROOT, 'src/content')).filter((f) => f.endsWith('.md')).slice(0, 0).join('');
const words = new Set(corpus.match(/[A-Za-z_][\w-]*/g));
for (const c of defined) if (!dyn(c) && !words.has(c) && !/^(i-sun|i-moon)$/.test(c)) errors.push(`class "${c}" is defined in CSS but never used (dead style)`);

// ---- data-* hooks: scripts read [data-x] selectors; markup must write data-x somewhere
const markup = src.filter((f) => f.endsWith('.astro') || f.includes('scripts/')).map(read).join('\n');
const written = new Set([...markup.matchAll(/\bdata-([a-z][\w-]*)/g)].map((m) => m[1]));
for (const f of src.filter((f) => f.includes('scripts/'))) {
  for (const m of read(f).matchAll(/\[data-([a-z][\w-]*)/g)) if (!written.has(m[1])) errors.push(`${path.relative(ROOT, f)} reads [data-${m[1]}] which no markup writes`);
}
// the other direction: attributes written in .astro that no script/CSS ever reads (dead hooks)
const readers = src.filter((f) => f.includes('scripts/')).map(read).join('\n') + css.map(read).join('\n');
const astroText = src.filter((f) => f.endsWith('.astro')).map(read).join('\n');
const SERVER_ONLY = new Set(['theme', 'text', 'motion', 'tint', 'language', 'spy', 'ex']);
for (const m of new Set([...astroText.matchAll(/\bdata-([a-z][\w-]*)/g)].map((x) => x[1]))) {
  const camel = m.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
  if (SERVER_ONLY.has(m)) continue;
  if (!new RegExp(`data-${m}\\b|dataset\\.${camel}\\b|dataset\\[['"]${camel}`).test(readers)) errors.push(`attribute data-${m} is written in markup but nothing reads it`);
}
if (errors.length) { console.error([...new Set(errors)].join('\n')); console.error(`\n${new Set(errors).size} problem(s)`); process.exit(1); }
console.log(`ui OK: ${defined.size} classes defined, ${used.size} used, data hooks consistent`);
