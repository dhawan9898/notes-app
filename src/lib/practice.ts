import data from '../data/practice.json';
import { url } from './tracks';

export type Example = { input: string; output: string; why?: string };
export type Problem = {
  id: string; track: 'c' | 'dsa'; topic: string; slug: string; rel: string; title: string; statement: string;
  difficulty: 'Easy' | 'Medium' | 'Hard'; tags: string[]; examples: Example[]; constraints: string[]; hints: string[];
  prefix: string; starter: string; suffix: string; solution: string;
  note: { title: string; path: string; file: string }; hash: string; runnable: boolean;
};
export const problems = data as Problem[];
export const TRACK_LABEL: Record<string, string> = { c: 'C', dsa: 'DSA' };
export const practiceHref = (p: Pick<Problem, 'id'>) => url(`/practice/${p.id}/`);
export const noteHrefOf = (p: Problem) => url(`/${p.note.path}/`);

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
/** The small markup the problem texts use: paragraphs, `code` and **bold**. */
export const inline = (s: string) => esc(s).replace(/`([^`]+)`/g, '<code>$1</code>').replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
export const paragraphs = (s: string) => s.split(/\n{2,}/).map((p) => `<p>${inline(p).replace(/\n/g, '<br>')}</p>`).join('');
