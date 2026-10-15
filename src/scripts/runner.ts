// Run buttons for C code blocks and the playground. The compiler lives in a Web Worker (public/runner/worker.js).
const baseMeta = document.querySelector<HTMLMetaElement>('meta[name="dn-base"]');
const BASE = new URL(baseMeta?.content || '/', location.href).href;
const RUNNER = new URL('runner/', BASE).href;
const RUN_LIMIT_MS = 20000;

export type RunResult = { ok: boolean; phase: string; diagnostics: string; stdout: string; stderr: string; exitCode: number | null; timings: Record<string, number> };

let worker: Worker | null = null, nextId = 1, busy = false;
let bundles: Promise<Record<string, Record<string, string>>> | null = null;

/** The compiler resolves quoted includes in the same directory only, so "../x/foo.h" becomes "foo.h". */
export const flattenIncludes = (code: string) => code.replace(/^(\s*#\s*include\s+")(?:[^"]*\/)?([^"\/]+)(")/gm, '$1$2$3');

export async function filesFor(hash?: string): Promise<Record<string, string>> {
  if (!hash) return {};
  bundles ||= fetch(RUNNER + 'bundles.json').then((r) => (r.ok ? r.json() : {})).catch(() => ({}));
  return (await bundles)[hash] || {};
}

export function run(source: string, opts: { stdin?: string; files?: Record<string, string>; onStatus?: (m: string) => void }): { promise: Promise<RunResult>; cancel: () => void } {
  if (!worker) worker = new Worker(RUNNER + 'worker.js');
  const w = worker, id = nextId++;
  let timer: ReturnType<typeof setTimeout> | undefined;
  let settle: (r: RunResult) => void = () => {}, fail: (e: Error) => void = () => {};
  const promise = new Promise<RunResult>((res, rej) => { settle = res; fail = rej; });
  const done = () => { clearTimeout(timer); w.removeEventListener('message', onMsg); busy = false; };
  const onMsg = (e: MessageEvent) => {
    if (e.data.id !== id) return;
    if (e.data.status) {
      opts.onStatus?.(e.data.status);
      if (e.data.status === 'Running…') { clearTimeout(timer); timer = setTimeout(() => { kill(); fail(new Error(`Stopped: the program ran longer than ${RUN_LIMIT_MS / 1000} seconds (infinite loop?)`)); }, RUN_LIMIT_MS); }
    } else if (e.data.error) { done(); fail(new Error(e.data.error)); }
    else if (e.data.result) { done(); settle(e.data.result); }
  };
  const kill = () => { done(); w.terminate(); if (worker === w) worker = null; };
  w.addEventListener('message', onMsg);
  busy = true;
  w.postMessage({ id, source: flattenIncludes(source), stdin: opts.stdin || '', files: opts.files || {} });
  return { promise, cancel: () => { kill(); fail(new Error('Cancelled')); } };
}
export const isBusy = () => busy;

/* ---------- UI shared by code blocks and the playground ---------- */
const ansi = /\x1b\[[0-9;]*m/g;
const esc = (s: string) => s.replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]!));
const fmt = (ms?: number) => (ms === undefined ? '' : ms < 1000 ? `${Math.round(ms)} ms` : `${(ms / 1000).toFixed(1)} s`);

export function renderResult(out: HTMLElement, r: RunResult) {
  const parts: string[] = [];
  const diag = r.diagnostics.replace(ansi, '').trim();
  if (diag) parts.push(`<div class="ro-sec ${r.phase === 'run' ? 'warn' : 'err'}"><span class="ro-label">${r.phase === 'run' ? 'Compiler warnings' : 'Compiler output'}</span><pre>${esc(diag)}</pre></div>`);
  if (r.phase === 'run') {
    parts.push(`<div class="ro-sec"><span class="ro-label">Output</span><pre>${r.stdout ? esc(r.stdout) : '<i class="muted">(no output)</i>'}</pre></div>`);
    if (r.stderr.trim()) parts.push(`<div class="ro-sec err"><span class="ro-label">Standard error</span><pre>${esc(r.stderr)}</pre></div>`);
    const t = r.timings;
    parts.push(`<div class="ro-foot">exit code ${r.exitCode} · compile ${fmt(t.compile)} · link ${fmt(t.link)} · run ${fmt(t.run)}</div>`);
  }
  out.innerHTML = parts.join(''); out.hidden = false;
}
export function renderError(out: HTMLElement, msg: string) { out.innerHTML = `<div class="ro-sec err"><span class="ro-label">Could not run</span><pre>${esc(msg)}</pre></div>`; out.hidden = false; }

