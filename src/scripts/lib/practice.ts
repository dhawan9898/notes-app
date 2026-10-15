// Practice state: the reader's drafts and submissions per problem. Built on the versioned store.
import aliases from '../../data/id-aliases.json';
import { createStore, isObj } from './store';

export type Verdict = 'Accepted' | 'Wrong Answer' | 'Compile Error' | 'Runtime Error' | 'Time Limit Exceeded';
export type Submission = { ts: number; verdict: Verdict; passed: number; total: number; ms: number; code: string };
export type Rec = { draft?: string; draftTs?: number; subs: Submission[]; solved?: number };
export type Practice = { p: Record<string, Rec> };
export const MAX_SUBMISSIONS = 25;
void aliases;

const check = (d: unknown): d is Practice => isObj(d) && isObj(d.p) && Object.values(d.p).every((r) => isObj(r) && Array.isArray((r as Rec).subs));
export const practice = createStore<Practice>({
  key: 'dn:practice', version: 1, empty: () => ({ p: {} }), check,
  legacy: { key: 'deepnotes:practice:v1', convert: (raw) => (isObj(raw) && raw.v === 1 && isObj(raw.p) ? { p: raw.p } : null) },
});

const rec = (d: Practice, id: string): Rec => (d.p[id] ||= { subs: [] });
export const getDraft = (id: string): string | undefined => practice.get().p[id]?.draft;
export function setDraft(id: string, text: string): boolean {
  practice.update((d) => { const r = rec(d, id); r.draft = text; r.draftTs = Date.now(); });
  return practice.state !== 'memory-only';
}
export const submissions = (id: string): Submission[] => practice.get().p[id]?.subs ?? [];
export const lastSubmission = (id: string) => submissions(id)[0];

/** Records a submission; the submitted text becomes the draft too, so the editor reopens with the latest work. */
export function addSubmission(id: string, s: Omit<Submission, 'ts'>): Submission {
  const sub: Submission = { ...s, ts: Date.now() };
  practice.update((d) => {
    const r = rec(d, id);
    r.subs.unshift(sub); if (r.subs.length > MAX_SUBMISSIONS) r.subs.length = MAX_SUBMISSIONS;
    r.draft = s.code; r.draftTs = sub.ts;
    if (s.verdict === 'Accepted' && !r.solved) r.solved = sub.ts;
  });
  return sub;
}
/** For question problems: "I got it" / "Review again". */
export const setSolved = (id: string, value: boolean) => practice.update((d) => { const r = rec(d, id); if (value) r.solved ||= Date.now(); else delete r.solved; });

export type Status = 'solved' | 'attempted' | 'todo';
export function status(id: string): Status {
  const r = practice.get().p[id];
  return r?.solved ? 'solved' : r && (r.subs.length || r.draft) ? 'attempted' : 'todo';
}
export const solvedCount = (ids: readonly string[]) => ids.filter((i) => status(i) === 'solved').length;
