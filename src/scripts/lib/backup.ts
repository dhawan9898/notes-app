// One export/import format for everything the reader stored: progress, practice, settings.
import { progress } from './progress';
import { practice } from './practice';
import { settings } from './settings';

export const FORMAT = 'deepnotes-backup';
export const exportAll = () => JSON.stringify({ format: FORMAT, v: 1, exported: new Date().toISOString(), progress: progress.get(), practice: practice.get(), settings: settings.get() }, null, 2);

/** All-or-nothing: every part present in the file must be valid before anything is replaced. */
export function importAll(text: string): { ok: true } | { ok: false; reason: string } {
  let b: any;
  try { b = JSON.parse(text); } catch { return { ok: false, reason: 'The file is not valid JSON.' }; }
  if (!b || b.format !== FORMAT || b.v !== 1) return { ok: false, reason: 'This is not a DeepNotes backup.' };
  const stores = [[progress, b.progress], [practice, b.practice], [settings, b.settings]] as const;
  const snapshot = stores.map(([s]) => s.get());
  for (const [s, data] of stores) if (data !== undefined && !s.replace(data)) {
    stores.forEach(([t], i) => t.replace(snapshot[i] as never));          // roll back
    return { ok: false, reason: 'The backup contains data this version cannot read.' };
  }
  return { ok: true };
}
export const resetAll = () => { progress.reset(); practice.reset(); };
