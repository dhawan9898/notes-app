import { load, isDone, setDone, setLast, setScroll, addActive, streak, totalMinutes, exportJson, importJson, reset } from './progress';

type Item = { i: string; t: string; h: string; k: string; m: number; s: number };
const $$ = <T extends HTMLElement = HTMLElement>(sel: string, root: ParentNode = document) => Array.from(root.querySelectorAll<T>(sel));
const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
document.documentElement.classList.add('js');

/* ---------- Android app (Capacitor): the in-app web server answers every extension-less path with the home page, so
   "/dsa/" would reload "/". Folder links are rewritten to "…/index.html" (a real file) before the browser follows them. ---------- */
const inApp = !!(window as unknown as { Capacitor?: { isNativePlatform?: () => boolean } }).Capacitor?.isNativePlatform?.();
export const appUrl = (u: URL) => (inApp && u.origin === location.origin && u.pathname.endsWith('/') ? `${u.pathname}index.html${u.search}${u.hash}` : u.href);
if (inApp) {
  document.addEventListener('click', (e) => {
    const a = (e.target as HTMLElement).closest<HTMLAnchorElement>('a[href]');
    if (!a || e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || (a.target && a.target !== '_self') || a.hasAttribute('download')) return;
    const u = new URL(a.href, location.href);
    if (u.origin !== location.origin || !u.pathname.endsWith('/')) return;
    if (u.pathname === location.pathname && u.hash) return;               // same-page anchor
    e.preventDefault(); location.assign(appUrl(u));
  }, true);
}

/* ---------- data embedded by the page (ordered list of notes) ---------- */
function items(): Item[] {
  const el = document.getElementById('dn-items');
  try { return el ? JSON.parse(el.textContent || '[]') : []; } catch { return []; }
}

/* ---------- progress rendering ---------- */
function renderProgress() {
  const state = load();
  $$('[data-note-id]').forEach((el) => el.toggleAttribute('data-done', el.dataset.noteId! in state.done));

  $$('[data-complete]').forEach((b) => b.setAttribute('aria-pressed', String(!!b.closest('[data-note-id]')?.hasAttribute('data-done'))));

  // groups: either children with data-note-id, or an explicit id list
  $$('[data-progress-group]').forEach((g) => {
    const ids = g.dataset.ids ? g.dataset.ids.split(',') : $$('[data-note-id]', g).map((e) => e.dataset.noteId!);
    const uniq = [...new Set(ids)];
    const done = uniq.filter((id) => id in state.done).length;
    const pct = uniq.length ? done / uniq.length : 0;
    g.style.setProperty('--p', String(pct));
    g.dataset.complete = pct >= 1 ? '1' : '0';
    const own = (sel: string) => $$(sel, g).filter((e) => e.closest('[data-progress-group]') === g);
    own('[data-progress-text]').forEach((t) => (t.textContent = t.dataset.format === 'pct' ? `${Math.round(pct * 100)}%` : `${done}/${uniq.length}`));
    own('[data-progress-bar]').forEach((b) => (b.style.transform = `scaleX(${pct})`));
    own('[data-ring-fg]').forEach((c) => {
      const r = Number(c.getAttribute('r')); const len = 2 * Math.PI * r;
      c.style.strokeDasharray = String(len); c.style.strokeDashoffset = String(len * (1 - pct));
    });
  });

  // global numbers
  const all = items();
  const total = all.length || Number(document.body.dataset.total || 0);
  const doneAll = all.length ? all.filter((n) => n.i in state.done).length : Object.keys(state.done).length;
  const mins = all.filter((n) => n.i in state.done).reduce((a, n) => a + n.m, 0);
  const remaining = all.filter((n) => !(n.i in state.done)).reduce((a, n) => a + n.m, 0);
  const set = (k: string, v: string) => $$(`[data-stat="${k}"]`).forEach((e) => countTo(e, v));
  set('done', String(doneAll)); set('total', String(total)); set('streak', String(streak()));
  set('hours', (totalMinutes() / 60).toFixed(1)); set('remaining', remaining >= 90 ? `${Math.round(remaining / 60)} h` : `${remaining} min`);
  set('percent', total ? `${Math.round((doneAll / total) * 100)}` : '0');
  void mins;
  const ov = total ? doneAll / total : 0;
  $$('[data-overall]').forEach((e) => { e.style.setProperty('--p', String(ov)); $$('[data-ring-fg]', e).forEach((c) => { const r = Number(c.getAttribute('r')); const len = 2 * Math.PI * r; c.style.strokeDasharray = String(len); c.style.strokeDashoffset = String(len * (1 - ov)); }); });

  // "continue" buttons: last opened unfinished note, else the first unfinished note of the scope
  $$('[data-continue]').forEach((a) => {
    const scope = a.dataset.continue!;                            // "all" or a track id
    const pool = all.filter((n) => scope === 'all' || n.k === scope);
    const last = state.last && (scope === 'all' || state.last.track === scope) && !(state.last.id in state.done) ? state.last : undefined;
    const next = last ? { t: last.title, h: last.href } : pool.find((n) => !(n.i in state.done));
    const label = a.querySelector<HTMLElement>('[data-continue-label]');
    const sub = a.querySelector<HTMLElement>('[data-continue-title]');
    if (next) {
      (a as HTMLAnchorElement).href = (next as { h: string }).h; a.hidden = false;
      if (label) label.textContent = last ? 'Continue reading' : (state.done && Object.keys(state.done).length ? 'Next up' : 'Start learning');
      if (sub) sub.textContent = (next as { t: string }).t;
    } else if (pool.length) {
      a.hidden = false; (a as HTMLAnchorElement).removeAttribute('href');
      if (label) label.textContent = 'Everything complete'; if (sub) sub.textContent = 'Revisit any note from the roadmap';
    } else a.hidden = true;
  });

  // filters on track/roadmap pages
  applyFilter();
}

