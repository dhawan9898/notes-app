// A small, strict persistence layer on top of localStorage. Every store keeps one JSON envelope {v, data}:
//  - versioned, with step-by-step migrations (and an optional importer for a legacy key)
//  - validated on read; unreadable or invalid data is set aside under `<key>:corrupt` and the store starts empty (nothing is thrown)
//  - writes are debounced and flushed when the page is hidden; a failed write (full or blocked storage) keeps the data in memory and is reported once
//  - other tabs' changes are picked up through the `storage` event
// No DOM is touched except through `window` events, so the module runs unchanged under Node tests.
export type Json = null | boolean | number | string | Json[] | { [k: string]: Json };

export interface Store<T> {
  get(): T;
  /** Mutates a draft in place and persists it. `defer` batches rapid changes (scroll position, reading time). */
  update(fn: (draft: T) => void, opts?: { defer?: boolean }): void;
  /** Replaces the data if it is valid (returns false and changes nothing otherwise). A failed write is reported through `state`. */
  replace(next: T): boolean;
  reset(): void;
  flush(): void;
  subscribe(fn: () => void): () => void;
  /** 'ok', 'recovered' (corrupt data was set aside) or 'memory-only' (the last write failed). */
  readonly state: 'ok' | 'recovered' | 'memory-only';
}

export interface StoreOptions<T> {
  key: string;
  version: number;
  empty: () => T;
  check: (data: unknown) => data is T;
  /** migrations[n] converts data written by version n into version n + 1 */
  migrations?: Record<number, (data: any) => any>;
  /** reads data from an older storage key (any shape) and returns the current-version data, or null */
  legacy?: { key: string; convert: (raw: any) => unknown };
  debounceMs?: number;
}

const ls = (): Storage | null => { try { return typeof localStorage === 'undefined' ? null : localStorage; } catch { return null; } };
const emit = (name: string, detail?: unknown) => { try { window.dispatchEvent(new CustomEvent(name, { detail })); } catch { /* no window (tests) */ } };

export function createStore<T>(o: StoreOptions<T>): Store<T> {
  let cache: T | null = null, state: Store<T>['state'] = 'ok', timer: ReturnType<typeof setTimeout> | undefined, dirty = false;
  const listeners = new Set<() => void>();

  function migrate(v: number, data: unknown): unknown {
    let cur = data;
    for (let n = v; n < o.version; n++) { const step = o.migrations?.[n]; if (!step) return null; cur = step(cur); }
    return cur;
  }
  function setAside(raw: string) { try { ls()?.setItem(`${o.key}:corrupt`, raw.slice(0, 200_000)); } catch { /* ignore */ } state = 'recovered'; }

  function read(): T {
    const s = ls();
    if (!s) return o.empty();
    let raw: string | null = null;
    try { raw = s.getItem(o.key); } catch { return o.empty(); }
    if (raw !== null) {
      try {
        const env = JSON.parse(raw);
        if (env && typeof env === 'object' && Number.isInteger(env.v) && env.v >= 1 && env.v <= o.version) {
          const data = migrate(env.v, env.data);
          if (data !== null && o.check(data)) return data;
        }
      } catch { /* fall through */ }
      setAside(raw);
      return o.empty();
    }
    if (o.legacy) {
      try {
        const old = s.getItem(o.legacy.key);
        if (old) { let parsed: unknown = old; try { parsed = JSON.parse(old); } catch { /* a bare string such as "dark" */ } const data = o.legacy.convert(parsed); if (data !== null && data !== undefined && o.check(data)) { cache = data; dirty = true; persist(); return data; } }
      } catch { /* ignore a broken legacy value */ }
    }
    return o.empty();
  }

  function persist(): boolean {
    clearTimeout(timer); timer = undefined;
    if (!dirty || cache === null) return true;
    dirty = false;
    const s = ls();
    try { if (!s) throw new Error('no storage'); s.setItem(o.key, JSON.stringify({ v: o.version, data: cache })); if (state === 'memory-only') state = 'ok'; return true; }
    catch { if (state !== 'memory-only') emit('dn:storage-error', { key: o.key }); state = 'memory-only'; return false; }
  }
  const notify = () => { listeners.forEach((f) => { try { f(); } catch { /* a bad listener must not break the others */ } }); emit('dn:store', { key: o.key }); };

  try {
    window.addEventListener('storage', (e: StorageEvent) => { if (e.key === o.key) { cache = null; notify(); } });
    window.addEventListener('pagehide', () => persist());
    document.addEventListener('visibilitychange', () => { if (document.hidden) persist(); });
  } catch { /* no window (tests) */ }

  return {
    get: () => (cache ??= read()),
    update(fn, opts) {
      const d = (cache ??= read()); fn(d); dirty = true;
      if (opts?.defer) { clearTimeout(timer); timer = setTimeout(persist, o.debounceMs ?? 500); } else persist();
      notify();
    },
    replace(next) { if (!o.check(next)) return false; cache = next; dirty = true; persist(); notify(); return true; },
    reset() { cache = o.empty(); dirty = true; persist(); notify(); },
    flush() { persist(); },
    subscribe(fn) { listeners.add(fn); return () => listeners.delete(fn); },
    get state() { return state; },
  };
}

export const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v);
export const isNumMap = (v: unknown): v is Record<string, number> => isObj(v) && Object.values(v).every((x) => typeof x === 'number' && Number.isFinite(x));
