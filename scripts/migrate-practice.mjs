// ONE-OFF migration: turns the 15 "Practice:" notes that embed starter programs into standalone problem files under authoring/practice/.
// Code problems: metadata from authoring/practice/meta.mjs, explanation from the note's Answers. Question problems: statement and answer from the note.
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto'; import * as yaml from 'js-yaml';
const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const NOTES = path.join(ROOT, 'src/content/notes'), OUT = path.join(ROOT, 'authoring/practice');
const { default: meta } = await import(path.join(OUT, 'meta.mjs'));
const problems = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/practice.json'), 'utf8'));
const byHash = new Map(problems.map((p) => [p.hash, p]));
const hashOf = (c) => crypto.createHash('sha1').update(c.trim()).digest('hex').slice(0, 16);
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
const slugify = (s) => s.toLowerCase().replace(/\(.*?\)/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const LEVEL = { beginner: 'Easy', intermediate: 'Medium', advanced: 'Hard', expert: 'Hard' };
const MODDIR = (d) => d.replace('00-start-here', '00-getting-started');
const MODNAME = (m) => (m === 'Start Here' ? 'Getting Started' : m);
const files = walk(NOTES).filter((f) => f.endsWith('.md') && fs.readFileSync(f, 'utf8').includes('details class="starter"')).sort();
const counters = {}; let nCode = 0, nQ = 0; const problems_out = [];

for (const f of files) {
  const text = fs.readFileSync(f, 'utf8');
  const fm = yaml.load(text.match(/^---\n([\s\S]*?)\n---/)[1]);
  const body = text.replace(/^---\n[\s\S]*?\n---\n/, '');
  const ex = body.slice(body.indexOf('\n## Exercises') + 1, body.indexOf('\n## Answers') + 1);
  const an = body.slice(body.indexOf('\n## Answers') + 1, body.indexOf('\n## Key takeaways') + 1);
  const dir = MODDIR(path.basename(path.dirname(f))), track = fm.track, modKey = `${track}/${dir}`;
  // ---- exercises: items "N. **Title.** text" or "**N. Title.** text"
  const lines = ex.split('\n').slice(1);
  const items = []; let cur = null;
  for (const line of lines) {
    const m = line.match(/^(?:(\d+)\. \*\*(.+?)\*\*|\*\*(\d+)\. (.+?)\*\*)\s*(.*)$/);
    if (m) { cur = { n: +(m[1] || m[3]), title: (m[2] || m[4]).replace(/\.$/, '').trim(), rest: [m[5]] }; items.push(cur); }
    else if (cur) cur.rest.push(line);
  }
  for (const it of items) {
    const raw = it.rest.join('\n');
    const dm = raw.match(/<details class="starter">[\s\S]*?```c\n([\s\S]*?)```\s*\n\s*<\/details>/);
    it.starter = dm ? byHash.get(hashOf(dm[1])) : null;
    if (dm) it.statementText = raw.replace(dm[0], '').trim(); else it.statementText = raw.trim();
  }
  // ---- answers: blocks "**N.**" / "**N and M.**" / "**2, 3 and 4.**", or list items "N. text"
  const ans = {};
  const blockHead = /^\*\*([\d][\d, and]*)\.\*\*[ \t]*/;
  const aLines = an.split('\n');
  let start = aLines.findIndex((l) => /^<summary>/.test(l)); start = start < 0 ? 0 : start + 1;
  const blocks = []; let b = null;
  for (let i = start; i < aLines.length; i++) {
    const l = aLines[i];
    if (/^<\/details>\s*$/.test(l) && i > aLines.length - 6) break;
    const h = l.match(blockHead);
    if (h) { b = { nums: (h[1].match(/\d+/g) || []).map(Number), lines: [l.replace(blockHead, '')] }; blocks.push(b); }
    else if (b) b.lines.push(l);
  }
  if (blocks.length) for (const bl of blocks) { const md = bl.lines.join('\n').replace(/\n+$/, '').trim(); for (const n of bl.nums) ans[n] = (ans[n] ? ans[n] + '\n\n' : '') + md; }
  else { // list form
    let cn = null;
    for (let i = start; i < aLines.length; i++) {
      const l = aLines[i], m = l.match(/^(\d+)\. (.*)$/);
      if (m) { cn = +m[1]; ans[cn] = m[2]; } else if (cn && l.trim() && !/^The program below|^```|^<\/details>/.test(l)) ans[cn] += '\n' + l; else if (/^The program below|^```/.test(l)) cn = null;
    }
  }
  if (!blocks.length && !Object.keys(ans).length) {      // dsa warm-ups: one paragraph of costs, one of key ideas, then a program for all
    const costs = (an.match(/^Costs: (.*)$/m) || [, ''])[1], keys = (an.match(/^Key ideas: (.*)$/m) || [, ''])[1];
    for (const m of costs.matchAll(/(\d+): (.*?)(?=\s\d+: |$)/g)) ans[+m[1]] = `**Cost.** ${m[2].trim()}`;
    for (const m of keys.matchAll(/\((\d+)\) (.*?)(?=; \(\d+\) |\.$|$)/g)) ans[+m[1]] = (ans[+m[1]] ? ans[+m[1]] + '\n\n' : '') + `**Key idea.** ${m[2].trim().replace(/\.$/, '')}.`;
  }
  for (const it of items) {
    const order = (counters[modKey] = (counters[modKey] || 0) + 1);
    const explanation = (ans[it.n] || '').trim();
    if (!explanation) console.warn(`NO ANSWER: ${path.relative(NOTES, f)} exercise ${it.n}`);
    let head, statement, slug;
    if (it.starter) {
      const m = meta[it.starter.rel];
      head = { kind: 'code', title: m.title, module: MODNAME(fm.module), difficulty: m.difficulty, tags: m.tags, starter: it.starter.rel, examples: m.examples, constraints: m.constraints, hints: m.hints };
      statement = m.statement; slug = m.slug; nCode++;
    } else {
      head = { kind: 'question', title: it.title, module: MODNAME(fm.module), difficulty: LEVEL[fm.level] };
      statement = it.statementText; slug = slugify(it.title); nQ++;
    }
    const fileName = `${String(order).padStart(2, '0')}-${slug}.md`;
    const outPath = path.join(OUT, track, dir, fileName);
    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    const dropProgram = (s) => s.replace(/```c\n(?:(?!```)[\s\S])*?\bmain\s*\([\s\S]*?```\n*(?:```text\n\$ [\s\S]*?```\n*)?/g, '').replace(/\n{3,}/g, '\n\n').trim();
    const clean = (s) => s.replace(/<\/?details[^>]*>|<summary>.*?<\/summary>/g, '').replace(/\n{3,}/g, '\n\n').trim();
    fs.writeFileSync(outPath, `---\n${yaml.dump(head, { lineWidth: 120, noRefs: true })}---\n\n${clean(statement)}\n${explanation ? `\n## Explanation\n\n${clean(it.starter ? dropProgram(explanation) : explanation)}\n` : ''}`);
    problems_out.push(outPath);
  }
}
console.log(`${nCode} code problems, ${nQ} questions -> ${path.relative(ROOT, OUT)}`);
