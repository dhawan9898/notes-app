// Loaded on every page: theme and preferences, global shortcuts, storage warnings, the Android link fix, and lazy loading of the
// progress runtime only on pages that use it.
import { settings, apply, setTheme } from './lib/settings';
import { appUrl, inApp, toast, go, href } from './lib/ui';

/* ---------- preferences (the inline script in <head> has already applied them before first paint) ---------- */
const dark = matchMedia('(prefers-color-scheme: dark)');
function paintTheme() {
  apply();
  document.querySelector<HTMLMetaElement>('meta[name="theme-color"]')?.setAttribute('content', document.documentElement.dataset.theme === 'dark' ? '#070a12' : '#f4f6fb');
}
settings.subscribe(paintTheme);
dark.addEventListener('change', () => { if (settings.get().theme === 'system') paintTheme(); });
paintTheme();
document.querySelectorAll<HTMLElement>('[data-theme-toggle]').forEach((b) => b.addEventListener('click', () => {
  const now = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
  setTheme(now);
}));

/* ---------- storage warning (private mode, full disk) ---------- */
window.addEventListener('dn:storage-error', () => toast('Your browser is not letting this site save data. Progress will be lost when you close the page.', undefined, 9000));

/* ---------- search shortcuts: "/" and Ctrl/Cmd+K ---------- */
addEventListener('keydown', (e) => {
  const t = e.target as HTMLElement | null;
  const typing = !!t && (/^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName) || t.isContentEditable);
  if (((e.key === 'k' || e.key === 'K') && (e.metaKey || e.ctrlKey)) || (e.key === '/' && !typing && !e.metaKey && !e.ctrlKey && !e.altKey)) {
    if (location.pathname.replace(/\/index\.html$/, '').endsWith('/search/')) { e.preventDefault(); document.querySelector<HTMLInputElement>('[data-search-input]')?.focus(); return; }
    e.preventDefault(); go(href('/search/'));
  }
});

/* ---------- Android app: the in-app web server answers extension-less folders with the home page, so folder links get "index.html" ---------- */
if (inApp()) {
  document.addEventListener('click', (e) => {
    const a = (e.target as HTMLElement).closest<HTMLAnchorElement>('a[href]');
    if (!a || e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || (a.target && a.target !== '_self') || a.hasAttribute('download')) return;
    const u = new URL(a.href, location.href);
    if (u.origin !== location.origin || !u.pathname.endsWith('/')) return;
    if (u.pathname === location.pathname && u.hash) return;
    e.preventDefault(); location.assign(appUrl(u));
  }, true);
}

/* ---------- progress runtime, only where the page asks for it ---------- */
if (document.querySelector('[data-item],[data-toggle],[data-meter],[data-continue],[data-stat],[data-status],[data-filterable],[data-filter-q]')) import('./meters');
