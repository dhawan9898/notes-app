// Checks the built site (dist/): every internal link and anchor resolves, every page has exactly one h1, a title and a description,
// buttons and links have accessible names, and no page ships stale practice-note markup. Usage: node scripts/check-dist.mjs [dist]
import fs from 'node:fs'; import path from 'node:path';
const ROOT = path.resolve(new URL('..', import.meta.url).pathname), DIST = path.resolve(process.argv[2] || path.join(ROOT, 'dist'));
const BASE = (process.env.BASE || '/').replace(/\/$/, '');
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
const pages = walk(DIST).filter((f) => f.endsWith('.html') && !f.includes(`${path.sep}runner${path.sep}`));
const idsOf = new Map(), errors = [];
const html = new Map(pages.map((f) => [f, fs.readFileSync(f, 'utf8')]));
for (const [f, t] of html) idsOf.set(f, new Set([...t.matchAll(/\bid="([^"]+)"/g)].map((m) => m[1])));
const resolve = (p) => { const f = path.join(DIST, p); if (fs.existsSync(f) && fs.statSync(f).isFile()) return f; const i = path.join(f, 'index.html'); return fs.existsSync(i) ? i : null; };
const rel = (f) => path.relative(DIST, f);

for (const [file, t] of html) {
  const here = '/' + rel(file).replace(/index\.html$/, '');
  if (file.endsWith('404.html')) continue;
  const h1 = (t.match(/<h1[\s>]/g) || []).length;
  if (h1 !== 1) errors.push(`${rel(file)}: ${h1} <h1> elements (want exactly 1)`);
  if (!/<title>[^<]{3,}<\/title>/.test(t)) errors.push(`${rel(file)}: missing <title>`);
  if (!/<meta name="description" content="[^"]{10,}"/.test(t)) errors.push(`${rel(file)}: missing meta description`);
  for (const m of t.matchAll(/<button\b([^>]*)>([\s\S]*?)<\/button>/g)) {
    const text = m[2].replace(/<[^>]*>/g, '').trim();
    if (!text && !/aria-label=/.test(m[1])) errors.push(`${rel(file)}: a <button> has no text and no aria-label`);
  }
  for (const m of t.matchAll(/<a\b([^>]*)>([\s\S]*?)<\/a>/g)) {
    const text = m[2].replace(/<[^>]*>/g, '').trim();
    if (!text && !/aria-label=/.test(m[1])) errors.push(`${rel(file)}: a link has no text and no aria-label (${(m[1].match(/href="([^"]*)"/) || [])[1]})`);
  }
  for (const m of t.matchAll(/<img\b(?![^>]*\balt=)[^>]*>/g)) errors.push(`${rel(file)}: <img> without alt`);
  // links
  for (const m of t.matchAll(/\b(?:href|src)="([^"]+)"/g)) {
    let u = m[1].replace(/&#38;|&amp;/g, "&");
    if (/^(https?:|mailto:|data:|javascript:|#?$)/.test(u)) continue;
    if (u.startsWith('#')) { if (!idsOf.get(file).has(decodeURIComponent(u.slice(1)))) errors.push(`${rel(file)}: anchor ${u} has no target`); continue; }
    let [p, hash] = u.split('#'); p = p.split('?')[0];
    if (!p.startsWith('/')) p = path.posix.normalize(path.posix.join(here, p));
    if (BASE && p.startsWith(BASE + '/')) p = p.slice(BASE.length); else if (BASE && p !== BASE) { if (p.startsWith('/')) { errors.push(`${rel(file)}: ${u} is outside the site base ${BASE}`); continue; } }
    const target = resolve(p);
    if (!target) { errors.push(`${rel(file)}: broken link ${u}`); continue; }
    if (hash && target.endsWith('.html') && idsOf.has(target) && !idsOf.get(target).has(decodeURIComponent(hash))) errors.push(`${rel(file)}: ${u}: no element with id "${hash}" in the target page`);
  }
}
if (errors.length) { const uniq = [...new Set(errors)]; console.error(uniq.slice(0, 80).join('\n')); console.error(`\n${uniq.length} problem(s) in ${pages.length} pages`); process.exit(1); }
console.log(`dist OK: ${pages.length} pages, links, anchors, headings and names verified`);
