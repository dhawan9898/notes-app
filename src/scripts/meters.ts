// Declarative progress UI. Pages mark up what they need with data attributes; this module fills them from the stores.
//   [data-item="KEY"]              gets data-done when the lesson/lab KEY is completed
//   [data-toggle="KEY"]            click toggles completion of KEY
//   [data-meter="lessons|labs|problems"] [data-prefix="a,b"]   progress of everything whose id starts with a prefix
//        children: [data-fill] (bar, scaleX), [data-ring-fg] (svg circle), [data-text] ("3/12" or "25%" with data-format="pct"), [data-n-done], [data-n-total]
//   [data-continue="all|TRACK|path"] [data-prefix]   link to the lesson to read next (within a track, or within the prefixes) ([data-continue-label], [data-continue-title])
//   [data-stat="done|total|percent|streak|hours|solved|solved-total|labs-done"]   numbers
//   [data-status="PROBLEM-ID"]     gets data-state = solved | attempted | todo
//   [data-filter-status] / [data-filter-q] / [data-filterable] [data-search]   status and text filters on lists
import { progress, setDone, isDone, streak, totalMinutes } from './lib/progress';
import { practice, status as problemStatus } from './lib/practice';
import { lessons, labs, problems } from './lib/manifest';
import { $, $$, href } from './lib/ui';

const startsWithAny = (id: string, prefixes: string[]) => prefixes.some((p) => id.startsWith(p));

function meter(el: HTMLElement) {
  const kind = el.dataset.meter!, prefixes = (el.dataset.prefix ?? '').split(',').filter((p) => p !== undefined);
  const wanted = (id: string) => !el.dataset.prefix || startsWithAny(id, prefixes);
  let ids: string[];
  if (kind === 'problems') ids = problems.filter((p) => wanted(p.i)).map((p) => p.i);
  else ids = (kind === 'labs' ? labs : lessons).filter((x) => wanted(x.i)).map((x) => x.i);
  const done = kind === 'problems' ? ids.filter((i) => problemStatus(i) === 'solved').length : ids.filter((i) => i in progress.get().done).length;
  const p = ids.length ? done / ids.length : 0;
  el.style.setProperty('--p', String(p)); el.dataset.complete = p >= 1 ? '1' : '0';
  $$('[data-fill]', el).forEach((f) => (f.style.transform = `scaleX(${p})`));
  $$('[data-ring-fg]', el).forEach((c) => { const len = 2 * Math.PI * Number(c.getAttribute('r')); c.style.strokeDasharray = String(len); c.style.strokeDashoffset = String(len * (1 - p)); });
  $$('[data-text]', el).forEach((t) => (t.textContent = t.dataset.format === 'pct' ? `${Math.round(p * 100)}%` : `${done}/${ids.length}`));
  $$('[data-n-done]', el).forEach((t) => (t.textContent = String(done)));
  $$('[data-n-total]', el).forEach((t) => (t.textContent = String(ids.length)));
}

