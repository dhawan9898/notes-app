// Reader preferences. The same values are applied before first paint by the inline script in Base.astro.
import { createStore, isObj } from './store';

export type Settings = { theme: 'system' | 'light' | 'dark'; text: 's' | 'm' | 'l'; motion: 'system' | 'reduce' };
const empty = (): Settings => ({ theme: 'system', text: 'm', motion: 'system' });
const check = (d: unknown): d is Settings => isObj(d) && ['system', 'light', 'dark'].includes(d.theme as string) && ['s', 'm', 'l'].includes(d.text as string) && ['system', 'reduce'].includes(d.motion as string);
export const settings = createStore<Settings>({
  key: 'dn:settings', version: 1, empty, check,
  legacy: { key: 'theme', convert: (raw) => ({ ...empty(), theme: raw === 'dark' || raw === 'light' ? raw : 'system' }) },
});

export function apply(s: Settings = settings.get()) {
  const el = document.documentElement;
  if (s.theme === 'system') delete el.dataset.theme; else el.dataset.theme = s.theme;
  el.dataset.text = s.text; el.dataset.motion = s.motion;
}
export const effectiveTheme = (): 'light' | 'dark' => {
  const t = settings.get().theme;
  return t === 'system' ? (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light') : t;
};
export const setTheme = (theme: Settings['theme']) => { settings.update((s) => { s.theme = theme; }); apply(); };
