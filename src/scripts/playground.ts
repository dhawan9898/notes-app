// The C playground: the shared editor, a list of examples, Run with standard input, and the code kept in this browser.
import { createEditor } from './lib/editor';
import { createStore, isObj } from './lib/store';
import { $, $$ } from './lib/ui';

type Example = { title: string; blurb: string; code: string; stdin: string };
type Saved = { code: string; stdin: string };
const examples: Example[] = JSON.parse(document.getElementById('pg-data')!.textContent || '[]');
const store = createStore<Saved & { example: number }>({
  key: 'dn:playground', version: 1, empty: () => ({ code: examples[0]?.code ?? '', stdin: examples[0]?.stdin ?? '', example: 0 }),
  check: (d): d is Saved & { example: number } => isObj(d) && typeof d.code === 'string' && typeof d.stdin === 'string' && typeof d.example === 'number',
  legacy: { key: 'deepnotes:playground:v1', convert: (r) => (isObj(r) && typeof r.code === 'string' ? { code: r.code, stdin: String(r.stdin ?? ''), example: 0 } : null) },
});
const root = $('[data-playground]')!;
const stdin = $<HTMLTextAreaElement>('[data-pg-stdin]')!, out = $('[data-pg-out]')!, status = $('[data-pg-status]')!, runBtn = $<HTMLButtonElement>('[data-pg-run]')!;
const mark = (i: number) => $$('[data-ex]').forEach((b) => b.classList.toggle('on', Number(b.dataset.ex) === i));
const ed = createEditor(root, {
  symbols: $('.symbols'),
  onInput: () => store.update((s) => { s.code = ed.get(); }, { defer: true }),
  onRun: () => runBtn.click(),
});
const s0 = store.get();
ed.set(s0.code); stdin.value = s0.stdin; mark(s0.example);
stdin.addEventListener('input', () => store.update((s) => { s.stdin = stdin.value; }, { defer: true }));
const load = (i: number) => { const e = examples[i]; if (!e) return; ed.set(e.code); stdin.value = e.stdin; out.hidden = true; mark(i); store.update((s) => { s.code = e.code; s.stdin = e.stdin; s.example = i; }); };
$$('[data-ex]').forEach((b) => b.addEventListener('click', () => load(Number(b.dataset.ex))));
$('[data-pg-reset]')!.addEventListener('click', () => load(store.get().example));

let active: { cancel: () => void } | null = null;
runBtn.addEventListener('click', async () => {
  if (active) { active.cancel(); return; }
  const r = await import('./runner');
  if (r.isBusy()) return;
  runBtn.classList.add('running'); runBtn.querySelector('span')!.textContent = 'Stop'; status.textContent = 'Starting…'; out.hidden = true;
  try { active = r.run(ed.get(), { stdin: stdin.value, onStatus: (m) => (status.textContent = m) }); r.renderResult(out, await active.promise); }
  catch (e) { if ((e as Error).message !== 'Cancelled') r.renderError(out, (e as Error).message); }
  finally { active = null; runBtn.classList.remove('running'); runBtn.querySelector('span')!.textContent = 'Run'; status.textContent = ''; }
});
