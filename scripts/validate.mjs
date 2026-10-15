// The content gate. Fails (exit 1) on any inconsistency in lessons, labs, problems, paths, glossary and aliases.
// Usage: node scripts/validate.mjs [--fix]      (--fix only re-quotes `label:` values in front matter, the one mistake that breaks YAML silently)
import fs from 'node:fs'; import path from 'node:path';
import { ROOT, P, TRACKS, LEVELS, walk, rel, parseMd, loadLessons, loadLabs, loadProblemFiles, modulesOf, loadPaths, loadGlossary, loadJson } from './lib/content.mjs';

const fix = process.argv.includes('--fix');
const errors = [];
const err = (where, msg) => errors.push(`${typeof where === 'string' ? where : rel(where)}: ${msg}`);

if (fix) for (const f of [...walk(P.notes), ...walk(P.labs)].filter((f) => f.endsWith('.md'))) {
  const t = fs.readFileSync(f, 'utf8'), m = t.match(/^---\r?\n([\s\S]*?)\r?\n---/);
  if (!m) continue;
  const fixed = m[1].replace(/^(\s+- label: )(?!")(.*)$/gm, (_, p, v) => `${p}"${v.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`);
  if (fixed !== m[1]) fs.writeFileSync(f, t.replace(m[1], () => fixed));
}

let lessons, labs, problems;
try { lessons = loadLessons(); labs = loadLabs(); problems = loadProblemFiles(); } catch (e) { console.error(e.message); process.exit(1); }

const str = (v) => typeof v === 'string' && v.trim().length > 0;
const strList = (v) => Array.isArray(v) && v.every((x) => typeof x === 'string' && x.trim());

/* ---------------------------------------------------------------- lessons and labs */
function checkCommon(item, label) {
  const d = item.data, f = item.file;
  if (!TRACKS.includes(item.track)) err(f, `unknown track directory "${item.track}"`);
  if (d.track !== item.track) err(f, `front matter track "${d.track}" differs from the directory "${item.track}"`);
  if (!str(d.title)) err(f, 'title is required');
  if (!str(d.module)) err(f, 'module is required');
  if (typeof d.moduleOrder !== 'number') err(f, 'moduleOrder must be a number');
  if (!Number.isInteger(d.order)) err(f, 'order must be an integer');
  if (!LEVELS.includes(d.level)) err(f, `level must be one of ${LEVELS.join(', ')}`);
  if (!str(d.summary)) err(f, 'summary is required'); else if (d.summary.length > 400) err(f, `summary is ${d.summary.length} characters; keep it under 400`);
  if (d.prerequisites !== undefined && !strList(d.prerequisites)) err(f, 'prerequisites must be a list of strings (quote entries that contain a colon)');
  for (const s of d.sources ?? []) { if (!s || !str(s.label)) err(f, 'every source needs a label'); else if (s.url !== undefined && !/^https?:\/\//.test(s.url)) err(f, `source "${s.label}" has a url that is not http(s)`); }
  if (/^#\s/m.test(item.body.replace(/```[\s\S]*?```/g, ''))) err(f, 'the body must not contain a level-1 heading (the title is the h1)');
  if ((item.body.match(/```/g) || []).length % 2) err(f, 'unbalanced code fence');
  if (!/^## Key takeaways\s*$/m.test(item.body)) err(f, 'missing the "## Key takeaways" section');
  else {
    const prose = item.body.replace(/```[\s\S]*?```/g, (m) => m.replace(/^#/gm, ' #'));
    const heads = [...prose.matchAll(/^(#{2,4}) (.+)$/gm)].map((m) => ({ d: m[1].length, t: m[2].trim() }));
    const h2 = heads.filter((h) => h.d === 2);
    if (h2.at(-1).t !== 'Key takeaways') err(f, '"## Key takeaways" must be the last section (sources are added by the page)');
    let prev = 1; for (const h of heads) { if (h.d > prev + 1) { err(f, `heading "${h.t}" skips a level`); break; } prev = h.d; }
    const seen = new Set(); for (const h of h2) { if (seen.has(h.t)) { err(f, `duplicate section "${h.t}"`); break; } seen.add(h.t); }
    const n = ((prose.split(/^## Key takeaways\s*$/m)[1] || '').match(/^(?:[-*] |\d+\. )/gm) || []).length;
    if (n < 3 || n > 10) err(f, `Key takeaways has ${n} items; write 3 to 10`);
  }
  if (d.title && d.title.length > 100) err(f, `title is ${d.title.length} characters; keep it under 100`);
  if (/^title:\s*"?(Practice|Lab):/m.test(item.text ?? '')) err(f, 'do not prefix titles with "Practice:" or "Lab:"');
}

const titles = new Map();
for (const l of [...lessons, ...labs]) {
  checkCommon(l, l.kind);
  if (titles.has(l.data.title)) err(l.file, `duplicate title "${l.data.title}" (also ${rel(titles.get(l.data.title).file)})`);
  titles.set(l.data.title, l);
}
for (const l of lessons) {
  const sec = l.body.replace(/```[\s\S]*?```/g, '');
  if (/^## (Exercises|Answers|Practice)\s*$/m.test(sec)) err(l.file, 'lessons contain no exercises or answers: put them in authoring/practice (problems) or src/content/labs');
  if (l.data.level !== 'beginner' && !(l.data.prerequisites ?? []).some((p) => titles.has(p))) err(l.file, `a ${l.data.level} lesson needs at least one prerequisite that is the exact title of another lesson`);
}
for (const l of labs) {
  if (typeof l.data.minutes !== 'number' || l.data.minutes < 5) err(l.file, 'minutes (estimated time) is required for labs');
  if (!['browser', 'machine'].includes(l.data.env)) err(l.file, 'env must be "browser" (runs in the in-browser Linux terminal) or "machine" (needs the reader\'s own Linux machine)');
  const term = l.data.terminal ?? [];
  if (l.data.env === 'browser' && !term.length) err(l.file, 'a browser lab needs terminal: [commands to run in the in-browser Linux]');
  if (l.data.env === 'machine' && term.length) err(l.file, 'a machine lab must not list terminal commands');
  if (!strList(term)) err(l.file, 'terminal must be a list of strings');
  for (const cmd of term.filter((c) => !/chmod \+x/.test(c))) for (const m of cmd.matchAll(/(?:^|[;&|]\s*)\.\/([A-Za-z0-9_.-]+)/g)) if (!fs.existsSync(path.join(P.examples, 'vm', m[1])) && !fs.existsSync(path.join(P.examples, 'vm', m[1] + '.c')) && !fs.existsSync(path.join(P.examples, 'vm', m[1] + '.sh'))) err(l.file, `terminal command "${cmd}" runs ./${m[1]}, which is not in authoring/examples/vm`);
  if (/```python|^\$ python3? |^python3? /m.test(l.body) && l.track !== 'c') err(l.file, 'labs use C (or shell) programs, not Python');
  if (!(l.data.prerequisites ?? []).some((p) => titles.has(p) && titles.get(p).kind === 'lesson')) err(l.file, 'a lab needs at least one prerequisite that is a lesson');
}

// modules: one title per (track, moduleOrder), order and file numbering agree
const modules = modulesOf(lessons);
const byOrder = new Map();
for (const l of lessons) {
  const k = `${l.track}|${l.data.moduleOrder}`;
  if (byOrder.has(k) && byOrder.get(k) !== l.data.module) err(l.file, `moduleOrder ${l.data.moduleOrder} is used by "${byOrder.get(k)}" and "${l.data.module}" in track ${l.track}`);
  byOrder.set(k, l.data.module);
}
for (const m of modules) {
  const seen = new Set(), dirs = new Set(m.lessons.map((l) => l.moduleDir));
  if (dirs.size > 1) err(m.lessons[0].file, `module "${m.title}" is spread over directories ${[...dirs].join(', ')}`);
  for (const l of m.lessons) {
    if (seen.has(l.data.order)) err(l.file, `order ${l.data.order} is used twice in module "${m.title}"`);
    seen.add(l.data.order);
    const n = parseInt(l.name, 10);
    if (n !== l.data.order) err(l.file, `file number ${n} differs from front matter order ${l.data.order}`);
  }
}
const moduleKeys = new Set(modules.map((m) => `${m.track}|${m.title}`));
for (const l of labs) if (!moduleKeys.has(`${l.track}|${l.data.module}`)) err(l.file, `module "${l.data.module}" has no lessons in track ${l.track}`);
const labSlugs = new Set();
for (const l of labs) { const k = `${l.track}/${l.slug}`; if (labSlugs.has(k)) err(l.file, `duplicate lab slug ${k}`); labSlugs.add(k); }

// prerequisites: must exist; a prerequisite lesson of the same track must come earlier in that track
const key = (d) => d.moduleOrder * 1000 + d.order;
for (const l of [...lessons, ...labs]) for (const p of l.data.prerequisites ?? []) {
  const q = titles.get(p);
  if (!q) { if (/^[A-Z].*\b(Notes?|Lesson)\b/.test(p)) err(l.file, `prerequisite "${p}" is not the title of a lesson`); continue; }
  if (q.kind === 'lab') err(l.file, `prerequisite "${p}" is a lab; prerequisites must be lessons`);
  else if (l.kind === 'lesson' && q.track === l.track && key(q.data) >= key(l.data)) err(l.file, `prerequisite "${p}" does not come earlier in the ${l.track} track`);
}
// prerequisites that are plain phrases are allowed only if they do not look like a lesson title: keep the list explicit
for (const l of lessons) for (const p of l.data.prerequisites ?? []) if (!titles.has(p)) err(l.file, `prerequisite "${p}" matches no lesson title (fix the spelling or remove it)`);

/* ---------------------------------------------------------------- problems */
const probKeys = new Set(), starterRefs = new Set();
const DIFF = ['Easy', 'Medium', 'Hard'];
for (const p of problems) {
  const d = p.data, f = p.file;
  if (!TRACKS.includes(p.track)) err(f, `unknown track "${p.track}"`);
  if (!['code', 'question'].includes(d.kind)) err(f, 'kind must be "code" or "question"');
  if (!str(d.title)) err(f, 'title is required');
  if (!DIFF.includes(d.difficulty)) err(f, `difficulty must be one of ${DIFF.join(', ')}`);
  if (!moduleKeys.has(`${p.track}|${d.module}`)) err(f, `module "${d.module}" has no lessons in track ${p.track}`);
  if (d.tags !== undefined && !strList(d.tags)) err(f, 'tags must be a list of strings');
  if (!p.statement) err(f, 'the statement (text before "## Explanation") is empty');
  const k = `${p.track}/${p.slug}`; if (probKeys.has(k)) err(f, `duplicate problem slug ${k}`); probKeys.add(k);
  if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(p.slug)) err(f, `slug "${p.slug}" must be lower-case words joined by hyphens`);
  if (d.kind === 'question' && !p.explanation) err(f, 'a question needs an "## Explanation" section with the answer');
  if (d.kind === 'code') {
    if (!str(d.starter)) { err(f, 'a code problem needs starter: <path under authoring/examples>'); continue; }
    starterRefs.add(d.starter);
    const sp = path.join(P.examples, d.starter), sol = sp.replace(/\.c$/, '.solution.c');
    if (!fs.existsSync(sp)) err(f, `starter ${d.starter} does not exist`);
    else { const t = fs.readFileSync(sp, 'utf8'); if (!/\/\* ===== Your code/.test(t) || !/\/\* ===== End of your code ===== \*\//.test(t)) err(f, `${d.starter} lacks the "Your code" / "End of your code" markers`); }
    if (!fs.existsSync(sol)) err(f, `reference solution ${rel(sol)} does not exist`);
    if (!Array.isArray(d.examples) || !d.examples.length || d.examples.some((e) => !str(e.input) || e.output === undefined)) err(f, 'needs examples: [{input, output, why?}]');
    if (!strList(d.constraints) || !d.constraints.length) err(f, 'needs constraints: [..]');
    if (!strList(d.hints) || !d.hints.length) err(f, 'needs hints: [..]');
  }
}
for (const sp of walk(P.examples).filter((f) => /\/starters\/.*\.c$/.test(f) && !/\.(solution|wrong-[\w-]+)\.c$/.test(f))) {
  const r = path.relative(P.examples, sp);
  if (!starterRefs.has(r)) err(sp, 'starter program has no problem file in authoring/practice (add one or delete the starter)');
}

/* ---------------------------------------------------------------- paths, glossary, aliases */
for (const p of loadPaths()) {
  if (!str(p.id) || !str(p.title) || !str(p.summary) || !str(p.audience)) err('src/data/paths.json', `path ${p.id}: id, title, audience and summary are required`);
  if (!Array.isArray(p.modules) || !p.modules.length) err('src/data/paths.json', `path ${p.id}: needs modules`);
  for (const m of p.modules ?? []) if (!moduleKeys.has(`${m.track}|${m.module}`)) err('src/data/paths.json', `path ${p.id}: no module "${m.module}" in track ${m.track}`);
}
const terms = new Set();
for (const g of loadGlossary()) {
  if (!str(g.term) || !str(g.def)) err('src/data/glossary.json', `entry needs term and def: ${JSON.stringify(g).slice(0, 60)}`);
  if (terms.has(g.term.toLowerCase())) err('src/data/glossary.json', `duplicate term "${g.term}"`); terms.add(g.term.toLowerCase());
  if (g.see && !(titles.has(g.see) && titles.get(g.see).kind === 'lesson')) err('src/data/glossary.json', `"${g.term}": see "${g.see}" is not a lesson title`);
}
const aliases = loadJson('id-aliases.json');
const lessonIds = new Set(lessons.map((l) => l.id)), labIds = new Set(labs.map((l) => `lab:${l.id}`));
for (const [from, to] of Object.entries(aliases)) if (!lessonIds.has(to) && !labIds.has(to)) err('src/data/id-aliases.json', `${from} -> ${to}: target does not exist`);

if (errors.length) { console.error(errors.join('\n')); console.error(`\n${errors.length} problem(s)`); process.exit(1); }
console.log(`content OK: ${lessons.length} lessons in ${modules.length} modules, ${labs.length} labs, ${problems.length} problems (${problems.filter((p) => p.data.kind === 'code').length} code), ${loadPaths().length} paths, ${terms.size} glossary terms`);
