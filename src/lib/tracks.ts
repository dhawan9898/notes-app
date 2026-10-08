import { getCollection, type CollectionEntry } from 'astro:content';

export const TRACKS = {
  c: { name: 'C Programming', blurb: 'From the abstract machine to ABI, UB and systems-level C.' },
  dsa: { name: 'Data Structures & Algorithms', blurb: 'Complexity, structures and techniques, implemented in C.' },
  linux: { name: 'Linux', blurb: 'Userland, processes, kernel interfaces, tracing and containers.' },
  networking: { name: 'Networking (L2 & L3)', blurb: 'Ethernet to BGP, grounded in IEEE 802 and the RFCs.' },
} as const;
export type TrackId = keyof typeof TRACKS;
export const url = (p: string) => (import.meta.env.BASE_URL.replace(/\/$/, '') + p);

export type Note = CollectionEntry<'notes'>;

export async function notesForTrack(track: TrackId): Promise<Note[]> {
  const all = await getCollection('notes', (n) => n.data.track === track);
  return all.sort((a, b) => a.data.moduleOrder - b.data.moduleOrder || a.data.order - b.data.order);
}

export function groupByModule(notes: Note[]) {
  const mods: { title: string; notes: Note[] }[] = [];
  for (const n of notes) {
    let m = mods.find((x) => x.title === n.data.module);
    if (!m) mods.push((m = { title: n.data.module, notes: [] }));
    m.notes.push(n);
  }
  return mods;
}

/** "c/01-foundations/02-types" -> "foundations/types" (number prefixes only order files). */
export const noteSlug = (n: Note) =>
  n.id.split('/').slice(1).map((s) => s.replace(/^\d+-/, '')).join('/');
export const noteHref = (n: Note) => url(`/${n.data.track}/${noteSlug(n)}/`);
