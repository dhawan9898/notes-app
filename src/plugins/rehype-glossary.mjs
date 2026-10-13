// Link the first occurrence of each glossary term on a page to its glossary entry. Only for beginner and intermediate notes,
// only terms of the note's own track (or "General"), only in running text (never in code, headings or existing links).
import fs from 'node:fs';

const terms = JSON.parse(fs.readFileSync(new URL('../data/glossary.json', import.meta.url), 'utf8'));
const base = (process.env.BASE || '/').replace(/\/$/, '');
const slug = (t) => t.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const TRACK = { c: 'C', dsa: 'DSA', linux: 'Linux', networking: 'Networking' };

// match string -> glossary entry; strings that map to more than one entry are dropped as ambiguous
const owners = new Map();
for (const t of terms) {
  const m = t.term.match(/^(.*?)\s*\((.*)\)\s*$/);
  const head = m ? m[1] : t.term;
  const strings = new Set([head]);
  if (/^[A-Z0-9][A-Z0-9/ -]*$/.test(head) && m) strings.add(m[2]);                  // acronym (expansion): both spellings
  if (!/^[A-Z0-9]{2,}/.test(head)) strings.add(head[0].toLowerCase() + head.slice(1));
  for (const s of strings) {
    if (s.length < 3) continue;
    owners.set(s, owners.has(s) && owners.get(s) !== t ? null : t);
  }
}
const candidates = [...owners].filter(([, t]) => t).sort((a, b) => b[0].length - a[0].length);
const SKIP = new Set(['a', 'code', 'pre', 'kbd', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'script', 'style', 'svg']);

export default function rehypeGlossary() {
  return (tree, file) => {
    const fm = file.data?.astro?.frontmatter ?? {};
    if (!['beginner', 'intermediate'].includes(fm.level)) return;
    const track = TRACK[fm.track];
    const usable = candidates.filter(([, t]) => t.track === track || t.track === 'General');
    if (!usable.length) return;
    const used = new Set();
    const re = new RegExp('(?<![\\w-])(' + usable.map(([s]) => esc(s)).join('|') + ')(?![\\w-])', 'g');
    const lookup = new Map(usable);

    const walk = (node) => {
      if (!node.children) return;
      const out = [];
      for (const child of node.children) {
        if (child.type === 'element') {
          if (!SKIP.has(child.tagName)) walk(child);
          out.push(child);
        } else if (child.type === 'text' && node.type === 'element' && !SKIP.has(node.tagName)) {
          let last = 0, m, pieces = [];
          re.lastIndex = 0;
          while ((m = re.exec(child.value))) {
            const t = lookup.get(m[1]);
            if (used.has(t.term)) continue;
            used.add(t.term);
            if (m.index > last) pieces.push({ type: 'text', value: child.value.slice(last, m.index) });
            pieces.push({ type: 'element', tagName: 'a', properties: { href: `${base}/glossary/#${slug(t.term)}`, className: ['gl'], title: t.def }, children: [{ type: 'text', value: m[1] }] });
            last = m.index + m[1].length;
          }
          if (pieces.length) { if (last < child.value.length) pieces.push({ type: 'text', value: child.value.slice(last) }); out.push(...pieces); }
          else out.push(child);
        } else out.push(child);
      }
      node.children = out;
    };
    walk(tree);
  };
}
