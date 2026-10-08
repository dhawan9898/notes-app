// Validates note front matter and (with --fix) quotes `label:` values so ": " inside labels can't break YAML.
import fs from 'fs'; import path from 'path'; import * as yaml from 'js-yaml';
const fix = process.argv.includes('--fix');
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
let bad = 0;
for (const f of walk('src/content/notes').filter((f) => f.endsWith('.md'))) {
  let text = fs.readFileSync(f, 'utf8');
  if (fix) {
    const m = text.match(/^---\n([\s\S]*?)\n---/);
    const fm = m[1].replace(/^(\s+- label: )(?!")(.*)$/gm, (_, p, v) => `${p}"${v.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`);
    text = text.replace(m[1], () => fm); fs.writeFileSync(f, text);
  }
  try { yaml.load(text.match(/^---\n([\s\S]*?)\n---/)[1]); }
  catch (e) { bad++; console.error(`${f}:${e.mark.line + 1} ${e.reason}`); }
}
if (bad) { console.error(`${bad} file(s) with invalid front matter`); process.exit(1); }
console.log('front matter OK');
