// Learning-progress store. Everything lives in this browser's localStorage; nothing is sent anywhere.
export type Last = { id: string; title: string; href: string; track: string; ts: number };
export type State = {
  v: 1;
  done: Record<string, number>;      // note id -> completion timestamp (ms)
  last?: Last;                       // most recently opened note
  scroll: Record<string, number>;    // note id -> reading position, 0..1
  days: Record<string, number>;      // YYYY-MM-DD -> active reading seconds
};

const KEY = 'deepnotes:progress:v1';
const empty = (): State => ({ v: 1, done: {}, scroll: {}, days: {} });

let cache: State | null = null;
export function load(): State {
  if (cache) return cache;
  try {
    const raw = localStorage.getItem(KEY);
    const s = raw ? JSON.parse(raw) : null;
    cache = s && s.v === 1 ? { ...empty(), ...s } : empty();
  } catch { cache = empty(); }
  return cache!;
}
function save() {
  try { localStorage.setItem(KEY, JSON.stringify(cache)); } catch { /* private mode or quota: progress stays in memory for this tab */ }
  window.dispatchEvent(new CustomEvent('dn:change'));
}
window.addEventListener('storage', (e) => { if (e.key === KEY) { cache = null; window.dispatchEvent(new CustomEvent('dn:change')); } });

export const isDone = (id: string) => id in load().done;
export function setDone(id: string, value: boolean) {
  const s = load();
  if (value) s.done[id] = Date.now(); else delete s.done[id];
  save();
}
export function setLast(l: Omit<Last, 'ts'>) { load().last = { ...l, ts: Date.now() }; save(); }
export function setScroll(id: string, r: number) { load().scroll[id] = Math.round(r * 1000) / 1000; try { localStorage.setItem(KEY, JSON.stringify(cache)); } catch { /* ignore */ } }
const today = () => { const d = new Date(); return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`; };
export function addActive(seconds: number) { const s = load(); s.days[today()] = (s.days[today()] || 0) + seconds; try { localStorage.setItem(KEY, JSON.stringify(cache)); } catch { /* ignore */ } }

/** Consecutive days with activity or a completion, ending today or yesterday. */
export function streak(): number {
  const s = load();
  const active = new Set<string>(Object.keys(s.days).filter((d) => s.days[d] >= 30));
  for (const ts of Object.values(s.done)) { const d = new Date(ts); active.add(`${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`); }
  const cur = new Date();
  const fmt = (d: Date) => `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
  if (!active.has(fmt(cur))) cur.setDate(cur.getDate() - 1);          // a streak is still alive until the end of the next day
  let n = 0;
  while (active.has(fmt(cur))) { n++; cur.setDate(cur.getDate() - 1); }
  return n;
}
export const totalMinutes = () => Math.round(Object.values(load().days).reduce((a, b) => a + b, 0) / 60);

export function exportJson(): string { return JSON.stringify(load(), null, 2); }
export function importJson(text: string): boolean {
  try {
    const s = JSON.parse(text);
    if (!s || s.v !== 1 || typeof s.done !== 'object') return false;
    cache = { ...empty(), ...s }; save(); return true;
  } catch { return false; }
}
export function reset() { cache = empty(); save(); }
