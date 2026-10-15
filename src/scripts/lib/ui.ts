// Small DOM helpers shared by the page scripts.
export const $ = <T extends HTMLElement = HTMLElement>(sel: string, root: ParentNode = document) => root.querySelector<T>(sel);
export const $$ = <T extends HTMLElement = HTMLElement>(sel: string, root: ParentNode = document) => Array.from(root.querySelectorAll<T>(sel));
export const esc = (s: string) => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!));
export const reduceMotion = () => document.documentElement.dataset.motion === 'reduce' || matchMedia('(prefers-reduced-motion: reduce)').matches;

/** Site base path (the GitHub Pages sub-path, or "/" in the app), always without a trailing slash. */
export const base = () => (document.querySelector<HTMLMetaElement>('meta[name="dn-base"]')?.content || '/').replace(/\/$/, '');
export const href = (path: string) => base() + path;

/* ---------- the Android app serves files, so "/dsa/" must become "/dsa/index.html" ---------- */
export const inApp = () => !!(window as unknown as { Capacitor?: { isNativePlatform?: () => boolean } }).Capacitor?.isNativePlatform?.();
export const appUrl = (u: URL) => (inApp() && u.origin === location.origin && u.pathname.endsWith('/') ? `${u.pathname}index.html${u.search}${u.hash}` : u.href);
export const go = (path: string) => location.assign(appUrl(new URL(path, location.href)));

/* ---------- toast ---------- */
export function toast(message: string, action?: { label: string; run: () => void }, ms = 5000) {
  document.querySelectorAll('.toast').forEach((t) => t.remove());
  const t = document.createElement('div'); t.className = 'toast'; t.setAttribute('role', 'status');
  const span = document.createElement('span'); span.textContent = message; t.append(span);
  if (action) { const b = document.createElement('button'); b.type = 'button'; b.textContent = action.label; b.addEventListener('click', () => { action.run(); t.remove(); }); t.append(b); }
  document.body.append(t); requestAnimationFrame(() => t.classList.add('in'));
  setTimeout(() => { t.classList.remove('in'); setTimeout(() => t.remove(), 300); }, ms);
}
