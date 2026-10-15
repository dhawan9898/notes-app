// ONE-OFF restructuring (see docs/design.md section 9):
//  - lab-style "Practice:" notes move to src/content/labs (and, for C, their templates to authoring/templates/c/labs)
//  - exercise notes that were migrated to authoring/practice are deleted, with their templates
//  - the "Start Here: How to use" notes are deleted; module "Start Here" becomes "Getting Started" (dir 00-getting-started), files renumbered
//  - old note ids are recorded in src/data/id-aliases.json so saved progress keeps working
import fs from 'node:fs'; import path from 'node:path'; import { execFileSync } from 'node:child_process';
const ROOT = path.resolve(new URL('..', import.meta.url).pathname);
const NOTES = 'src/content/notes', LABS = 'src/content/labs', TPL = 'authoring/templates';
const git = (...a) => execFileSync('git', a, { cwd: ROOT, stdio: 'pipe' });
const walk = (d) => (fs.existsSync(d) ? fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)])) : []);
const read = (p) => fs.readFileSync(path.join(ROOT, p), 'utf8');
const write = (p, t) => { fs.mkdirSync(path.dirname(path.join(ROOT, p)), { recursive: true }); fs.writeFileSync(path.join(ROOT, p), t); };
const mv = (a, b) => { fs.mkdirSync(path.dirname(path.join(ROOT, b)), { recursive: true }); git('mv', a, b); };
const rm = (p) => { if (fs.existsSync(path.join(ROOT, p))) git('rm', '-q', '-f', p); };
const aliases = {};
const idOf = (p) => p.replace(/^src\/content\/notes\//, '').replace(/\.md$/, '');
const exists = (p) => fs.existsSync(path.join(ROOT, p));

const all = walk(path.join(ROOT, NOTES)).map((f) => path.relative(ROOT, f)).filter((f) => f.endsWith('.md')).sort();
const isMigrated = (t) => t.includes('details class="starter"');
const isIntro = (f) => /\/00-start-here\/01-start-here-/.test(f);
const isPractice = (f, t) => /^title: "?Practice/m.test(t);
const labsPerModule = {};

for (const f of all) {
  const text = read(f);
  const rel = f.replace(`${NOTES}/`, '');                  // c/00-start-here/05-practice-c-exercises.md
  const [track, dir, file] = rel.split('/');
  const tplPath = `${TPL}/${rel}`;
  if (isIntro(f) || (isPractice(f, text) && isMigrated(text))) { rm(f); rm(tplPath); continue; }
  if (isPractice(f, text)) {                               // a lab
    const modDir = dir.replace('00-start-here', '00-getting-started');
    const order = (labsPerModule[`${track}/${modDir}`] = (labsPerModule[`${track}/${modDir}`] || 0) + 1);
    const slug = file.replace(/^\d+-/, '').replace(/^practice-/, '');
    const edit = (t) => t
      .replace(/^title: "?Practice: (.*?)"?$/m, (_, x) => `title: ${/[:#]/.test(x) ? `"${x.replace(/"/g, '\\"')}"` : x}`)
      .replace(/^module: Start Here$/m, 'module: Getting Started')
      .replace(/^order: \d+$/m, `order: ${order}`)
      .replace(/^(level: .*)$/m, `$1\nminutes: ${Math.max(10, Math.round((15 + t.split(/\s+/).length / 60) / 5) * 5)}`);
    const newName = `${String(order).padStart(2, '0')}-${slug}`;
    const dst = `${LABS}/${track}/${modDir}/${newName}`;
    mv(f, dst); write(dst, edit(read(dst)));
    if (exists(tplPath)) { const tdst = `${TPL}/${track}/labs/${modDir}/${newName}`; mv(tplPath, tdst); write(tdst, edit(read(tdst))); }
    aliases[idOf(f)] = `lab:${track}/${modDir}/${newName.replace(/\.md$/, '')}`;
    continue;
  }
  if (dir === '00-start-here') {                           // a lesson of the renamed module: renumber later
    continue;
  }
}
// ---- rename 00-start-here -> 00-getting-started and renumber
for (const track of ['c', 'dsa', 'linux', 'networking']) {
  const left = walk(path.join(ROOT, NOTES, track, '00-start-here')).map((f) => path.relative(ROOT, f)).filter((f) => f.endsWith('.md')).sort();
  left.forEach((f, i) => {
    const n = String(i + 1).padStart(2, '0'), base = path.basename(f).replace(/^\d+-/, '');
    const dst = `${NOTES}/${track}/00-getting-started/${n}-${base}`;
    const edit = (t) => t.replace(/^module: Start Here$/m, 'module: Getting Started').replace(/^order: \d+$/m, `order: ${i + 1}`);
    mv(f, dst); write(dst, edit(read(dst)));
    const tp = f.replace(NOTES, TPL);
    if (exists(tp)) { const td = dst.replace(NOTES, TPL); mv(tp, td); write(td, edit(read(td))); }
    aliases[idOf(f)] = idOf(dst);
  });
}
fs.writeFileSync(path.join(ROOT, 'src/data/id-aliases.json'), JSON.stringify(aliases, null, 1) + '\n');
console.log('labs per module:', JSON.stringify(labsPerModule));
console.log(Object.keys(aliases).length, 'ids aliased');
