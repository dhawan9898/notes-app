// The problem page: code problems get a function-only editor with Run / Submit; questions get a notes box and self-assessment.
// Both share the frame: tabs, hints, solution, saved state, and the phone layout.
import { getDraft, setDraft, addSubmission, submissions, lastSubmission, status, setSolved, practice, type Submission } from './lib/practice';
import { judge, assemble, countLines, relocateDiagnostics, highlightC, type Judged, type RunLike } from './lib/judge';
import { createEditor } from './lib/editor';
import { toast, go } from './lib/ui';

type Data = { id: string; kind: 'code' | 'question'; title: string; hints: string[]; next: string; nextTitle: string; lesson: string; prefix?: string; starter?: string; suffix?: string; solution?: string };
const root = document.querySelector<HTMLElement>('[data-problem]');
if (root) init(root);

function init(root: HTMLElement) {
  const data: Data = JSON.parse(document.getElementById('pp-data')!.textContent || '{}');
  const $ = <T extends HTMLElement = HTMLElement>(s: string) => root.querySelector<T>(s)!;
  const $$ = <T extends HTMLElement = HTMLElement>(s: string) => Array.from(root.querySelectorAll<T>(s));
  const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const isCode = data.kind === 'code';
  document.body.classList.add('pp-page');
  let syncEditor: () => void = () => {};                 // replaced once the editor exists

  /* ---------------- phone layout: one pane at a time ---------------- */
  const grid = $('.pp-grid'), phone = matchMedia('(max-width: 900px)');
  const showPane = (name: string) => {
    grid.dataset.active = name;
    $$('[data-seg]').forEach((b) => { const on = b.dataset.seg === name; b.classList.toggle('on', on); b.setAttribute('aria-selected', String(on)); });
    if (name === 'result') $('[data-pp-dot]')?.setAttribute('hidden', '');
    root.dataset.pane = name;
    if (name === 'work') requestAnimationFrame(syncEditor);
  };
  $$('[data-seg]').forEach((b) => b.addEventListener('click', () => showPane(b.dataset.seg!)));
  const reveal = (name: string) => { if (phone.matches) showPane(name); };
  showPane('desc');

  /* ---------------- description tabs ---------------- */
  const tab = (name: string) => {
    $$('[data-tab]').forEach((b) => { const on = b.dataset.tab === name; b.classList.toggle('on', on); b.setAttribute('aria-selected', String(on)); });
    $$('[data-tabpane]').forEach((p) => (p.hidden = p.dataset.tabpane !== name));
    $('.pp-scroll').scrollTop = 0;
  };
  $$('[data-tab]').forEach((b) => b.addEventListener('click', () => tab(b.dataset.tab!)));

  /* ---------------- saved label ---------------- */
  const savedLabel = $('[data-pp-saved]');
  let flashTimer: ReturnType<typeof setTimeout> | undefined, draftTimer: ReturnType<typeof setTimeout> | undefined;
  const flash = (msg: string) => { savedLabel.textContent = msg; savedLabel.classList.add('on'); clearTimeout(flashTimer); flashTimer = setTimeout(() => savedLabel.classList.remove('on'), 2200); };
  const statusEl = $('[data-pp-status]');

  /* ---------------- hints and solution ---------------- */
  let shown = 0;
  const hintList = $('[data-pp-hints]'), hintBtn = $<HTMLButtonElement>('[data-pp-hint-btn]');
  hintBtn?.addEventListener('click', () => {
    if (shown >= data.hints.length) return;
    const li = document.createElement('li'); li.innerHTML = data.hints[shown]; hintList.append(li); shown++;
    hintBtn.hidden = shown >= data.hints.length; hintBtn.textContent = `Show hint ${shown + 1} of ${data.hints.length}`;
  });
  $('[data-pp-sol-btn]').addEventListener('click', () => {
    if (isCode && data.solution) $('[data-pp-sol-code]').innerHTML = highlightC(data.solution);
    $('[data-pp-sol-gate]').hidden = true; $('[data-pp-sol]').hidden = false;
  });
  const refreshSolved = () => { $('[data-pp-solved]').hidden = status(data.id) !== 'solved'; };

  if (!isCode) { initQuestion(); } else { initCode(); }
  refreshSolved(); practice.subscribe(refreshSolved);

  /* ================================================================ question ================================================================ */
  function initQuestion() {
    const ta = $<HTMLTextAreaElement>('[data-notes]');
    ta.value = getDraft(data.id) ?? '';
    ta.addEventListener('input', () => { clearTimeout(draftTimer); savedLabel.classList.remove('on'); draftTimer = setTimeout(() => { flash(setDraft(data.id, ta.value) ? 'Saved' : 'Could not save'); draftTimer = undefined; }, 700); });
    const save = () => { if (draftTimer) { clearTimeout(draftTimer); draftTimer = undefined; setDraft(data.id, ta.value); } };
    addEventListener('pagehide', save); document.addEventListener('visibilitychange', () => { if (document.hidden) save(); });
    $('[data-q-got]').addEventListener('click', () => {
      save(); setSolved(data.id, true);
      toast('Marked as solved', data.next ? { label: 'Next problem →', run: () => go(data.next) } : undefined);
    });
    $('[data-q-review]').addEventListener('click', () => { setSolved(data.id, false); tab('solution'); reveal('desc'); toast('Marked for review'); });
    statusEl.textContent = '';
  }

  /* ================================================================ code ================================================================ */
  function initCode() {
    const prefix = data.prefix!, suffix = data.suffix!, starter = data.starter!;
    const prefixLines = countLines(prefix);
    const ed = createEditor(root, { symbols: root.querySelector('.symbols'), onInput: () => { clearTimeout(draftTimer); savedLabel.classList.remove('on'); draftTimer = setTimeout(saveDraft, 700); }, onRun: () => runBtn.click(), onSubmit: () => submitBtn.click(), onSave: () => saveDraft() });
    const code = () => ed.get();
    syncEditor = ed.sync;
    const setCodeText = (text: string, announce = '') => { ed.set(text); if (announce) flash(announce); };
    const saveDraft = () => { clearTimeout(draftTimer); draftTimer = undefined; flash(setDraft(data.id, code()) ? 'Saved' : 'Could not save (storage is full or blocked)'); };
    const normal = (s: string) => s.replace(/\n*$/, '\n');

    const saved = getDraft(data.id);
    setCodeText(saved ?? starter);
    if (saved !== undefined && normal(saved) !== normal(starter)) flash('Restored your saved code');
    addEventListener('pagehide', () => { if (draftTimer) saveDraft(); });
    document.addEventListener('visibilitychange', () => { if (document.hidden && draftTimer) saveDraft(); });

    const resetBtn = $<HTMLButtonElement>('[data-pp-reset]'), lastBtn = $<HTMLButtonElement>('[data-pp-last]');
    let resetTimer: ReturnType<typeof setTimeout> | undefined;
    resetBtn.addEventListener('click', () => {
      if (!resetBtn.classList.contains('armed')) {
        resetBtn.classList.add('armed'); resetBtn.textContent = 'Tap again to reset';
        resetTimer = setTimeout(() => { resetBtn.classList.remove('armed'); resetBtn.textContent = 'Reset'; }, 3000); return;
      }
      clearTimeout(resetTimer); resetBtn.classList.remove('armed'); resetBtn.textContent = 'Reset';
      setCodeText(starter); saveDraft(); flash('Reset to the starting code');
    });
    lastBtn.addEventListener('click', () => { const s = lastSubmission(data.id); if (s) { setCodeText(s.code, 'Loaded your last submission'); saveDraft(); } });

    /* ---------- run and submit ---------- */
    const runBtn = $<HTMLButtonElement>('[data-pp-run]'), submitBtn = $<HTMLButtonElement>('[data-pp-submit]');
    const resBox = $('[data-pp-res]'), resEmpty = $('[data-pp-res-empty]');
    let active: { cancel: () => void } | null = null;
    const setBusy = (busy: boolean, label = '') => { runBtn.classList.toggle('running', busy); runBtn.querySelector('span')!.textContent = busy ? 'Stop' : 'Run'; submitBtn.disabled = busy; statusEl.textContent = label; };

    async function execute(submit: boolean) {
      if (active) { active.cancel(); return; }
      const mine = code();
      if (!mine.trim()) { statusEl.textContent = 'The editor is empty'; return; }
      const runner = await import('./runner');
      if (runner.isBusy()) { statusEl.textContent = 'Another program is running…'; return; }
      if (draftTimer) saveDraft();
      setBusy(true, 'Starting…'); reveal('result'); pending(submit ? 'Submitting…' : 'Running…');
      let result: RunLike | null = null, error = '', ms = 0;
      const t0 = performance.now();
      try {
        const h = runner.run(assemble(prefix, mine, suffix), { stdin: '', files: await runner.filesFor(), onStatus: (m) => setBusy(true, m) });
        active = h; const r = await h.promise; result = r; ms = Math.round(r.timings?.run ?? performance.now() - t0);
      } catch (e) {
        error = (e as Error).message;
        if (error === 'Cancelled') { active = null; setBusy(false); resBox.hidden = true; resEmpty.hidden = false; return; }
      }
      active = null; setBusy(false);
      const j = judge(result, error), lines = countLines(mine);
      if (j.verdict === 'Compile Error') j.message = relocateDiagnostics(j.message, prefixLines, lines);
      render(j, ms, result?.diagnostics ? relocateDiagnostics(result.diagnostics, prefixLines, lines) : '', submit);
      if (submit) {
        addSubmission(data.id, { verdict: j.verdict, passed: j.passed, total: j.total, ms, code: mine });
        flash(j.verdict === 'Accepted' ? 'Accepted: code saved' : 'Submitted: code saved');
      }
      if (phone.matches && grid.dataset.active !== 'result') $('[data-pp-dot]').hidden = false;
    }
    runBtn.addEventListener('click', () => execute(false));
    submitBtn.addEventListener('click', () => execute(true));

    const pending = (msg: string) => { resEmpty.hidden = true; resBox.hidden = false; resBox.innerHTML = `<div class="pending"><i class="spin"></i><span>${esc(msg)}</span></div>`; };
    const firstBad = (j: Judged) => Math.max(0, j.cases.findIndex((c) => !c.ok));
    function render(j: Judged, ms: number, warnings: string, submitted: boolean) {
      resEmpty.hidden = true; resBox.hidden = false;
      const ok = j.verdict === 'Accepted';
      const sub = j.verdict === 'Compile Error' ? 'The code did not compile.'
        : j.verdict === 'Time Limit Exceeded' ? 'The program ran for more than 20 seconds. An infinite loop?'
        : j.verdict === 'Runtime Error' ? `Stopped early: ${j.passed} of ${j.total || '?'} tests passed before the crash.`
        : `${j.passed} / ${j.total} tests passed`;
      let html = `<div class="verdict ${ok ? 'ok' : 'bad'}"><div><strong>${j.verdict}</strong><span>${esc(sub)}${ms && j.verdict !== 'Compile Error' ? ` · ${ms} ms` : ''}</span></div>${submitted ? '<em>Saved</em>' : ''}</div>`;
      if (ok) html += `<div class="next-row">${data.next ? `<a class="btn primary" href="${data.next}">Next: ${esc(data.nextTitle)} →</a>` : ''}<a class="btn" href="${data.lesson}">Back to the module</a></div>`;
      if (j.verdict === 'Compile Error') html += `<pre class="out err">${esc(j.message.replace(/\x1b\[[0-9;]*m/g, ''))}</pre>`;
      else {
        if (j.cases.length) html += `<div class="cases" role="tablist" aria-label="Test cases">${j.cases.map((c, i) => `<button type="button" role="tab" class="case ${c.ok ? 'ok' : 'bad'}${i === firstBad(j) ? ' on' : ''}" data-case="${i}"><i></i>Case ${i + 1}</button>`).join('')}</div><div class="case-detail" data-pp-case></div>`;
        if (j.message) html += `<div class="sec"><span class="label">${j.verdict === 'Runtime Error' ? 'Error' : 'Standard error'}</span><pre class="out err">${esc(j.message)}</pre></div>`;
        if (j.stdout) html += `<div class="sec"><span class="label">Your output</span><pre class="out">${esc(j.stdout)}</pre></div>`;
        const w = warnings.replace(/\x1b\[[0-9;]*m/g, '').trim();
        if (w) html += `<div class="sec"><span class="label">Compiler warnings</span><pre class="out warn">${esc(w)}</pre></div>`;
      }
      resBox.innerHTML = html;
      const detail = resBox.querySelector<HTMLElement>('[data-pp-case]');
      const show = (i: number) => {
        const c = j.cases[i]; if (!c || !detail) return;
        resBox.querySelectorAll('[data-case]').forEach((b) => b.classList.toggle('on', Number((b as HTMLElement).dataset.case) === i));
        detail.innerHTML = `<dl><dt>Test</dt><dd><code>${esc(c.label.trim())}</code></dd>` + (c.ok
          ? `<dt>Result</dt><dd>${c.got !== undefined ? `<code>${esc(c.got)}</code> as expected` : 'passed'}</dd>`
          : c.expected !== undefined ? `<dt>Expected</dt><dd><code>${esc(c.expected)}</code></dd><dt>Your result</dt><dd><code class="bad">${esc(c.got ?? '')}</code></dd>` : `<dt>Problem</dt><dd>${esc(c.line.replace(/^FAIL /, ''))}</dd>`) + '</dl>';
      };
      resBox.querySelectorAll<HTMLElement>('[data-case]').forEach((b) => b.addEventListener('click', () => show(Number(b.dataset.case))));
      if (j.cases.length) show(firstBad(j));
      resBox.parentElement!.scrollTop = 0;
    }

    /* ---------- submissions tab ---------- */
    const fmtDate = (ts: number) => new Date(ts).toLocaleString(undefined, { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
    function renderSubs() {
      const subs = submissions(data.id), list = $('[data-pp-subs]');
      $('[data-pp-subs-empty]').hidden = subs.length > 0;
      const count = $('[data-pp-subcount]'); count.hidden = !subs.length; count.textContent = String(subs.length);
      lastBtn.hidden = !subs.length;
      list.innerHTML = subs.map((s: Submission, i: number) => `<li class="${s.verdict === 'Accepted' ? 'ok' : 'bad'}"><div><strong>${s.verdict}</strong><span>${s.passed}/${s.total} tests · ${s.ms} ms · ${fmtDate(s.ts)}</span></div><button type="button" class="mini" data-load="${i}">Load</button></li>`).join('');
      list.querySelectorAll<HTMLElement>('[data-load]').forEach((b) => b.addEventListener('click', () => {
        const s = subs[Number(b.dataset.load)]; if (!s) return;
        setCodeText(s.code, 'Loaded a previous submission'); saveDraft(); reveal('work'); ed.focus();
      }));
    }
    renderSubs(); practice.subscribe(renderSubs);
  }

  /* ---------------- fixed-height screen that follows the visual viewport (keyboard-safe) ---------------- */
  const vv = window.visualViewport, header = document.querySelector<HTMLElement>('header.topbar');
  const fit = () => {
    const h = vv?.height ?? innerHeight, top = header && getComputedStyle(header).position !== 'static' ? Math.min(120, header.getBoundingClientRect().height) : 0;
    root.style.setProperty('--pp-h', `${Math.max(240, Math.round(h - top))}px`);
    root.classList.toggle('kb-open', phone.matches && !!vv && innerHeight - vv.height > 140);
    if (scrollY) scrollTo(0, 0);
  };
  vv?.addEventListener('resize', fit); addEventListener('resize', fit); addEventListener('orientationchange', fit); fit();
}
