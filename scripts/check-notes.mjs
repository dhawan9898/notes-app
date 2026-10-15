// Validates note front matter and (with --fix) quotes `label:` values so ": " inside labels can't break YAML.
import fs from 'fs'; import path from 'path'; import * as yaml from 'js-yaml';
const fix = process.argv.includes('--fix');
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
let bad = 0;
const notes = new Map();                          // title -> { f, track, key } for the learning-order check below
for (const f of walk('src/content/notes').filter((f) => f.endsWith('.md') || f.endsWith('.mdx'))) {
  let text = fs.readFileSync(f, 'utf8');
  const m = text.match(/^---\r?\n([\s\S]*?)\r?\n---/);
  if (!m) {
    bad++;
    console.error(`${f}: Missing or malformed front matter delimiters (---)`);
    continue;
  }
  let front = m[1];
  if (fix) {
    const fm = m[1].replace(/^(\s+- label: )(?!")(.*)$/gm, (_, p, v) => `${p}"${v.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`);
    text = text.replace(m[1], () => fm);
    fs.writeFileSync(f, text);
    front = fm;                                  // validate the repaired text, not the original
  }
  try {
    const data = yaml.load(front);
    if (data && data.title) notes.set(data.title, { f, data });
    const pre = data && data.prerequisites;
    if (pre !== undefined && !(Array.isArray(pre) && pre.every((x) => typeof x === 'string'))) {
      bad++;
      console.error(`${f}: prerequisites must be a list of strings (quote entries that contain a colon)`);
    }
  } catch (e) {
    bad++;
    const line = e.mark ? e.mark.line + 1 : '?';
    console.error(`${f}:${line} ${e.reason || e.message}`);
  }
}
// Learning order: a prerequisite that is a note of the same track must come earlier in that track's order.
const key = (d) => d.moduleOrder * 1000 + d.order;
for (const [title, { f, data }] of notes) {
  // Every note above beginner level must be linked into the reading graph (drives "Before you read this", "Continue with" and the roadmap).
  if (data.level !== 'beginner' && !(data.prerequisites ?? []).some((p) => notes.has(p))) {
    bad++;
    console.error(`${f}: a ${data.level} note needs at least one prerequisite that is the exact title of another note`);
  }
  for (const p of data.prerequisites ?? []) {
    const q = notes.get(p);
    if (q && q.data.track === data.track && key(q.data) > key(data)) {
      bad++;
      console.error(`${f}: prerequisite "${p}" comes later in the ${data.track} track; reorder the notes or drop the prerequisite`);
    }
  }
}
if (bad) { console.error(`${bad} file(s) with invalid front matter`); process.exit(1); }
console.log('front matter OK');
