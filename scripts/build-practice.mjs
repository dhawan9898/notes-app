// Builds src/data/practice.json: one record per practice problem, for the /practice/ pages.
// Sources, all under authoring/:
//   examples/<track>/starters/<topic>/NAME.c           the starter program (stub + tests); the region between the "Your code" markers is what the reader edits
//   examples/<track>/starters/<topic>/NAME.solution.c  the reference solution for that region
//   templates/<track>/<module>/<note>.md                the practice note that {{include}}s the starter (the problem links back to it)
//   practice/meta.mjs                                  per problem (key = path of the starter below authoring/examples): slug, title, statement, difficulty, tags, examples, constraints, hints
// Usage: node scripts/build-practice.mjs [--check]
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const EX = path.join(ROOT, 'authoring/examples'), TPL = path.join(ROOT, 'authoring/templates');
const OUT = path.join(ROOT, 'src/data/practice.json');
const BEGIN = /^.*\/\* ===== Your code.*$/m, END = /^.*\/\* ===== End of your code ===== \*\/.*$/m;
const hashOf = (code) => crypto.createHash('sha1').update(code.trim()).digest('hex').slice(0, 16);
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
const { default: meta } = await import(path.join(ROOT, 'authoring/practice/meta.mjs'));
let runnable = {}; try { runnable = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/runnable.json'), 'utf8')); } catch { /* validator has not run */ }

const problems = [], errors = [];
const noteSlug = (rel) => rel.replace(/\.md$/, '').split('/').map((s) => s.replace(/^\d+-/, '')).join('/');

for (const file of walk(TPL).filter((f) => f.endsWith('.md')).sort()) {
  const lines = fs.readFileSync(file, 'utf8').split('\n');
  const relNote = path.relative(TPL, file);
  const title = (lines.join('\n').match(/^title:\s*"?(.*?)"?\s*$/m) || [, relNote])[1];
  lines.forEach((line, i) => {
    const inc = line.match(/^\{\{include (\S+\/starters\/\S+\.c)\}\}$/);
    if (!inc) return;
    const rel = inc[1], m = meta[rel];
    if (!m) { errors.push(`no entry in authoring/practice/meta.mjs for ${rel}`); return; }
    const full = fs.readFileSync(path.join(EX, rel), 'utf8');
    const a = full.search(BEGIN), b = full.search(END);
    if (a < 0 || b < 0) { errors.push(`${rel}: missing markers`); return; }
    const aEnd = full.indexOf('\n', a) + 1;
    const solFile = path.join(EX, rel.replace(/\.c$/, '.solution.c'));
    if (!fs.existsSync(solFile)) { errors.push(`${rel}: no solution file`); return; }
    const [track, , topic] = rel.split('/');
    const h = hashOf(full);
    problems.push({
      id: `${track}/${m.slug}`, track, topic, slug: m.slug, rel,
      title: m.title, statement: m.statement,
      difficulty: m.difficulty, tags: m.tags || [], examples: m.examples || [], constraints: m.constraints || [], hints: m.hints || [],
      prefix: full.slice(0, aEnd), starter: full.slice(aEnd, b).replace(/\n+$/, '\n'), suffix: full.slice(b),
      solution: fs.readFileSync(solFile, 'utf8').replace(/\n*$/, '\n'),
      note: { title, path: noteSlug(relNote), file: relNote }, hash: h, runnable: !!runnable[h]?.ok,
    });
  });
}
for (const k of Object.keys(meta)) if (!problems.some((p) => p.rel === k)) errors.push(`meta.json has an entry nothing includes: ${k}`);
const ids = new Set(); for (const p of problems) { if (ids.has(p.id)) errors.push(`duplicate problem id ${p.id}`); ids.add(p.id); }
for (const p of problems) {
  if (!p.title || !p.statement) errors.push(`${p.id}: needs a title and a statement`);
  if (!['Easy', 'Medium', 'Hard'].includes(p.difficulty)) errors.push(`${p.id}: difficulty must be Easy, Medium or Hard`);
  if (!p.examples.length) errors.push(`${p.id}: needs at least one example`);
  if (!p.constraints.length) errors.push(`${p.id}: needs constraints`);
  if (!p.hints.length) errors.push(`${p.id}: needs a hint`);
}
if (errors.length) { console.error(errors.join('\n')); process.exit(1); }
const order = { c: 0, dsa: 1 };
problems.sort((x, y) => (order[x.track] ?? 9) - (order[y.track] ?? 9));   // stable: template order within a track
fs.writeFileSync(OUT, JSON.stringify(problems, null, 1) + '\n');
console.log(`${problems.length} problems -> ${path.relative(ROOT, OUT)} (${problems.filter((p) => !p.runnable).length} not runnable in the app)`);
