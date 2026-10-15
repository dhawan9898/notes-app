// Lesson and lab pages: reading position and progress bar, resume, active time, completion, outline, code-block helpers, diagrams.
import { progress, setLast, setScroll, setDone, isDone, addActive } from './lib/progress';
import { $, $$, toast, go, reduceMotion } from './lib/ui';

const page = document.querySelector<HTMLElement>('[data-lesson]');
if (page) init(page);

function init(page: HTMLElement) {
  const id = page.dataset.id!, nextHref = page.dataset.next || '';
  setLast({ id, title: page.dataset.title!, href: location.pathname, track: page.dataset.track! });
  const bar = $('[data-readbar]');
  let ticking = false, lastInput = Date.now();

  const update = () => {
    ticking = false;
    const r = page.getBoundingClientRect(), span = r.height - innerHeight * 0.6;
    const ratio = Math.min(1, Math.max(0, span > 0 ? -r.top / span : 1));
    if (bar) bar.style.transform = `scaleX(${ratio})`;
    setScroll(id, ratio);
  };
  addEventListener('scroll', () => { lastInput = Date.now(); if (!ticking) { ticking = true; requestAnimationFrame(update); } }, { passive: true });
  ['pointerdown', 'keydown'].forEach((ev) => addEventListener(ev, () => (lastInput = Date.now()), { passive: true }));
  update();

  // completion
  let wasDone = isDone(id);
  progress.subscribe(() => {
    const now = isDone(id);
    if (now && !wasDone) toast('Marked as complete', nextHref ? { label: 'Next →', run: () => go(nextHref) } : undefined);
    wasDone = now;
  });
  addEventListener('keydown', (e) => {
    if (e.key.toLowerCase() === 'm' && !e.metaKey && !e.ctrlKey && !e.altKey && !/^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement as HTMLElement)?.tagName)) setDone(id, !isDone(id));
  });

  // offer to resume where the reader stopped
  const saved = progress.get().scroll[id];
  if (saved && saved > 0.04 && saved < 0.96 && !location.hash) {
    const target = () => { const r = page.getBoundingClientRect(); return scrollY + r.top + (r.height - innerHeight * 0.6) * saved - 90; };
    toast(`Resume at ${Math.round(saved * 100)}%?`, { label: 'Resume', run: () => scrollTo({ top: target(), behavior: reduceMotion() ? 'auto' : 'smooth' }) });
  }
  // active reading time: counts only while the tab is visible and the reader did something in the last minute
  const t = setInterval(() => { if (!document.hidden && Date.now() - lastInput < 60000) addActive(5); }, 5000);
  addEventListener('pagehide', () => clearInterval(t));

  /* ---------- outline: highlight the section being read ---------- */
  const links = $$<HTMLAnchorElement>('.toc a[data-spy]');
  if (links.length && 'IntersectionObserver' in window) {
    const io = new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) links.forEach((l) => l.classList.toggle('on', l.dataset.spy === e.target.id)); }), { rootMargin: '-72px 0px -70% 0px' });
    $$('.prose h2[id], .prose h3[id]').forEach((h) => io.observe(h));
  }

  /* ---------- contents sheet (phones) ---------- */
  const dlg = $<HTMLDialogElement>('[data-contents]');
  if (dlg) {
    $('[data-open-contents]')?.addEventListener('click', () => { if (typeof dlg.showModal === 'function') dlg.showModal(); else dlg.setAttribute('open', ''); });
    $('[data-close]', dlg)?.addEventListener('click', () => dlg.close());
    dlg.addEventListener('click', (e) => { const t = e.target as HTMLElement; if (t === dlg || t.closest('a')) dlg.close(); });
  }

  /* ---------- code blocks: copy button; headings: permalink ---------- */
  $$('.prose pre').forEach((pre) => {
    if (pre.dataset.language === 'mermaid' || pre.closest('.runbox')) return;
    const b = document.createElement('button'); b.type = 'button'; b.className = 'copy'; b.textContent = 'Copy'; b.setAttribute('aria-label', 'Copy code');
    b.addEventListener('click', async () => {
      try { await navigator.clipboard.writeText(pre.querySelector('code')?.innerText || pre.innerText); b.textContent = 'Copied'; setTimeout(() => (b.textContent = 'Copy'), 1800); }
      catch { b.textContent = 'Press Ctrl+C'; }
    });
    pre.classList.add('has-copy'); pre.append(b);
  });
  $$('.prose h2[id], .prose h3[id]').forEach((h) => { const a = document.createElement('a'); a.className = 'anchor'; a.href = `#${h.id}`; a.setAttribute('aria-label', `Link to ${h.textContent}`); a.textContent = '#'; h.append(a); });

  /* ---------- runnable C blocks and diagrams, only if the page has them ---------- */
  if (document.querySelector('.prose pre[data-language="c"]')) import('./runner');
  const diagrams = $$('.prose pre[data-language="mermaid"]');
  if (diagrams.length) import('mermaid').then(({ default: mermaid }) => {
    mermaid.initialize({ startOnLoad: false, theme: document.documentElement.dataset.theme === 'dark' ? 'dark' : 'default' });
    diagrams.forEach(async (d, i) => {
      try { const { svg } = await mermaid.render(`m${i}`, d.textContent || ''); const w = document.createElement('div'); w.className = 'diagram'; w.innerHTML = svg; d.replaceWith(w); }
      catch (err) { console.error('Diagram error', err); }
    });
  });
}
