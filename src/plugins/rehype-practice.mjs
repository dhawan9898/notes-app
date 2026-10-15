// In the practice notes, replaces each collapsible "Starter program with tests" block with a card that opens the problem on its own page
// (/practice/<track>/<slug>/). The block is matched by the hash of its code, which scripts/build-practice.mjs stores in src/data/practice.json.
import fs from 'node:fs'; import crypto from 'node:crypto';

let problems = [];
try { problems = JSON.parse(fs.readFileSync(new URL('../data/practice.json', import.meta.url), 'utf8')); } catch { /* generator has not run yet */ }
const byHash = new Map(problems.map((p) => [p.hash, p]));
const hashOf = (code) => crypto.createHash('sha1').update(code.trim()).digest('hex').slice(0, 16);
const text = (n) => (n.type === 'text' ? n.value : (n.children || []).map(text).join(''));
const el = (tagName, properties, children = []) => ({ type: 'element', tagName, properties, children });
const t = (value) => ({ type: 'text', value });
const find = (node, pred) => { for (const c of node.children || []) { if (c.type === 'element' && pred(c)) return c; const r = find(c, pred); if (r) return r; } return null; };
const base = (process.env.BASE || '/').replace(/\/$/, '');

/** The notes write the block as raw HTML around a fenced code block, so at this stage it is a run of sibling nodes:
 *  raw "<details class="starter">", raw "<summary>…", the code block, raw "</details>". Replace that whole run with one card. */
export default function rehypePractice() {
  return (tree) => {
    const walk = (node) => {
      const kids = node.children;
      if (!kids) return;
      for (let i = 0; i < kids.length; i++) {
        const k = kids[i];
        if (k.type === 'raw' && /^<details class="starter">/.test(k.value)) {
          let end = i + 1;
          while (end < kids.length && !(kids[end].type === 'raw' && /<\/details>/.test(kids[end].value))) end++;
          const pre = kids.slice(i, end + 1).map((n) => (n.type === 'element' ? (n.tagName === 'pre' ? n : find(n, (x) => x.tagName === 'pre')) : null)).find(Boolean);
          const p = pre && end < kids.length && byHash.get(hashOf(text(pre)));
          if (p) {
            kids.splice(i, end - i + 1, card(p));
            continue;
          }
        }
        walk(k);
      }
    };
    walk(tree);
  };
}

const card = (p) => el('a', { className: ['practice-card'], href: `${base}/practice/${p.id}/`, dataPracticeId: p.id }, [
  el('span', { className: ['pc-ic'], ariaHidden: 'true' }, [el('svg', { viewBox: '0 0 24 24' }, [el('path', { d: 'M8 5v14l11-7z' })])]),
  el('span', { className: ['pc-body'] }, [el('strong', {}, [t(`Solve “${p.title}” on its own page`)]), el('small', {}, [t(`${p.difficulty} · task, examples, constraints, hints and a function-only editor`)])]),
  el('span', { className: ['pc-state'] }, [t('Open →')]),
]);
