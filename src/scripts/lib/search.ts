// Client-side search over the build-time index (scripts/build-data.mjs). Pure: no DOM, no network.
export type Entry = {
  k: 'l' | 'b' | 'p' | 'g' | 'a';      // lesson, lab, problem, glossary term, path
  t: string; u: string;               // title, URL below the site base
  s?: string; m?: string; r?: string; // summary, module, track
  h?: string[]; d?: string;           // headings, difficulty
};
export const KIND_LABEL: Record<Entry['k'], string> = { l: 'Lesson', b: 'Lab', p: 'Problem', g: 'Term', a: 'Path' };

type Prepared = { title: string; words: string[]; heads: string; text: string; mod: string };
const cache = new WeakMap<Entry, Prepared>();
const norm = (s: string) => s.toLowerCase().normalize('NFKD').replace(/[̀-ͯ]/g, '');
const words = (s: string) => norm(s).split(/[^a-z0-9+#]+/).filter(Boolean);
const prep = (e: Entry): Prepared => {
  let p = cache.get(e);
  if (!p) { p = { title: norm(e.t), words: words(e.t), heads: norm((e.h ?? []).join(' | ')), text: norm(e.s ?? ''), mod: norm(e.m ?? '') }; cache.set(e, p); }
  return p;
};
const KIND_BONUS: Record<Entry['k'], number> = { l: 1.2, p: 1, b: 0.8, g: 0.6, a: 1.5 };

function score(e: Entry, tokens: string[]): number {
  const p = prep(e);
  let total = 0;
  for (const t of tokens) {
    let s = 0;
    if (p.words.includes(t)) s = 10;
    else if (p.words.some((w) => w.startsWith(t))) s = 8;
    else if (p.title.includes(t)) s = 5;
    else if (p.heads.includes(t)) s = 3;
    else if (p.text.includes(t)) s = 2;
    else if (p.mod.includes(t)) s = 1;
    if (!s) return 0;                              // every word of the query must match somewhere
    total += s;
  }
  if (tokens.length > 1 && tokens.join(' ') && p.title.includes(tokens.join(' '))) total += 6;     // the whole phrase in the title
  return total * KIND_BONUS[e.k];
}

export function search(index: readonly Entry[], query: string, limit = 40): Entry[] {
  const q = norm(query).trim();
  if (q.length < 2) return [];
  const tokens = words(q);
  if (!tokens.length) return [];
  const scored: { e: Entry; s: number }[] = [];
  for (const e of index) { const s = score(e, tokens); if (s > 0) scored.push({ e, s }); }
  scored.sort((a, b) => b.s - a.s || a.e.t.localeCompare(b.e.t));
  return scored.slice(0, limit).map((x) => x.e);
}
