import { getCollection, type CollectionEntry } from 'astro:content';

export const TRACKS = {
  c: {
    name: 'C Programming',
    blurb: 'From the abstract machine to ABI, UB, linking and systems-level C.',
    icon: '⚙️',
    standards: 'ISO C17/C23 · SysV ABI',
    tags: ['ELF Internals', 'Undefined Behavior', 'Linkers & Dynamic Libs', 'POSIX I/O'],
  },
  dsa: {
    name: 'Data Structures & Algorithms',
    blurb: 'Complexity, cache performance and data structures, implemented in pure C.',
    icon: '⚡',
    standards: 'Source-compiled & verified',
    tags: ['Open Addressing', 'Intrusive Structures', 'Memory Locality', 'Arena Allocation'],
  },
  linux: {
    name: 'Linux Systems & Kernel',
    blurb: 'Userland, processes, memory paging, kernel interfaces, tracing and containers.',
    icon: '🐧',
    standards: 'POSIX.1-2017 · Linux 6.x · cgroups v2',
    tags: ['Virtual Memory & Page Cache', 'epoll & io_uring', 'eBPF & perf', 'Namespaces & cgroups'],
  },
  networking: {
    name: 'Networking (L2 & L3)',
    blurb: 'Ethernet framing to BGP convergence, grounded in IEEE 802 and IETF RFCs.',
    icon: '🌐',
    standards: 'IEEE 802.1/3 · IETF RFCs',
    tags: ['STP / RSTP / LACP', 'OSPF & BGP Peering', 'IPv6 & Neighbor Discovery', 'VXLAN & EVPN'],
  },
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

/* ---------------- learning path and progress helpers ---------------- */

/** Recommended track order for someone starting from zero. Every track also stands alone. */
export const PATH_TRACKS: TrackId[] = ['c', 'dsa', 'linux', 'networking'];

export const readingMinutes = (n: Note) =>
  Math.max(1, Math.ceil((n.body || '').split(/\s+/).filter(Boolean).length / 220));

export type PathStep = { note: Note; step: number; stage: string };

/**
 * One ordered curriculum over all notes: first the "Start here" module of every track, then each track in PATH_TRACKS order
 * (modules and notes in their authored order). A note whose prerequisites (by title, across tracks) have not been placed yet is
 * deferred until they have, so following the list never meets a note before what it builds on.
 */
export async function learningPath(): Promise<PathStep[]> {
  const all = await getCollection('notes');
  const byTitle = new Map(all.map((n) => [n.data.title, n]));
  const preferred: { note: Note; stage: string }[] = [];
  for (const id of PATH_TRACKS) {
    for (const n of await notesForTrack(id)) if (n.data.moduleOrder === 0) preferred.push({ note: n, stage: 'Start here' });
  }
  for (const id of PATH_TRACKS) {
    for (const n of await notesForTrack(id)) if (n.data.moduleOrder !== 0) preferred.push({ note: n, stage: TRACKS[id].name });
  }
  const placed = new Set<string>();
  const out: PathStep[] = [];
  const pending = [...preferred];
  while (pending.length) {
    let idx = pending.findIndex(({ note }) => note.data.prerequisites.every((p) => !byTitle.has(p) || placed.has(p)));
    if (idx < 0) idx = 0;                                   // cycle guard: cannot happen once prerequisites are acyclic
    const [next] = pending.splice(idx, 1);
    placed.add(next.note.data.title);
    out.push({ note: next.note, step: out.length + 1, stage: next.stage });
  }
  return out;
}
