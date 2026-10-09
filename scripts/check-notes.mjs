// Validates note front matter and (with --fix) quotes `label:` values so ": " inside labels can't break YAML.
import fs from 'fs'; import path from 'path'; import * as yaml from 'js-yaml';
const fix = process.argv.includes('--fix');
const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
let bad = 0;
for (const f of walk('src/content/notes').filter((f) => f.endsWith('.md') || f.endsWith('.mdx'))) {
  let text = fs.readFileSync(f, 'utf8');
  const m = text.match(/^---\r?\n([\s\S]*?)\r?\n---/);
  if (!m) {
    bad++;
    console.error(`${f}: Missing or malformed front matter delimiters (---)`);
    continue;
  }
  if (fix) {
    const fm = m[1].replace(/^(\s+- label: )(?!")(.*)$/gm, (_, p, v) => `${p}"${v.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`);
    text = text.replace(m[1], () => fm);
    fs.writeFileSync(f, text);
  }
  try {
    yaml.load(m[1]);
  } catch (e) {
    bad++;
    const line = e.mark ? e.mark.line + 1 : '?';
    console.error(`${f}:${line} ${e.reason || e.message}`);
  }
}
if (bad) { console.error(`${bad} file(s) with invalid front matter`); process.exit(1); }
console.log('front matter OK');
