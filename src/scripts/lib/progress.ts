// Learning progress: lessons and labs completed, reading position, active time. Built on the versioned store.
import aliases from '../../data/id-aliases.json';
import { createStore, isObj, isNumMap } from './store';

export type Last = { id: string; title: string; href: string; track: string; ts: number };
export type Progress = {
  done: Record<string, number>;      // item id -> completion time (ms). Lessons: "c/02-memory/01-x"; labs: "lab:linux/04-memory/01-x"
  scroll: Record<string, number>;    // item id -> reading position 0..1
  days: Record<string, number>;      // YYYY-MM-DD -> active seconds
  last?: Last;
};
const empty = (): Progress => ({ done: {}, scroll: {}, days: {} });
const check = (d: unknown): d is Progress => isObj(d) && isNumMap(d.done) && isNumMap(d.scroll) && isNumMap(d.days) && (d.last === undefined || (isObj(d.last) && typeof d.last.id === 'string' && typeof d.last.href === 'string'));

const alias = (m: Record<string, number>) => Object.fromEntries(Object.entries(m).map(([k, v]) => [(aliases as Record<string, string>)[k] ?? k, v]));
export const progress = createStore<Progress>({
  key: 'dn:progress', version: 1, empty, check,
  // the first release stored a bare object under another key and used note ids that have since been renamed
  legacy: { key: 'deepnotes:progress:v1', convert: (raw) => (isObj(raw) && raw.v === 1 ? { done: alias(raw.done as Record<string, number>), scroll: alias((raw.scroll as Record<string, number>) ?? {}), days: (raw.days as Record<string, number>) ?? {}, last: undefined } : null) },
});

export const isDone = (id: string) => id in progress.get().done;
export const setDone = (id: string, value: boolean) => progress.update((p) => { if (value) p.done[id] = Date.now(); else delete p.done[id]; });
export const setLast = (l: Omit<Last, 'ts'>) => progress.update((p) => { p.last = { ...l, ts: Date.now() }; }, { defer: true });
export const setScroll = (id: string, r: number) => progress.update((p) => { p.scroll[id] = Math.round(Math.min(1, Math.max(0, r)) * 1000) / 1000; }, { defer: true });

const fmt = (d: Date) => `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
export const addActive = (seconds: number) => progress.update((p) => { const k = fmt(new Date()); p.days[k] = (p.days[k] || 0) + seconds; }, { defer: true });

/** Consecutive days with at least 30 s of reading or a completion, ending today or yesterday. */
export function streak(extraDays: number[] = []): number {
  const p = progress.get(), active = new Set(Object.keys(p.days).filter((d) => p.days[d] >= 30));
  for (const ts of [...Object.values(p.done), ...extraDays]) active.add(fmt(new Date(ts)));
  const cur = new Date();
  if (!active.has(fmt(cur))) cur.setDate(cur.getDate() - 1);
  let n = 0;
  while (active.has(fmt(cur))) { n++; cur.setDate(cur.getDate() - 1); }
  return n;
}
export const totalMinutes = () => Math.round(Object.values(progress.get().days).reduce((a, b) => a + b, 0) / 60);
export const countDone = (ids: readonly string[]) => { const d = progress.get().done; let n = 0; for (const id of ids) if (id in d) n++; return n; };