const needsInput = (src: string) => /\b(scanf|getchar|fgets|getline|fread|gets|getc|fscanf)\s*\(/.test(src);

/** A fragment (no main) opened for editing becomes a program to complete: the usual headers if it has none, then
 *  either the fragment above a main() (when it defines functions, types or macros) or the fragment inside main()
 *  (when it is only statements and local declarations, which cannot stand at file scope). */
export const completeFragment = (code: string) => {
  const head = /^\s*#\s*include\b/m.test(code) ? '' : '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n';
  const topLevel = /^\s*#\s*(define|if|include)|^(typedef|struct|union|enum)\b|^[A-Za-z_][\w\s*]*\([^;]*\)\s*(\{|$)/m.test(code);
  if (topLevel) return head + code + '\n\nint main(void) {\n    // Use the code above here, then press Run.\n    return 0;\n}\n';
  const body = code.split('\n').map((l) => (l ? '    ' + l : l)).join('\n');
  return head + 'int main(void) {\n' + body + '\n    return 0;\n}\n';
};

/** Inline Run / Edit UI. A fragment starts read-only with an "Edit and run" button and stays on the page (no navigation). */
function hydrateBlock(pre: HTMLElement, fragment = false) {
  if (pre.dataset.hydrated) return; pre.dataset.hydrated = '1';
  const hash = pre.dataset.runnable;
  const original = (pre.querySelector('code')?.textContent ?? pre.textContent ?? '').replace(/\n$/, '');
  const startText = fragment ? completeFragment(original) : original;
  const box = document.createElement('div'); box.className = 'runbox';
  pre.replaceWith(box); box.append(pre);
  const bar = document.createElement('div'); bar.className = 'run-bar';
  bar.innerHTML = `<button class="run-btn" type="button"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M8 5v14l11-7z"/></svg><span>Run</span></button><button class="run-edit" type="button" title="Change the code, then press Run">✎ Edit code</button><button class="run-reset" type="button" hidden>Reset</button><span class="run-status" aria-live="polite"></span>`;
  const out = document.createElement('div'); out.className = 'run-out'; out.hidden = true; out.setAttribute('aria-live', 'polite');
  const stdin = document.createElement('textarea'); stdin.className = 'run-stdin'; stdin.placeholder = 'Input for the program (standard input)'; stdin.rows = 2; stdin.hidden = true;
  box.append(bar, stdin, out);
  const runBtn = bar.querySelector<HTMLButtonElement>('.run-btn')!, editBtn = bar.querySelector<HTMLButtonElement>('.run-edit')!, resetBtn = bar.querySelector<HTMLButtonElement>('.run-reset')!, status = bar.querySelector<HTMLElement>('.run-status')!;
  let editor: HTMLTextAreaElement | null = null, active: { cancel: () => void } | null = null;
  if (needsInput(original)) stdin.hidden = false;
  if (fragment) {
    runBtn.hidden = true; editBtn.textContent = '✎ Edit and run'; editBtn.title = 'Complete this fragment into a program and run it here';
    status.textContent = 'Fragment: add a main() to run it';
  }

  const openEditor = () => {
    if (editor) { editor.focus(); return; }
    editor = document.createElement('textarea'); editor.className = 'run-editor'; editor.spellcheck = false; editor.value = startText;
    editor.setAttribute('autocapitalize', 'off'); editor.setAttribute('autocorrect', 'off'); editor.setAttribute('aria-label', 'Editable C source');
    editor.rows = Math.min(40, startText.split('\n').length + 1);
    editor.addEventListener('keydown', (e) => {
      if (e.key === 'Tab') { e.preventDefault(); const s = editor!.selectionStart; editor!.setRangeText('    ', s, editor!.selectionEnd, 'end'); }
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') { e.preventDefault(); runBtn.click(); }
    });
    editor.addEventListener('input', () => { stdin.hidden = !needsInput(editor!.value); });
    pre.hidden = true; box.insertBefore(editor, bar); editBtn.hidden = true; resetBtn.hidden = false; runBtn.hidden = false; if (fragment) status.textContent = ''; editor.focus();
  };
  editBtn.addEventListener('click', openEditor);
  pre.addEventListener('dblclick', openEditor);
  resetBtn.addEventListener('click', () => {
    editor?.remove(); editor = null; pre.hidden = false; editBtn.hidden = false; resetBtn.hidden = true; stdin.hidden = !needsInput(original); out.hidden = true;
    if (fragment) { runBtn.hidden = true; status.textContent = 'Fragment: add a main() to run it'; }
  });
  runBtn.addEventListener('click', async () => {
    if (active) { active.cancel(); return; }
    if (isBusy()) { status.textContent = 'Another program is running…'; return; }
    const src = editor ? editor.value : original;
    runBtn.classList.add('running'); runBtn.querySelector('span')!.textContent = 'Stop'; status.textContent = 'Starting…'; out.hidden = true;
    try {
      const files = await filesFor(hash);
      active = run(src, { stdin: stdin.value, files, onStatus: (m) => (status.textContent = m) });
      renderResult(out, await active.promise);
    } catch (e) { if ((e as Error).message !== 'Cancelled') renderError(out, (e as Error).message); }
    finally { active = null; runBtn.classList.remove('running'); runBtn.querySelector('span')!.textContent = 'Run'; status.textContent = ''; }
  });
}

/** C blocks that cannot run in the app as they stand. Fragments (no main) are completed and run in place, inside the note;
 *  whole programs that failed the validator need things the in-app compiler lacks (threads, sockets, several source files,
 *  a real OS), so they are labelled instead. */
function offerFragment(pre: HTMLElement) {
  const code = (pre.querySelector('code')?.textContent ?? pre.textContent ?? '');
  if (!/\bmain\s*\(/.test(code)) { hydrateBlock(pre, true); return; }
  if (pre.dataset.hydrated) return; pre.dataset.hydrated = '1';
  const box = document.createElement('div'); box.className = 'runbox'; pre.replaceWith(box); box.append(pre);
  const bar = document.createElement('div'); bar.className = 'run-bar';
  bar.innerHTML = `<span class="run-status">Complete program, but it needs more than the in-app compiler offers (several files, threads, sockets or the real system). Compile it on your computer.</span>`;
  box.append(bar);
}

export function initRunnableBlocks() {
  document.querySelectorAll<HTMLElement>('pre[data-runnable]').forEach(hydrateBlock);
  document.querySelectorAll<HTMLElement>('article pre[data-language="c"]:not([data-runnable])').forEach(offerFragment);
}
initRunnableBlocks();