function countTo(el: HTMLElement, value: string) {
  const target = Number(value);
  if (reduce || matchMedia('(hover: none)').matches || Number.isNaN(target) || !value.match(/^\d+$/)) { el.textContent = value; return; }
  const from = Number(el.dataset.v ?? 0); el.dataset.v = String(target);
  if (from === target) { el.textContent = value; return; }
  const t0 = performance.now();
  const step = (t: number) => { const k = Math.min(1, (t - t0) / 700); const e = 1 - Math.pow(1 - k, 3); el.textContent = String(Math.round(from + (target - from) * e)); if (k < 1) requestAnimationFrame(step); };
  requestAnimationFrame(step);
}

/* ---------- check circles anywhere on the site toggle completion ---------- */
document.addEventListener('click', (e) => {
  const b = (e.target as HTMLElement).closest<HTMLElement>('[data-toggle]');
  if (!b) return;
  e.preventDefault(); e.stopPropagation();
  const id = b.dataset.toggle!; const now = !isDone(id); setDone(id, now);
  if (now) celebrate(b);
}, true);

/* ---------- filters (all / to do / done / by track) ---------- */
let filter = { status: 'all', track: 'all', q: '' };
function applyFilter() {
  $$('[data-filterable]').forEach((row) => {
    const done = row.hasAttribute('data-done');
    const ok = (filter.status === 'all' || (filter.status === 'done') === done) && (filter.track === 'all' || row.dataset.track === filter.track)
      && (!filter.q || (row.dataset.search || '').includes(filter.q));
    row.hidden = !ok;
  });
  $$('[data-filter-section]').forEach((sec) => { sec.hidden = $$('[data-filterable]', sec).every((r) => r.hidden); });
  const none = document.querySelector<HTMLElement>('[data-filter-empty]');
  if (none) none.hidden = $$('[data-filterable]').some((r) => !r.hidden);
}
function bindFilters() {
  $$('[data-filter-status]').forEach((b) => b.addEventListener('click', () => { filter.status = b.dataset.filterStatus!; $$('[data-filter-status]').forEach((x) => x.classList.toggle('on', x === b)); applyFilter(); }));
  $$('[data-filter-track]').forEach((b) => b.addEventListener('click', () => { filter.track = b.dataset.filterTrack!; $$('[data-filter-track]').forEach((x) => x.classList.toggle('on', x === b)); applyFilter(); }));
  const q = document.querySelector<HTMLInputElement>('[data-filter-q]');
  q?.addEventListener('input', () => { filter.q = q.value.trim().toLowerCase(); applyFilter(); });
}

/* ---------- note page: completion, reading bar, resume, active time ---------- */
function toast(msg: string, action?: { label: string; run: () => void }) {
  document.querySelectorAll('.toast').forEach((t) => t.remove());
  const t = document.createElement('div'); t.className = 'toast glass'; t.setAttribute('role', 'status');
  t.append(Object.assign(document.createElement('span'), { textContent: msg }));
  if (action) { const b = document.createElement('button'); b.textContent = action.label; b.addEventListener('click', () => { action.run(); t.remove(); }); t.append(b); }
  document.body.append(t); requestAnimationFrame(() => t.classList.add('in'));
  setTimeout(() => { t.classList.remove('in'); setTimeout(() => t.remove(), 400); }, 6000);
}

// completion feedback is the animated check itself; no particles
const celebrate = (_el: HTMLElement) => {};

