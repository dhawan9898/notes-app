// Pure helpers for the practice pages (no DOM): turning a program run into a verdict and per-test results, mapping compiler
// messages back to the editor, and a small C syntax highlighter. Tested by scripts/test-practice.mjs.
import type { Verdict } from './practice-store';

export type Case = { ok: boolean; label: string; expected?: string; got?: string; line: string };
export type Judged = { verdict: Verdict; passed: number; total: number; cases: Case[]; stdout: string; message: string };
export type RunLike = { ok?: boolean; phase: string; diagnostics: string; stdout: string; stderr: string; exitCode: number | null };

const SUMMARY = /^(\d+) of (\d+) checks passed/;
/** The starter programs print `PASS label = value`, `FAIL label: expected X, got Y`, any other lines the author chose to show, and a summary. */
export function parseOutput(stdout: string) {
  const cases: Case[] = [], extra: string[] = [];
  let summary: { passed: number; total: number } | null = null;
  for (const line of stdout.split('\n')) {
    let m;
    if ((m = line.match(SUMMARY))) summary = { passed: +m[1], total: +m[2] };
    else if (/^PASS /.test(line)) { const t = line.slice(5), eq = t.lastIndexOf(' = '); cases.push({ ok: true, label: eq > 0 ? t.slice(0, eq) : t, got: eq > 0 ? t.slice(eq + 3) : undefined, line }); }
    else if (/^FAIL /.test(line)) {
      const t = line.slice(5), x = t.match(/^(.*): expected (.*), got (.*)$/);
      cases.push(x ? { ok: false, label: x[1], expected: x[2], got: x[3], line } : { ok: false, label: t, line });
    } else if (line.trim()) extra.push(line);
  }
  return { cases, extra: extra.join('\n'), summary };
}

export function judge(r: RunLike | null, error?: string): Judged {
  const none = { passed: 0, total: 0, cases: [] as Case[], stdout: '' };
  if (!r) {
    const tle = /^Stopped:/.test(error || '');
    return { verdict: tle ? 'Time Limit Exceeded' : 'Runtime Error', ...none, message: error || 'The program could not be run.' };
  }
  if (r.phase !== 'run') return { verdict: 'Compile Error', ...none, message: r.diagnostics };
  const { cases, extra, summary } = parseOutput(r.stdout);
  const passed = cases.filter((c) => c.ok).length, total = summary?.total ?? cases.length;
  if (!summary) {
    // the program ended before printing its summary: it crashed or called exit()
    const why = r.stderr.trim() || (r.exitCode ? `The program stopped with exit code ${r.exitCode} before all tests ran.` : 'The program ended before all tests ran.');
    return { verdict: 'Runtime Error', passed, total: Math.max(total, cases.length), cases, stdout: extra, message: why };
  }
  const allPassed = summary.passed === summary.total && !cases.some((c) => !c.ok) && summary.total > 0;
  return { verdict: allPassed ? 'Accepted' : 'Wrong Answer', passed: summary.passed, total: summary.total, cases, stdout: extra, message: r.stderr.trim() };
}

/** Compiler messages name lines of the whole program; the editor shows only the reader's part, so shift them. */
export function relocateDiagnostics(diag: string, prefixLines: number, codeLines: number): string {
  return diag.replace(/\b([\w.-]+\.c):(\d+):(\d+):/g, (_m, f, l, c) => {
    const n = +l - prefixLines;
    return n >= 1 && n <= codeLines ? `line ${n}, col ${c}:` : `${f}:${l}:${c} (outside your code):`;
  });
}

/** The program the compiler gets: the given code, the reader's function(s), then the tests. */
export const assemble = (prefix: string, code: string, suffix: string) => prefix + code.replace(/\s*$/, '') + '\n' + suffix;
export const countLines = (s: string) => (s.match(/\n/g) || []).length;

/* ---------- syntax highlighting (overlay behind the textarea) ---------- */
const KEYWORDS = new Set('if else for while do switch case default break continue return goto sizeof typedef struct union enum static extern const volatile register inline restrict'.split(' '));
const TYPES = new Set('void char short int long float double signed unsigned bool size_t ssize_t uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t uintptr_t intptr_t ptrdiff_t FILE NULL true false'.split(' '));
const TOKEN = /(\/\*[\s\S]*?(?:\*\/|$)|\/\/[^\n]*)|("(?:\\.|[^"\\\n])*"?|'(?:\\.|[^'\\\n])*'?)|(^[ \t]*#[ \t]*\w+)|(\b0[xX][0-9a-fA-F]+[uUlL]*\b|\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[uUlLfF]*\b)|(\b[A-Za-z_]\w*\b)/gm;
const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
export function highlightC(code: string): string {
  let out = '', last = 0, m: RegExpExecArray | null;
  TOKEN.lastIndex = 0;
  while ((m = TOKEN.exec(code))) {
    out += esc(code.slice(last, m.index)); last = m.index + m[0].length;
    const [t, comment, str, pre, num, word] = m;
    const cls = comment ? 'c' : str ? 's' : pre ? 'p' : num ? 'n' : word ? (KEYWORDS.has(word) ? 'k' : TYPES.has(word) ? 't' : /^\s*\(/.test(code.slice(last)) ? 'f' : '') : '';
    out += cls ? `<span class="tk-${cls}">${esc(t)}</span>` : esc(t);
  }
  return out + esc(code.slice(last));
}

/** Auto-indent: a new line keeps the indentation of the current one, plus one level after an opening brace. */
export function indentAfterEnter(value: string, pos: number): { insert: string; caretBack: number } {
  const lineStart = value.lastIndexOf('\n', pos - 1) + 1;
  const indent = (value.slice(lineStart, pos).match(/^[ \t]*/) || [''])[0];
  const before = value.slice(lineStart, pos).trimEnd(), after = value.slice(pos);
  if (before.endsWith('{')) {
    const closes = /^[ \t]*\}/.test(after.split('\n')[0]);
    return closes ? { insert: `\n${indent}    \n${indent}`, caretBack: indent.length + 1 } : { insert: `\n${indent}    `, caretBack: 0 };
  }
  return { insert: '\n' + indent, caretBack: 0 };
}
