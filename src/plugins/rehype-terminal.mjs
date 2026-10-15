// Marks command blocks ("$ command" lines) that scripts/verify-terminal.mjs proved work in the in-browser Linux, so lesson.ts can offer
// "Run in terminal" for exactly those blocks. The table is keyed by the hash of the block text, like src/data/runnable.json.
import fs from 'node:fs'; import crypto from 'node:crypto';

let table = {};
try { table = JSON.parse(fs.readFileSync(new URL('../data/terminal-blocks.json', import.meta.url), 'utf8')); } catch { /* the verifier has not run yet */ }
const hashOf = (code) => crypto.createHash('sha1').update(code.trim()).digest('hex').slice(0, 16);
const text = (n) => (n.type === 'text' ? n.value : (n.children || []).map(text).join(''));

export default function rehypeTerminal() {
  return (tree) => {
    const walk = (node) => {
      for (const child of node.children || []) {
        if (child.type === 'element' && child.tagName === 'pre') { if (table[hashOf(text(child))]?.ok) child.properties.dataTerminal = '1'; }
        else walk(child);
      }
    };
    walk(tree);
  };
}
