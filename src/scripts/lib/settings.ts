// Reader preferences. The same values are applied before first paint by the inline script in Base.astro.
import { createStore, isObj } from './store';

export type Settings = { theme: 'system' | 'light' | 'dark'; text: 's' | 'm' | 'l'; motion: 'system' | 'reduce' };
const empty = (): Settings => ({ theme: 'system', text: 'm', motion: 'system' });
const check = (d: unknown): d is Settings => isObj(d) && ['system', 'light', 'dark'].includes(d.theme as string) && ['s', 'm', 'l'].includes(d.text as string) && ['system', 'reduce'].includes(d.motion as string);
export const settings = createStore<Settings>({
  key: 'dn:settings', version: 1, empty, check,
  legacy: { key: 'theme', convert: (raw) => ({ ...empty(), theme: raw === 'dark' || raw === 'light' ? raw : 'system' }) },
});

/** Always writes a resolved theme ("light" or "dark") so the stylesheet needs only two palettes. */
export function apply(s: Settings = settings.get()) {
  const el = document.documentElement;
  const dark = s.theme === 'dark' || (s.theme === 'system' && matchMedia('(prefers-color-scheme: dark)').matches);
  el.dataset.theme = dark ? 'dark' : 'light'; el.dataset.text = s.text; el.dataset.motion = s.motion;
}
export const setTheme = (theme: Settings['theme']) => { settings.update((s) => { s.theme = theme; }); apply(); };
