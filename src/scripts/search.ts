// The search page: instant results over the build-time index, kept offline. Recent searches are stored on this device.
import { search, KIND_LABEL, type Entry } from './lib/search';
import { createStore, isObj } from './lib/store';
import { $, esc, href, base } from './lib/ui';

const input = $<HTMLInputElement>('[data-search-input]')!, list = $('[data-results]')!, hint = $('[data-search-hint]')!, recentBox = $('[data-recent]')!;
const recent = createStore<{ q: string[] }>({ key: 'dn:recent-searches', version: 1, empty: () => ({ q: [] }), check: (d): d is { q: string[] } => isObj(d) && Array.isArray(d.q) && d.q.every((x) => typeof x === 'string') });
let index: Entry[] | null = null, loading: Promise<Entry[]> | null = null;
const TRACK: Record<string, string> = { c: 'C', dsa: 'DSA', linux: 'Linux', networking: 'Networking' };
const load = () => (loading ??= fetch(href('/search-index.json')).then((r) => r.json()).then((d: Entry[]) => (index = d)).catch(() => { loading = null; return []; }));

function renderRecent() {
  const q = recent.get().q;
  recentBox.hidden = !q.length || !!input.value.trim();
  recentBox.querySelector('ul')!.innerHTML = q.map((s) => `<li><button type="button" class="chip" data-r="${esc(s)}">${esc(s)}</button></li>`).join('');
}
async function run() {
  const q = input.value.trim();
  history.replaceState(null, '', q ? `?q=${encodeURIComponent(q)}` : location.pathname);
  renderRecent();
  if (q.length < 2) { list.innerHTML = ''; hint.hidden = false; hint.textContent = 'Type at least two letters.'; return; }
  const data = index ?? (await load());
  if (input.value.trim() !== q) return;                                  // the reader kept typing
  const res = search(data, q, 40);
  hint.hidden = res.length > 0; hint.textContent = res.length ? '' : `Nothing found for "${q}". Try fewer or different words.`;
  list.innerHTML = res.map((e) => `<li><a class="result" href="${esc(base() + e.u)}" data-k="${e.k}"><span class="result-kind k-${e.k}">${KIND_LABEL[e.k]}</span><span class="result-text"><strong>${esc(e.t)}</strong><span>${esc([e.r ? TRACK[e.r] : '', e.m, e.d, e.k === 'g' || e.k === 'a' || e.k === 'l' || e.k === 'b' ? (e.s ?? '') : ''].filter(Boolean).join(' · ').slice(0, 140))}</span></span></a></li>`).join('');
}
list.addEventListener('click', (e) => { const a = (e.target as HTMLElement).closest('a'); const q = input.value.trim(); if (a && q.length >= 2) recent.update((s) => { s.q = [q, ...s.q.filter((x) => x !== q)].slice(0, 6); }); });
recentBox.addEventListener('click', (e) => { const b = (e.target as HTMLElement).closest<HTMLElement>('[data-r]'); if (b) { input.value = b.dataset.r!; run(); input.focus(); } });
$('[data-clear-recent]')?.addEventListener('click', () => { recent.reset(); renderRecent(); });
input.addEventListener('input', run);
input.value = new URLSearchParams(location.search).get('q') ?? '';
void load(); run();
if (!input.value) input.focus();
