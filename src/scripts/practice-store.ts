// Practice-problem store: the reader's code and submissions. Everything lives in this browser's localStorage (and so inside the
// Android app's WebView); nothing is sent anywhere.
export type Verdict = 'Accepted' | 'Wrong Answer' | 'Compile Error' | 'Runtime Error' | 'Time Limit Exceeded';
export type Submission = { ts: number; verdict: Verdict; passed: number; total: number; ms: number; code: string };
export type Rec = {
  draft?: string;          // the editable region as the reader last left it (autosaved)
  draftTs?: number;
  subs: Submission[];      // newest first
  solved?: number;         // timestamp of the first Accepted submission
};
export type State = { v: 1; p: Record<string, Rec> };

const KEY = 'deepnotes:practice:v1';
export const MAX_SUBMISSIONS = 25;
const empty = (): State => ({ v: 1, p: {} });

let cache: State | null = null;
export function load(): State {
  if (cache) return cache;
  try {
    const raw = typeof localStorage === 'undefined' ? null : localStorage.getItem(KEY);
    const s = raw ? JSON.parse(raw) : null;
    cache = s && s.v === 1 && s.p && typeof s.p === 'object' ? s : empty();
  } catch { cache = empty(); }
  return cache!;
}
const announce = () => { try { window.dispatchEvent(new CustomEvent('dn:practice')); } catch { /* no window (tests) */ } };
function save(): boolean {
  try { localStorage.setItem(KEY, JSON.stringify(cache)); announce(); return true; }
  catch { announce(); return false; }       // private mode or full storage: the work stays in memory for this tab
}
try { window.addEventListener('storage', (e) => { if (e.key === KEY) { cache = null; announce(); } }); } catch { /* no window (tests) */ }

const rec = (id: string): Rec => (load().p[id] ||= { subs: [] });

export const getDraft = (id: string): string | undefined => load().p[id]?.draft;
export function setDraft(id: string, code: string): boolean { const r = rec(id); r.draft = code; r.draftTs = Date.now(); return save(); }
export function clearDraft(id: string) { const r = load().p[id]; if (r) { delete r.draft; delete r.draftTs; save(); } }
export const submissions = (id: string): Submission[] => load().p[id]?.subs ?? [];
export const lastSubmission = (id: string): Submission | undefined => submissions(id)[0];

/** Records a submission. The submitted code becomes the draft too, so the editor always reopens with the latest work. */
export function addSubmission(id: string, s: Omit<Submission, 'ts'>): Submission {
  const r = rec(id), sub: Submission = { ...s, ts: Date.now() };
  r.subs.unshift(sub);
  if (r.subs.length > MAX_SUBMISSIONS) r.subs.length = MAX_SUBMISSIONS;
  r.draft = s.code; r.draftTs = sub.ts;
  if (s.verdict === 'Accepted' && !r.solved) r.solved = sub.ts;
  save();
  return sub;
}

export type Status = 'solved' | 'attempted' | 'todo';
export function status(id: string): Status {
  const r = load().p[id];
  return r?.solved ? 'solved' : r && (r.subs.length || r.draft) ? 'attempted' : 'todo';
}
export const solvedCount = (ids: string[]) => ids.filter((i) => status(i) === 'solved').length;

export const exportJson = () => JSON.stringify(load(), null, 2);
export function importJson(text: string): boolean {
  try {
    const s = JSON.parse(text);
    if (!s || s.v !== 1 || typeof s.p !== 'object') return false;
    cache = s; return save();
  } catch { return false; }
}
export function reset() { cache = empty(); save(); }
export const _forTests = () => { cache = null; };
