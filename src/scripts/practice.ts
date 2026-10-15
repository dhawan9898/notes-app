// The practice problem page: function-only editor, Run / Submit against the tests, results, hints, saved submissions.
import { run, isBusy, filesFor } from './runner';
import { getDraft, setDraft, addSubmission, submissions, lastSubmission, status, type Submission } from './practice-store';
import { judge, assemble, countLines, relocateDiagnostics, highlightC, indentAfterEnter, type Judged, type RunLike } from './practice-judge';

type Data = { id: string; title: string; prefix: string; starter: string; suffix: string; solution: string; hints: string[]; runnable: boolean; next: string; nextTitle: string; note: string };
const root = document.querySelector<HTMLElement>('[data-problem]');
if (root) init(root);

function init(root: HTMLElement) {
  const data: Data = JSON.parse(document.getElementById('pp-data')!.textContent || '{}');
  const $ = <T extends HTMLElement = HTMLElement>(s: string) => root.querySelector<T>(s)!;
  const $$ = <T extends HTMLElement = HTMLElement>(s: string) => Array.from(root.querySelectorAll<T>(s));
  const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const prefixLines = countLines(data.prefix);
  document.body.classList.add('pp-page');

  /* ---------------- phone layout: one pane at a time ---------------- */
  const grid = $('.pp-grid');
  const showPane = (name: string) => {
    grid.dataset.active = name;
    $$('[data-seg]').forEach((b) => { const on = b.dataset.seg === name; b.classList.toggle('on', on); b.setAttribute('aria-selected', String(on)); });
    if (name === 'result') $('[data-pp-dot]').hidden = true;
    if (name === 'code') requestAnimationFrame(syncEditor);
    root.dataset.pane = name;
  };
  $$('[data-seg]').forEach((b) => b.addEventListener('click', () => { showPane(b.dataset.seg!); window.scrollTo({ top: 0 }); }));
  const phone = matchMedia('(max-width: 900px)');
  const reveal = (name: string) => { if (phone.matches) showPane(name); };
  showPane('desc');

  /* ---------------- description tabs ---------------- */
  const tab = (name: string) => {
    $$('[data-tab]').forEach((b) => { const on = b.dataset.tab === name; b.classList.toggle('on', on); b.setAttribute('aria-selected', String(on)); });
    $$('[data-tabpane]').forEach((p) => (p.hidden = p.dataset.tabpane !== name));
    $('.pp-scroll').scrollTop = 0;
  };
  $$('[data-tab]').forEach((b) => b.addEventListener('click', () => tab(b.dataset.tab!)));

  /* ---------------- editor ---------------- */
  const ta = $<HTMLTextAreaElement>('[data-ed-ta]'), hl = $('[data-ed-hl]'), hlPre = $('.ed-hl'), gutter = $('[data-ed-gutter]'), gutterBox = $('.ed-gutter');
  const savedLabel = $('[data-pp-saved]');
  let draftTimer: ReturnType<typeof setTimeout> | undefined;
  const code = () => ta.value;
  function paint() {
    hl.innerHTML = highlightC(ta.value) + '\n ';
    const lines = ta.value.split('\n').length;
    if (gutter.dataset.n !== String(lines)) { gutter.dataset.n = String(lines); gutter.textContent = Array.from({ length: lines }, (_, i) => i + 1).join('\n'); }
  }
  function syncEditor() { hlPre.scrollTop = ta.scrollTop; hlPre.scrollLeft = ta.scrollLeft; gutterBox.scrollTop = ta.scrollTop; }
  function setCode(text: string, announce = '') {
    ta.value = text.replace(/\n*$/, '\n'); paint(); syncEditor(); if (announce) flash(announce);
  }
  function flash(msg: string) { savedLabel.textContent = msg; savedLabel.classList.add('on'); clearTimeout(flashTimer); flashTimer = setTimeout(() => savedLabel.classList.remove('on'), 2200); }
  let flashTimer: ReturnType<typeof setTimeout> | undefined;
  const saveDraft = () => { clearTimeout(draftTimer); const ok = setDraft(data.id, code()); flash(ok ? 'Saved' : 'Could not save (storage is full or blocked)'); };
  const startText = () => data.starter.replace(/\n*$/, '\n');

  const saved = getDraft(data.id);
  setCode(saved ?? data.starter);
  if (saved !== undefined && saved.replace(/\n*$/, '\n') !== startText()) flash('Restored your saved code');

  ta.addEventListener('input', () => { paint(); syncEditor(); clearTimeout(draftTimer); savedLabel.classList.remove('on'); draftTimer = setTimeout(saveDraft, 700); });
  ta.addEventListener('scroll', syncEditor);
  addEventListener('pagehide', () => { if (draftTimer) saveDraft(); });
  document.addEventListener('visibilitychange', () => { if (document.hidden && draftTimer) saveDraft(); });

  const insert = (text: string, back = 0) => {
    const s = ta.selectionStart, e = ta.selectionEnd;
    ta.setRangeText(text, s, e, 'end'); if (back) ta.setSelectionRange(ta.selectionStart - back, ta.selectionStart - back);
    ta.dispatchEvent(new Event('input'));
  };
  const PAIRS = new Map([['(', ')'], ['[', ']'], ['{', '}'], ['"', '"'], ["'", "'"]]);
  ta.addEventListener('keydown', (e) => {
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key === 'Enter') { e.preventDefault(); (e.shiftKey ? submitBtn : runBtn).click(); return; }
    if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); saveDraft(); return; }
    if (e.key === 'Tab') {
      e.preventDefault();
      const s = ta.selectionStart, eIdx = ta.selectionEnd;
      if (s !== eIdx && ta.value.slice(s, eIdx).includes('\n')) {           // indent or outdent the selected lines
        const from = ta.value.lastIndexOf('\n', s - 1) + 1, block = ta.value.slice(from, eIdx);
        const out = block.split('\n').map((l) => (e.shiftKey ? l.replace(/^( {1,4}|\t)/, '') : '    ' + l)).join('\n');
        ta.setRangeText(out, from, eIdx, 'select'); ta.dispatchEvent(new Event('input'));
      } else if (e.shiftKey) {
        const from = ta.value.lastIndexOf('\n', s - 1) + 1, m = ta.value.slice(from, s).match(/^ {1,4}/);
        if (m) { ta.setRangeText('', from, from + m[0].length, 'preserve'); ta.dispatchEvent(new Event('input')); }
      } else insert('    ');
      return;
    }
    if (e.key === 'Enter' && !mod && !e.shiftKey && ta.selectionStart === ta.selectionEnd) {
      e.preventDefault();
      const { insert: text, caretBack } = indentAfterEnter(ta.value, ta.selectionStart);
      insert(text, caretBack); return;
    }
    if (!mod && !e.altKey && ta.selectionStart === ta.selectionEnd) {
      const pos = ta.selectionStart, next = ta.value[pos] ?? '', prev = ta.value[pos - 1] ?? '';
      if (')]}"\''.includes(e.key) && next === e.key) { e.preventDefault(); ta.setSelectionRange(pos + 1, pos + 1); return; }     // typing over the closing character
      const close = PAIRS.get(e.key);
      if (close) {
        const quote = e.key === '"' || e.key === "'";
        if (quote ? /\w/.test(prev) || /\w/.test(next) : /\w/.test(next)) return;      // an apostrophe, or a bracket in front of a word: no pair
        e.preventDefault(); insert(e.key + close, 1); return;
      }
      if (e.key === 'Backspace' && next && PAIRS.get(prev) === next) { e.preventDefault(); ta.setRangeText('', pos - 1, pos + 1, 'end'); ta.dispatchEvent(new Event('input')); }
    }
  });
  // symbol row for phones: pointerdown keeps the keyboard open and the caret where it is
  $$('[data-sym]').forEach((b) => {
    b.addEventListener('pointerdown', (e) => e.preventDefault());
    b.addEventListener('click', () => {
      ta.focus();
      const s = b.dataset.sym!;
      if (s === 'Tab') insert('    '); else if (PAIRS.has(s) && s !== '"') insert(s + PAIRS.get(s), 1); else insert(s);
    });
  });

  // reset (two taps) and the last submission
  const resetBtn = $<HTMLButtonElement>('[data-pp-reset]'), lastBtn = $<HTMLButtonElement>('[data-pp-last]');
  let resetTimer: ReturnType<typeof setTimeout> | undefined;
  resetBtn.addEventListener('click', () => {
    if (!resetBtn.classList.contains('armed')) {
      resetBtn.classList.add('armed'); resetBtn.textContent = 'Tap again to reset';
      resetTimer = setTimeout(() => { resetBtn.classList.remove('armed'); resetBtn.textContent = 'Reset'; }, 3000); return;
    }
    clearTimeout(resetTimer); resetBtn.classList.remove('armed'); resetBtn.textContent = 'Reset';
    setCode(data.starter); saveDraft(); flash('Reset to the starting code');
  });
  lastBtn.addEventListener('click', () => { const s = lastSubmission(data.id); if (s) { setCode(s.code, 'Loaded your last submission'); saveDraft(); } });

  /* ---------------- run and submit ---------------- */
  const runBtn = $<HTMLButtonElement>('[data-pp-run]'), submitBtn = $<HTMLButtonElement>('[data-pp-submit]'), statusEl = $('[data-pp-status]');
  const resBox = $('[data-pp-res]'), resEmpty = $('[data-pp-res-empty]');
  let active: { cancel: () => void } | null = null;
  const setBusy = (busy: boolean, label = '') => {
    runBtn.classList.toggle('running', busy); runBtn.querySelector('span')!.textContent = busy ? 'Stop' : 'Run';
    submitBtn.disabled = busy; statusEl.textContent = label;
  };
  if (!data.runnable) { runBtn.disabled = submitBtn.disabled = true; statusEl.textContent = 'Not runnable in the app'; }

  async function execute(submit: boolean) {
    if (active) { active.cancel(); return; }
    if (isBusy()) { statusEl.textContent = 'Another program is running…'; return; }
    const mine = code();
    if (!mine.trim()) { statusEl.textContent = 'The editor is empty'; return; }
    if (draftTimer) saveDraft();
    setBusy(true, 'Starting…');
    reveal('result');
    renderPending(submit ? 'Submitting…' : 'Running…');
    let result: RunLike | null = null, error = '', ms = 0;
    const t0 = performance.now();
    try {
      const program = assemble(data.prefix, mine, data.suffix);
      const h = run(program, { stdin: '', files: await filesFor(), onStatus: (m) => setBusy(true, m) });
      active = h;
      const r = await h.promise; result = r; ms = Math.round(r.timings?.run ?? performance.now() - t0);
    } catch (e) { error = (e as Error).message; if (error === 'Cancelled') { active = null; setBusy(false); renderPending(''); resBox.hidden = true; resEmpty.hidden = false; return; } }
    active = null; setBusy(false);
    const j = judge(result, error);
    if (j.verdict === 'Compile Error') j.message = relocateDiagnostics(j.message, prefixLines, countLines(mine));
    renderResult(j, ms, result?.diagnostics ? relocateDiagnostics(result.diagnostics, prefixLines, countLines(mine)) : '', submit);
    if (submit) {
      addSubmission(data.id, { verdict: j.verdict, passed: j.passed, total: j.total, ms, code: mine });
      flash(j.verdict === 'Accepted' ? 'Accepted: code saved' : 'Submitted: code saved');
      refreshStatus();
    }
    if (phone.matches && grid.dataset.active !== 'result') $('[data-pp-dot]').hidden = false;
  }
  runBtn.addEventListener('click', () => execute(false));
  submitBtn.addEventListener('click', () => execute(true));

  function renderPending(msg: string) {
    resEmpty.hidden = true; resBox.hidden = false;
    resBox.innerHTML = msg ? `<div class="pp-pending"><i class="pp-spin"></i><span>${esc(msg)}</span></div>` : '';
  }

  function renderResult(j: Judged, ms: number, warnings: string, submitted: boolean) {
    resEmpty.hidden = true; resBox.hidden = false;
    const cls = j.verdict === 'Accepted' ? 'ok' : 'bad';
    const sub = j.verdict === 'Compile Error' ? 'The code did not compile.'
      : j.verdict === 'Time Limit Exceeded' ? 'The program ran for more than 20 seconds. An infinite loop?'
      : j.verdict === 'Runtime Error' ? `Stopped early: ${j.passed} of ${j.total || '?'} tests passed before the crash.`
      : `${j.passed} / ${j.total} tests passed`;
    let html = `<div class="pp-verdict ${cls}"><div><strong>${j.verdict}</strong><span>${esc(sub)}${ms && j.verdict !== 'Compile Error' ? ` · ${ms} ms` : ''}</span></div>${submitted ? '<em>Saved</em>' : ''}</div>`;
    if (j.verdict === 'Accepted') {
      html += `<div class="pp-next">${data.next ? `<a class="pp-btn primary" href="${data.next}">Next: ${esc(data.nextTitle)} →</a>` : ''}<a class="pp-btn ghost" href="${data.note}">Back to the note</a></div>`;
    }
    if (j.verdict === 'Compile Error') html += `<pre class="pp-pre err">${esc(j.message.replace(/\x1b\[[0-9;]*m/g, ''))}</pre>`;
    else {
      if (j.cases.length) {
        html += `<div class="pp-cases" role="tablist" aria-label="Test cases">${j.cases.map((c, i) => `<button type="button" role="tab" class="pp-case ${c.ok ? 'ok' : 'bad'}${i === firstBad(j) ? ' on' : ''}" data-case="${i}"><i></i>Case ${i + 1}</button>`).join('')}</div><div class="pp-case-detail" data-pp-case></div>`;
      }
      if (j.message) html += `<div class="pp-sec"><span class="pp-label">${j.verdict === 'Runtime Error' ? 'Error' : 'Standard error'}</span><pre class="pp-pre err">${esc(j.message)}</pre></div>`;
      if (j.stdout) html += `<div class="pp-sec"><span class="pp-label">Your output</span><pre class="pp-pre">${esc(j.stdout)}</pre></div>`;
      const w = warnings.replace(/\x1b\[[0-9;]*m/g, '').trim();
      if (w) html += `<div class="pp-sec"><span class="pp-label">Compiler warnings</span><pre class="pp-pre warn">${esc(w)}</pre></div>`;
    }
    resBox.innerHTML = html;
    const detail = resBox.querySelector<HTMLElement>('[data-pp-case]');
    const show = (i: number) => {
      const c = j.cases[i]; if (!c || !detail) return;
      resBox.querySelectorAll('[data-case]').forEach((b) => b.classList.toggle('on', Number((b as HTMLElement).dataset.case) === i));
      detail.innerHTML = `<dl><dt>Test</dt><dd><code>${esc(c.label.trim())}</code></dd>` +
        (c.ok ? `<dt>Result</dt><dd>${c.got !== undefined ? `<code>${esc(c.got)}</code> as expected` : 'passed'}</dd>`
          : c.expected !== undefined ? `<dt>Expected</dt><dd><code>${esc(c.expected)}</code></dd><dt>Your result</dt><dd><code class="bad">${esc(c.got ?? '')}</code></dd>` : `<dt>Problem</dt><dd>${esc(c.line.replace(/^FAIL /, ''))}</dd>`) + '</dl>';
    };
    resBox.querySelectorAll<HTMLElement>('[data-case]').forEach((b) => b.addEventListener('click', () => show(Number(b.dataset.case))));
    if (j.cases.length) show(Math.max(0, firstBad(j)));
    resBox.scrollTop = 0;
  }
  const firstBad = (j: Judged) => { const i = j.cases.findIndex((c) => !c.ok); return i < 0 ? 0 : i; };

  /* ---------------- hints, solution, submissions, status ---------------- */
  let shown = 0;
  const hintList = $('[data-pp-hints]'), hintBtn = $<HTMLButtonElement>('[data-pp-hint-btn]');
  hintBtn.addEventListener('click', () => {
    if (shown >= data.hints.length) return;
    const li = document.createElement('li'); li.innerHTML = esc(data.hints[shown]).replace(/`([^`]+)`/g, '<code>$1</code>'); hintList.append(li); shown++;
    hintBtn.hidden = shown >= data.hints.length; hintBtn.textContent = `Show hint ${shown + 1} of ${data.hints.length}`;
  });
  $('[data-pp-sol-btn]').addEventListener('click', () => {
    $('[data-pp-sol-code]').innerHTML = highlightC(data.solution);
    $('[data-pp-sol-gate]').hidden = true; $('[data-pp-sol]').hidden = false;
  });

  const fmtDate = (ts: number) => new Date(ts).toLocaleString(undefined, { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
  function renderSubs() {
    const subs = submissions(data.id), list = $('[data-pp-subs]');
    $('[data-pp-subs-empty]').hidden = subs.length > 0;
    const count = $('[data-pp-subcount]'); count.hidden = !subs.length; count.textContent = String(subs.length);
    lastBtn.hidden = !subs.length;
    list.innerHTML = subs.map((s: Submission, i: number) => `<li class="${s.verdict === 'Accepted' ? 'ok' : 'bad'}"><div><strong>${s.verdict}</strong><span>${s.passed}/${s.total} tests · ${s.ms} ms · ${fmtDate(s.ts)}</span></div><button type="button" class="pp-mini" data-load="${i}">Load</button></li>`).join('');
    list.querySelectorAll<HTMLElement>('[data-load]').forEach((b) => b.addEventListener('click', () => {
      const s = subs[Number(b.dataset.load)]; if (!s) return;
      setCode(s.code, 'Loaded a previous submission'); saveDraft(); reveal('code'); ta.focus();
    }));
  }
  function refreshStatus() { $('[data-pp-solved]').hidden = status(data.id) !== 'solved'; renderSubs(); }
  refreshStatus();
  window.addEventListener('dn:practice', refreshStatus);

  // The page is a fixed-height app screen: height = what is visible below the site header. When the on-screen keyboard opens the
  // visual viewport shrinks, so the editor and the Run / Submit bar stay above the keyboard instead of being covered by it.
  const vv = window.visualViewport, header = document.querySelector<HTMLElement>('header.top');
  const fit = () => {
    const h = vv?.height ?? innerHeight, top = header ? Math.max(0, header.getBoundingClientRect().bottom) : 0;
    root.style.setProperty('--pp-h', `${Math.max(240, Math.round(h - Math.max(0, Math.min(top, 120))))}px`);
    root.classList.toggle('kb-open', phone.matches && !!vv && innerHeight - vv.height > 140);
    if (scrollY) scrollTo(0, 0);
  };
  vv?.addEventListener('resize', fit); addEventListener('resize', fit); addEventListener('orientationchange', fit); fit();
}