function initNote() {
  const page = document.querySelector<HTMLElement>('[data-note-page]');
  if (!page) return;
  const id = page.dataset.noteId!;
  setLast({ id, title: page.dataset.title!, href: location.pathname, track: page.dataset.track! });
  const article = document.querySelector<HTMLElement>('.article-content');
  const bar = document.getElementById('readbar');
  const pctEl = document.querySelector<HTMLElement>('[data-read-pct]');
  const complete = $$('[data-complete]');
  let ticking = false, lastInput = Date.now();

  const update = () => {
    ticking = false;
    if (!article) return;
    const r = article.getBoundingClientRect();
    const total = r.height - innerHeight * 0.6;
    const ratio = Math.min(1, Math.max(0, total > 0 ? -r.top / total : 1));
    if (bar) bar.style.transform = `scaleX(${ratio})`;
    if (pctEl) pctEl.textContent = `${Math.round(ratio * 100)}%`;
    setScroll(id, ratio);
    complete.forEach((b) => b.classList.toggle('nudge', ratio > 0.92 && !isDone(id)));
  };
  addEventListener('scroll', () => { lastInput = Date.now(); if (!ticking) { ticking = true; requestAnimationFrame(update); } }, { passive: true });
  ['mousemove', 'keydown', 'touchstart'].forEach((ev) => addEventListener(ev, () => (lastInput = Date.now()), { passive: true }));
  update();

  complete.forEach((b) => b.addEventListener('click', () => {
    const now = !isDone(id); setDone(id, now);
    if (now) { celebrate(b); toast('Marked complete', page.dataset.next ? { label: 'Next note →', run: () => location.assign(appUrl(new URL(page.dataset.next!, location.href))) } : undefined); }
  }));
  addEventListener('keydown', (e) => {
    if (e.key.toLowerCase() === 'm' && !e.metaKey && !e.ctrlKey && !e.altKey && !/INPUT|TEXTAREA/.test((document.activeElement as HTMLElement)?.tagName)) complete[0]?.click();
  });

  // offer to resume where the reader stopped
  const saved = load().scroll[id];
  if (saved && saved > 0.04 && saved < 0.96 && !location.hash && article) {
    const target = () => { const r = article.getBoundingClientRect(); return scrollY + r.top + (r.height - innerHeight * 0.6) * saved - 90; };
    toast(`Resume at ${Math.round(saved * 100)}%?`, { label: 'Resume', run: () => scrollTo({ top: target(), behavior: reduce ? 'auto' : 'smooth' }) });
  }
  // active reading time: counts only while the tab is visible and the reader did something in the last minute
  const t = setInterval(() => { if (!document.hidden && Date.now() - lastInput < 60000) addActive(5); }, 5000);
  addEventListener('pagehide', () => clearInterval(t));
}

/* ---------- roadmap page: export / import / reset ---------- */
function initTools() {
  document.querySelector('[data-export]')?.addEventListener('click', () => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([exportJson()], { type: 'application/json' }));
    a.download = `deepnotes-progress-${new Date().toISOString().slice(0, 10)}.json`; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  });
  const file = document.querySelector<HTMLInputElement>('[data-import-file]');
  document.querySelector('[data-import]')?.addEventListener('click', () => file?.click());
  file?.addEventListener('change', async () => { const f = file.files?.[0]; if (!f) return; toast(importJson(await f.text()) ? 'Progress imported' : 'That file is not a DeepNotes progress export'); file.value = ''; });
  document.querySelector('[data-reset]')?.addEventListener('click', () => { if (confirm('Erase all progress stored in this browser?')) { reset(); toast('Progress cleared'); } });
}

/* ---------- visual polish: cursor-lit glass, scroll reveal, header state ---------- */
function initVisuals() {
  const root = document.documentElement;
  // header state: toggles a class only when it changes (no per-frame style writes)
  const header = document.querySelector('header.top');
  let scrolled = false;
  addEventListener('scroll', () => { const now = scrollY > 8; if (now !== scrolled) { scrolled = now; header?.classList.toggle('scrolled', now); } }, { passive: true });

  // cursor-following highlight on glass panels
  if (!reduce) {
    let pending: PointerEvent | null = null;
    document.addEventListener('pointermove', (e) => {
      if (e.pointerType !== 'mouse') return;
      if (pending) { pending = e; return; }
      pending = e;
      requestAnimationFrame(() => { const ev = pending!; pending = null; lit(ev); });
    }, { passive: true });
    const lit = (e: PointerEvent) => {
      const el = (e.target as HTMLElement).closest<HTMLElement>('.track-card, .module-card, .cta-card, .pager-card, .dash, .rm-hero, .rm-list, .philosophy-item, .sources');
      if (!el) return;
      const r = el.getBoundingClientRect();
      el.style.setProperty('--mx', `${e.clientX - r.left}px`); el.style.setProperty('--my', `${e.clientY - r.top}px`);
    };
  }

  // reveal: article blocks join the explicitly marked elements. Browsers with scroll-driven animations scrub this in CSS;
  // the others use an IntersectionObserver (same visual result, time-based instead of scroll-based).
  const targets = $$('[data-reveal]');
  const scrubbed = typeof CSS !== 'undefined' && CSS.supports?.('animation-timeline: view()');
  if (reduce || scrubbed || !('IntersectionObserver' in window)) { targets.forEach((t) => t.classList.add('shown')); return; }
  const io = new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) { (e.target as HTMLElement).classList.add('shown'); io.unobserve(e.target); } }), { rootMargin: '0px 0px -6% 0px', threshold: 0.04 });
  targets.forEach((t, i) => { t.style.setProperty('--d', `${Math.min(i % 5, 4) * 45}ms`); io.observe(t); });
}

renderProgress(); bindFilters(); initNote(); initTools(); initVisuals();
window.addEventListener('dn:change', renderProgress);
