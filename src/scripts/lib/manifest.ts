import data from '../../data/generated/manifest.json';
export type Item = { i: string; t: string; u: string; m: number; r: string; d?: string };
export type Prob = { i: string; r: string; df: 'Easy' | 'Medium' | 'Hard'; k: 'code' | 'question' };
export const lessons = data.lessons as Item[];
export const labs = data.labs as Item[];
export const problems = data.problems as Prob[];
