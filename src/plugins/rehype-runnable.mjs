// Marks C code blocks that the build-time validator (scripts/validate-snippets.mjs) proved can be compiled and run in the app.
import fs from 'node:fs'; import crypto from 'node:crypto';

let table = {};
try { table = JSON.parse(fs.readFileSync(new URL('../data/runnable.json', import.meta.url), 'utf8')); } catch { /* validator has not run yet */ }
const hashOf = (code) => crypto.createHash('sha1').update(code.trim()).digest('hex').slice(0, 16);
const text = (n) => (n.type === 'text' ? n.value : (n.children || []).map(text).join(''));

export default function rehypeRunnable() {
  return (tree) => {
    const walk = (node) => {
      for (const child of node.children || []) {
        if (child.type === 'element' && child.tagName === 'pre') {
          const lang = String(child.properties?.dataLanguage || '').toLowerCase();
          if (lang === 'c') {
            const h = hashOf(text(child));
            if (table[h]?.ok) child.properties.dataRunnable = h;
          }
        } else walk(child);
      }
    };
    walk(tree);
  };
}
