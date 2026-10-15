// Generates src/data/generated/{practice,search-index}.json from the authored sources. Run by `npm run data` (and by dev/build/test).
// Nothing here is committed: it is derived from src/content, authoring/practice, authoring/examples and src/data/*.json.
import fs from 'node:fs'; import path from 'node:path';
import { createMarkdownProcessor } from '@astrojs/markdown-remark';
import { P, TRACKS, ROOT, rel, loadLessons, loadLabs, loadProblemFiles, modulesOf, loadPaths, loadGlossary, readingMinutes, stripNum } from './lib/content.mjs';
import { compactHtml } from '../src/plugins/shiki-compact.mjs';

const md = await createMarkdownProcessor({ shikiConfig: { themes: { light: 'github-light', dark: 'github-dark' } } });
const html = async (s) => compactHtml((await md.render(s)).code);
const inline = async (s) => (await html(s)).replace(/^<p>/, '').replace(/<\/p>\s*$/, '');

const lessons = loadLessons(), labs = loadLabs(), files = loadProblemFiles();
const modules = modulesOf(lessons);
const modOrder = new Map(modules.map((m) => [`${m.track}|${m.title}`, m.order]));
const BEGIN = /^.*\/\* ===== Your code.*$/m, END = /^.*\/\* ===== End of your code ===== \*\/.*$/m;

/* ---------------------------------------------------------------- problems */
const rows = files.map((f) => ({ f, key: [TRACKS.indexOf(f.track), modOrder.get(`${f.track}|${f.data.module}`), f.moduleDir, f.name] }))
  .sort((a, b) => a.key[0] - b.key[0] || a.key[1] - b.key[1] || a.key[3].localeCompare(b.key[3]));
const problems = [];
for (const [i, { f }] of rows.entries()) {
  const d = f.data, rec = {
    id: `${f.track}/${f.slug}`, kind: d.kind, track: f.track, module: d.module, moduleOrder: modOrder.get(`${f.track}|${d.module}`), seq: i + 1,
    title: d.title, slug: f.slug, difficulty: d.difficulty, tags: d.tags ?? [],
    statement: await html(f.statement), explanation: f.explanation ? await html(f.explanation) : '',
  };
  if (d.kind === 'code') {
    const text = fs.readFileSync(path.join(P.examples, d.starter), 'utf8'), a = text.search(BEGIN), b = text.search(END), aEnd = text.indexOf('\n', a) + 1;
    rec.examples = d.examples.map((e) => ({ input: String(e.input), output: String(e.output), why: e.why ? e.why : undefined }));
    rec.constraints = await Promise.all(d.constraints.map(inline));
    rec.hints = await Promise.all(d.hints.map(inline));
    rec.whyHtml = await Promise.all(d.examples.map((e) => (e.why ? inline(e.why) : '')));
    rec.code = {
      starterFile: d.starter, prefix: text.slice(0, aEnd), starter: text.slice(aEnd, b).replace(/\n+$/, '\n'), suffix: text.slice(b),
      solution: fs.readFileSync(path.join(P.examples, d.starter.replace(/\.c$/, '.solution.c')), 'utf8').replace(/\n*$/, '\n'),
    };
  }
  problems.push(rec);
}

/* ---------------------------------------------------------------- search index (compact keys; paths are relative to the site base) */
const q = [];
for (const l of lessons) q.push({ k: 'l', t: l.data.title, u: `/${l.track}/${l.moduleSlug}/${l.slug}/`, s: l.data.summary, m: l.data.module, r: l.track, h: [...l.body.replace(/```[\s\S]*?```/g, '').matchAll(/^## (.+)$/gm)].map((m) => m[1].replace(/[`*]/g, '')) });
for (const l of labs) q.push({ k: 'b', t: l.data.title, u: `/practice/labs/${l.track}/${l.slug}/`, s: l.data.summary, m: l.data.module, r: l.track, h: [...l.body.replace(/```[\s\S]*?```/g, '').matchAll(/^## (.+)$/gm)].map((m) => m[1].replace(/[`*]/g, '')) });
for (const p of problems) q.push({ k: 'p', t: p.title, u: `/practice/${p.track}/${p.slug}/`, s: p.tags.join(', '), m: p.module, r: p.track, d: p.difficulty });
for (const g of loadGlossary()) q.push({ k: 'g', t: g.term, u: `/glossary/#${g.term.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')}`, s: g.def, r: (g.track || '').toLowerCase() });
for (const pa of loadPaths()) q.push({ k: 'a', t: pa.title, u: `/paths/${pa.id}/`, s: pa.summary });

fs.mkdirSync(P.generated, { recursive: true });
fs.writeFileSync(path.join(P.generated, 'practice.json'), JSON.stringify(problems));
fs.writeFileSync(path.join(P.generated, 'search-index.json'), JSON.stringify(q));
const kb = (n) => (n / 1024).toFixed(0) + ' kB';
console.log(`data: ${problems.length} problems (${kb(fs.statSync(path.join(P.generated, 'practice.json')).size)}), search index ${q.length} entries (${kb(fs.statSync(path.join(P.generated, 'search-index.json')).size)})`);