function items() {
  const done = progress.get().done;
  $$('[data-item]').forEach((el) => el.toggleAttribute('data-done', el.dataset.item! in done));
  $$('[data-toggle]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.toggle! in done)));
}

function continueLinks() {
  const p = progress.get();
  $$<HTMLAnchorElement>('[data-continue]').forEach((a) => {
    const scope = a.dataset.continue!, prefixes = (a.dataset.prefix ?? '').split(',').filter(Boolean);
    const pool = lessons.filter((l) => (prefixes.length ? startsWithAny(l.i, prefixes) : scope === 'all' || l.r === scope));
    const last = p.last && !(p.last.id in p.done) && pool.some((l) => l.i === p.last!.id) ? p.last : undefined;
    const next = last ? { t: last.title, u: last.href.replace(base(), '') } : pool.find((l) => !(l.i in p.done));
    const label = $('[data-continue-label]', a), title = $('[data-continue-title]', a);
    a.hidden = false;
    if (next) {
      a.href = last ? last.href : href(next.u);
      if (label) label.textContent = last ? 'Continue reading' : Object.keys(p.done).length ? 'Up next' : 'Start here';
      if (title) title.textContent = next.t;
    } else if (pool.length) { a.removeAttribute('href'); if (label) label.textContent = 'All done'; if (title) title.textContent = 'Revisit any lesson from the list'; }
    else a.hidden = true;
  });
}
const base = () => href('').replace(/\/$/, '');

function stats() {
  const p = progress.get(), validDone = lessons.filter((l) => l.i in p.done);
  const doneMin = validDone.reduce((a, l) => a + l.m, 0), left = lessons.filter((l) => !(l.i in p.done)).reduce((a, l) => a + l.m, 0);
  const solved = problems.filter((x) => problemStatus(x.i) === 'solved').length;
  const set = (k: string, v: string) => $$(`[data-stat="${k}"]`).forEach((e) => (e.textContent = v));
  set('done', String(validDone.length)); set('total', String(lessons.length)); set('percent', String(lessons.length ? Math.round((validDone.length / lessons.length) * 100) : 0));
  set('streak', String(streak(Object.values(practice.get().p).flatMap((r) => (r.solved ? [r.solved] : [])))));
  set('hours', (totalMinutes() / 60).toFixed(1)); set('left', left >= 90 ? `${Math.round(left / 60)} h` : `${left} min`); set('read-min', String(doneMin));
  set('solved', String(solved)); set('solved-total', String(problems.length));
  set('labs-done', String(labs.filter((l) => l.i in p.done).length)); set('labs-total', String(labs.length));
  for (const df of ['Easy', 'Medium', 'Hard'] as const) set(`solved-${df.toLowerCase()}`, String(problems.filter((x) => x.df === df && problemStatus(x.i) === 'solved').length));
  $$('[data-overall]').forEach((e) => {
    const pct = lessons.length ? validDone.length / lessons.length : 0;
    $$('[data-ring-fg]', e).forEach((c) => { const len = 2 * Math.PI * Number(c.getAttribute('r')); c.style.strokeDasharray = String(len); c.style.strokeDashoffset = String(len * (1 - pct)); });
  });
}

function statuses() { $$('[data-status]').forEach((el) => (el.dataset.state = problemStatus(el.dataset.status!))); }

/* ---------- list filters ---------- */
const filter = { status: 'all', q: '' };
function applyFilter() {
  const rows = $$('[data-filterable]');
  if (!rows.length) return;
  const done = progress.get().done;
  rows.forEach((row) => {
    const isDoneRow = (row.dataset.item ?? row.dataset.filterKey ?? '') in done;
    row.hidden = !((filter.status === 'all' || (filter.status === 'done') === isDoneRow) && (!filter.q || (row.dataset.search || '').includes(filter.q)));
  });
  $$('[data-filter-group]').forEach((g) => (g.hidden = $$('[data-filterable]', g).every((r) => r.hidden)));
  const none = $('[data-filter-empty]'); if (none) none.hidden = rows.some((r) => !r.hidden);
}
function bindFilters() {
  $$('[data-filter-status]').forEach((b) => b.addEventListener('click', () => {
    filter.status = b.dataset.filterStatus!; $$('[data-filter-status]').forEach((x) => x.classList.toggle('on', x === b)); applyFilter();
  }));
  const q = $<HTMLInputElement>('[data-filter-q]');
  q?.addEventListener('input', () => { filter.q = q.value.trim().toLowerCase(); applyFilter(); });
}

function render() { items(); $$('[data-meter]').forEach(meter); continueLinks(); stats(); statuses(); applyFilter(); }

document.addEventListener('click', (e) => {
  const b = (e.target as HTMLElement).closest<HTMLElement>('[data-toggle]');
  if (!b) return;
  e.preventDefault(); e.stopPropagation();
  const id = b.dataset.toggle!; setDone(id, !isDone(id));
}, true);

bindFilters(); render();
progress.subscribe(render); practice.subscribe(render);
