// The code editor used by problems and the playground: a transparent textarea over a syntax-highlighted layer, with line numbers,
// auto-indent, bracket pairing, block indent with Tab, and an optional on-screen symbol row for phones.
import { highlightC, indentAfterEnter } from './judge';

export interface Editor { ta: HTMLTextAreaElement; get(): string; set(text: string): void; sync(): void; insert(text: string, back?: number): void; focus(): void }
export interface Options { symbols?: ParentNode | null; onInput?: () => void; onRun?: () => void; onSubmit?: () => void; onSave?: () => void }
const PAIRS = new Map([['(', ')'], ['[', ']'], ['{', '}'], ['"', '"'], ["'", "'"]]);

export function createEditor(root: ParentNode, o: Options = {}): Editor {
  const q = <T extends HTMLElement>(s: string) => root.querySelector<T>(s)!;
  const ta = q<HTMLTextAreaElement>('[data-ed-ta]'), hl = q('[data-ed-hl]'), hlPre = q('.ed-hl'), gutter = q('[data-ed-gutter]'), gutterBox = q('.ed-gutter');
  const paint = () => {
    hl.innerHTML = highlightC(ta.value) + '\n ';
    const lines = ta.value.split('\n').length;
    if (gutter.dataset.n !== String(lines)) { gutter.dataset.n = String(lines); gutter.textContent = Array.from({ length: lines }, (_, i) => i + 1).join('\n'); }
  };
  const sync = () => { hlPre.scrollTop = ta.scrollTop; hlPre.scrollLeft = ta.scrollLeft; gutterBox.scrollTop = ta.scrollTop; };
  const changed = () => { paint(); sync(); o.onInput?.(); };
  const insert = (text: string, back = 0) => {
    ta.setRangeText(text, ta.selectionStart, ta.selectionEnd, 'end');
    if (back) ta.setSelectionRange(ta.selectionStart - back, ta.selectionStart - back);
    ta.dispatchEvent(new Event('input'));
  };

  ta.addEventListener('input', changed);
  ta.addEventListener('scroll', sync);
  ta.addEventListener('keydown', (e) => {
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key === 'Enter') { e.preventDefault(); (e.shiftKey ? o.onSubmit ?? o.onRun : o.onRun)?.(); return; }
    if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); o.onSave?.(); return; }
    if (e.key === 'Tab') {
      e.preventDefault();
      const s = ta.selectionStart, end = ta.selectionEnd;
      if (s !== end && ta.value.slice(s, end).includes('\n')) {                     // indent or outdent the selected lines
        const from = ta.value.lastIndexOf('\n', s - 1) + 1, block = ta.value.slice(from, end);
        ta.setRangeText(block.split('\n').map((l) => (e.shiftKey ? l.replace(/^( {1,4}|\t)/, '') : '    ' + l)).join('\n'), from, end, 'select');
        ta.dispatchEvent(new Event('input'));
      } else if (e.shiftKey) {
        const from = ta.value.lastIndexOf('\n', s - 1) + 1, m = ta.value.slice(from, s).match(/^ {1,4}/);
        if (m) { ta.setRangeText('', from, from + m[0].length, 'preserve'); ta.dispatchEvent(new Event('input')); }
      } else insert('    ');
      return;
    }
    if (e.key === 'Enter' && !mod && !e.shiftKey && ta.selectionStart === ta.selectionEnd) { e.preventDefault(); const r = indentAfterEnter(ta.value, ta.selectionStart); insert(r.insert, r.caretBack); return; }
    if (!mod && !e.altKey && ta.selectionStart === ta.selectionEnd) {
      const pos = ta.selectionStart, next = ta.value[pos] ?? '', prev = ta.value[pos - 1] ?? '';
      if (')]}"\''.includes(e.key) && next === e.key) { e.preventDefault(); ta.setSelectionRange(pos + 1, pos + 1); return; }   // type over a closing character
      const close = PAIRS.get(e.key);
      if (close) {
        const quote = e.key === '"' || e.key === "'";
        if (quote ? /\w/.test(prev) || /\w/.test(next) : /\w/.test(next)) return;                                           // an apostrophe, or a bracket before a word
        e.preventDefault(); insert(e.key + close, 1); return;
      }
      if (e.key === 'Backspace' && next && PAIRS.get(prev) === next) { e.preventDefault(); ta.setRangeText('', pos - 1, pos + 1, 'end'); ta.dispatchEvent(new Event('input')); }
    }
  });

  // the symbol row: pointerdown keeps the on-screen keyboard open and the caret where it is
  o.symbols?.querySelectorAll<HTMLElement>('[data-sym]').forEach((b) => {
    b.addEventListener('pointerdown', (e) => e.preventDefault());
    b.addEventListener('click', () => { ta.focus(); const s = b.dataset.sym!; if (s === 'Tab') insert('    '); else if (PAIRS.has(s) && s !== '"') insert(s + PAIRS.get(s), 1); else insert(s); });
  });

  return { ta, get: () => ta.value, set(text) { ta.value = text.replace(/\n*$/, '\n'); paint(); sync(); }, sync, insert, focus: () => ta.focus() };
}
