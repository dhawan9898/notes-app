// The one place pages get content from: lessons, labs, problems, modules, paths. Pure functions over Astro's collections and the generated data.
import { getCollection, type CollectionEntry } from 'astro:content';
import problemData from '../data/generated/practice.json';
import pathData from '../data/paths.json';

export const TRACKS = {
  c: { id: 'c', name: 'C Programming', short: 'C', icon: '⚙️', tint: 'c', blurb: 'From the abstract machine to ABI, undefined behaviour, linking and systems-level C.', standards: 'ISO C17/C23 · System V ABI' },
  dsa: { id: 'dsa', name: 'Data Structures & Algorithms', short: 'DSA', icon: '⚡', tint: 'dsa', blurb: 'Complexity, cache behaviour and the structures every system is built from, implemented in C.', standards: 'Every program compiled and tested' },
  linux: { id: 'linux', name: 'Linux Systems', short: 'Linux', icon: '🐧', tint: 'linux', blurb: 'The command line, processes, memory, storage, kernel interfaces, containers and observability.', standards: 'POSIX.1-2017 · Linux 6.x · cgroups v2' },
  networking: { id: 'networking', name: 'Networking', short: 'Networking', icon: '🌐', tint: 'net', blurb: 'Ethernet framing to BGP convergence, grounded in IEEE 802 and the IETF RFCs.', standards: 'IEEE 802.1/3 · IETF RFCs' },
} as const;
export type TrackId = keyof typeof TRACKS;
export const TRACK_IDS = Object.keys(TRACKS) as TrackId[];

export const url = (p: string) => import.meta.env.BASE_URL.replace(/\/$/, '') + p;

export type Lesson = CollectionEntry<'notes'>;
export type Lab = CollectionEntry<'labs'>;
export type Problem = (typeof problemData)[number] & { kind: 'code' | 'question' };
export const problems = problemData as unknown as Problem[];

const strip = (s: string) => s.replace(/^\d+-/, '');
const bySeq = (a: { data: { moduleOrder: number; order: number } }, b: typeof a) => a.data.moduleOrder - b.data.moduleOrder || a.data.order - b.data.order;
export const wordsToMinutes = (body: string | undefined) => Math.max(1, Math.ceil((body || '').split(/\s+/).filter(Boolean).length / 220));
export const minutesOf = (l: Lesson) => wordsToMinutes(l.body);

export const lessonSlug = (l: Lesson) => l.id.split('/').slice(1).map(strip).join('/');
export const lessonHref = (l: Lesson) => url(`/${l.data.track}/${lessonSlug(l)}/`);
export const labSlug = (l: Lab) => strip(l.id.split('/')[2]);
export const labHref = (l: Lab) => url(`/practice/labs/${l.data.track}/${labSlug(l)}/`);
export const problemHref = (p: Pick<Problem, 'track' | 'slug'>) => url(`/practice/${p.track}/${p.slug}/`);
/** Progress keys: lessons use the collection id, labs are prefixed so the two can never collide. */
export const labKey = (l: Lab) => `lab:${l.id}`;

export type Stage = 'start' | 'essentials' | 'core';
export const STAGES: { id: Stage; name: string; blurb: string }[] = [
  { id: 'start', name: 'Getting started', blurb: 'No background assumed. Read these first.' },
  { id: 'essentials', name: 'Essentials', blurb: 'The everyday skills every user of this subject needs.' },
  { id: 'core', name: 'Core modules', blurb: 'The mechanisms and standards in depth, from intermediate to expert.' },
];
export const stageOf = (moduleOrder: number): Stage => (moduleOrder < 0.25 ? 'start' : moduleOrder < 1 ? 'essentials' : 'core');

export type Module = {
  track: TrackId; title: string; order: number; slug: string; dir: string; prefix: string; stage: Stage; number: number;
  first: Lesson; lessons: Lesson[]; labs: Lab[]; problems: Problem[]; minutes: number;
  levels: Lesson['data']['level'][];
};

let cache: Promise<{ lessons: Lesson[]; labs: Lab[]; modules: Module[] }> | null = null;
export function loadAll() {
  return (cache ??= (async () => {
    const lessons = (await getCollection('notes')).sort((a, b) => TRACK_IDS.indexOf(a.data.track) - TRACK_IDS.indexOf(b.data.track) || bySeq(a, b));
    const labs = (await getCollection('labs')).sort((a, b) => TRACK_IDS.indexOf(a.data.track) - TRACK_IDS.indexOf(b.data.track) || bySeq(a, b));
    const modules: Module[] = [];
    const counters: Record<string, number> = {};
    for (const l of lessons) {
      let m = modules.find((x) => x.track === l.data.track && x.title === l.data.module);
      if (!m) {
        const n = (counters[l.data.track] = (counters[l.data.track] || 0) + 1);
        m = { track: l.data.track, title: l.data.module, order: l.data.moduleOrder, slug: strip(l.id.split('/')[1]), dir: l.id.split('/')[1], prefix: `${l.data.track}/${l.id.split('/')[1]}/`, stage: stageOf(l.data.moduleOrder), number: n, first: l, lessons: [], labs: [], problems: [], minutes: 0, levels: [] };
        modules.push(m);
      }
      m.lessons.push(l); m.minutes += minutesOf(l);
      if (!m.levels.includes(l.data.level)) m.levels.push(l.data.level);
    }
    for (const lab of labs) modules.find((m) => m.track === lab.data.track && m.title === lab.data.module)?.labs.push(lab);
    for (const p of problems) modules.find((m) => m.track === p.track && m.title === p.module)?.problems.push(p);
    return { lessons, labs, modules };
  })());
}
export const modulesOf = async (track: TrackId) => (await loadAll()).modules.filter((m) => m.track === track);
export const moduleHref = (m: Module) => url(`/${m.track}/#${m.slug}`);

/* ---------------------------------------------------------------- paths */
export type Path = { id: string; title: string; audience: string; summary: string; modules: Module[]; lessons: Lesson[]; minutes: number; tracks: TrackId[] };
export async function loadPaths(): Promise<Path[]> {
  const { modules } = await loadAll();
  return pathData.map((p) => {
    const mods = p.modules.map((r) => modules.find((m) => m.track === r.track && m.title === r.module)!).filter(Boolean);
    return { id: p.id, title: p.title, audience: p.audience, summary: p.summary, modules: mods, lessons: mods.flatMap((m) => m.lessons), minutes: mods.reduce((a, m) => a + m.minutes, 0), tracks: [...new Set(mods.map((m) => m.track))] };
  });
}
export const pathHref = (p: { id: string }) => url(`/paths/${p.id}/`);

export const fmtMinutes = (m: number) => (m < 90 ? `${m} min` : `${Math.round(m / 6) / 10} h`);
export const LEVEL_ORDER = ['beginner', 'intermediate', 'advanced', 'expert'] as const;
export const levelRange = (levels: readonly string[]) => {
  const idx = levels.map((l) => LEVEL_ORDER.indexOf(l as never)).filter((i) => i >= 0).sort();
  if (!idx.length) return '';
  const a = LEVEL_ORDER[idx[0]], b = LEVEL_ORDER[idx[idx.length - 1]];
  return a === b ? a : `${a} – ${b}`;
};
