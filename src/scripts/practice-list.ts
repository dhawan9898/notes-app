// Filters for the problem list: search, track, module, difficulty, status. The choice is kept for the session; ?track= and ?module= preselect.
import { practice, status } from './lib/practice';
import { $, $$ } from './lib/ui';

const rows = $$('[data-pr]');
const f = { q: '', track: '', diff: '', status: '', module: '', kind: '' };
const KEY = 'dn:practice-filters';
const empty = $('[data-pl-empty]')!, search = $<HTMLInputElement>('[data-pl-q]')!, moduleSel = $<HTMLSelectElement>('[data-pl-module]')!;

function apply() {
  let shown = 0;
  for (const r of rows) {
    const st = status(r.dataset.id!);
    const hit = (!f.q || r.dataset.search!.includes(f.q)) && (!f.track || r.dataset.track === f.track) && (!f.diff || r.dataset.diff === f.diff) && (!f.status || st === f.status) && (!f.module || r.dataset.module === f.module) && (!f.kind || r.dataset.kind === f.kind);
    r.hidden = !hit; if (hit) shown++;
  }
  empty.hidden = shown > 0;
  // only offer modules of the chosen track
  Array.from(moduleSel.options).forEach((o) => { if (o.value) o.hidden = !!f.track && o.dataset.track !== f.track; });
  try { sessionStorage.setItem(KEY, JSON.stringify(f)); } catch { /* ignore */ }
}
function chips(attr: string, key: 'track' | 'diff' | 'status' | 'kind') {
  const all = $$(`[${attr}]`);
  const mark = () => all.forEach((b) => b.classList.toggle('on', b.getAttribute(attr) === f[key]));
  all.forEach((b) => b.addEventListener('click', () => { f[key] = b.getAttribute(attr)!; if (key === 'track' && moduleSel.selectedOptions[0]?.hidden) { f.module = ''; moduleSel.value = ''; } mark(); apply(); }));
  return mark;
}
const marks = [chips('data-pl-track', 'track'), chips('data-pl-diff', 'diff'), chips('data-pl-status', 'status'), chips('data-pl-kind', 'kind')];
search.addEventListener('input', () => { f.q = search.value.trim().toLowerCase(); apply(); });
moduleSel.addEventListener('change', () => { f.module = moduleSel.value; apply(); });

try { Object.assign(f, JSON.parse(sessionStorage.getItem(KEY) || '{}')); } catch { /* ignore */ }
const qs = new URLSearchParams(location.search);
if (qs.has('track') || qs.has('module')) { f.track = qs.get('track') ?? ''; f.module = qs.get('module') ?? ''; f.q = ''; f.diff = ''; f.status = ''; f.kind = ''; }
search.value = f.q; moduleSel.value = f.module; marks.forEach((m) => m());
apply();
practice.subscribe(apply);
