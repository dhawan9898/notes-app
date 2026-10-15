// Shiki writes the colours of every token inline (`style="color:#D73A49;--shiki-dark:#F97583"`), which was a quarter of the HTML.
// The github-light / github-dark pair uses only a handful of colour pairs, so each becomes a short class (.k0 … .kN, defined in
// src/styles/base.css) and the code block's own style becomes a class too. A style that is not in the table is left untouched.
export const TOKEN = [
  'color:#D73A49;--shiki-dark:#F97583', 'color:#005CC5;--shiki-dark:#79B8FF', 'color:#6F42C1;--shiki-dark:#B392F0', 'color:#24292E;--shiki-dark:#E1E4E8',
  'color:#E36209;--shiki-dark:#FFAB70', 'color:#032F62;--shiki-dark:#9ECBFF', 'color:#6A737D;--shiki-dark:#6A737D',
  'color:#B31D28;--shiki-light-font-style:italic;--shiki-dark:#FDAEB7;--shiki-dark-font-style:italic', 'color:#22863A;--shiki-dark:#85E89D', 'color:#032F62;--shiki-dark:#DBEDFF',
];
const BLOCK = /^background-color:#fff;--shiki-dark-bg:#24292e;color:#24292e;--shiki-dark:#e1e4e8;\s*overflow-x:\s*auto;?$/;
const index = new Map(TOKEN.map((s, i) => [s, i]));

export function rehypeShikiCompact() {
  return (tree) => {
    const walk = (n) => {
      for (const c of n.children || []) {
        if (c.type !== 'element') continue;
        const style = c.properties?.style;
        if (typeof style === 'string') {
          if (c.tagName === 'span' && index.has(style)) { c.properties.className = [`k${index.get(style)}`]; delete c.properties.style; }
          else if (c.tagName === 'pre' && BLOCK.test(style)) { delete c.properties.style; }
        }
        walk(c);
      }
    };
    walk(tree);
  };
}

/** The same transformation for HTML strings (used when the build scripts render Markdown themselves). */
export const compactHtml = (html) => html
  .replace(/<span style="([^"]*)">/g, (m, s) => (index.has(s) ? `<span class="k${index.get(s)}">` : m))
  .replace(/ style="(background-color:#fff;--shiki-dark-bg:#24292e;color:#24292e;--shiki-dark:#e1e4e8;\s*overflow-x:\s*auto;)"/g, '');
