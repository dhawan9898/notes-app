// The You page: settings, backup export/import, reset.
import { settings, apply } from './lib/settings';
import { progress } from './lib/progress';
import { exportAll, importAll, resetAll } from './lib/backup';
import { $, $$, toast } from './lib/ui';

const mark = () => { const s = settings.get() as unknown as Record<string, string>; $$('[data-set]').forEach((b) => b.classList.toggle('on', s[b.dataset.set!] === b.dataset.value)); };
$$('[data-set]').forEach((b) => b.addEventListener('click', () => { settings.update((s) => { (s as unknown as Record<string, string>)[b.dataset.set!] = b.dataset.value!; }); apply(); mark(); }));
mark();

$('[data-export]')?.addEventListener('click', () => {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([exportAll()], { type: 'application/json' }));
  a.download = `deepnotes-backup-${new Date().toISOString().slice(0, 10)}.json`; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
});
const file = $<HTMLInputElement>('[data-import-file]');
$('[data-import]')?.addEventListener('click', () => file?.click());
file?.addEventListener('change', async () => {
  const f = file.files?.[0]; if (!f) return;
  const r = importAll(await f.text());
  toast(r.ok ? 'Backup imported' : r.reason); apply(); mark(); file.value = '';
});
$('[data-reset]')?.addEventListener('click', () => { if (confirm('Erase all progress and saved code stored on this device? This cannot be undone.')) { resetAll(); toast('Progress and saved code erased'); } });

const note = $('[data-storage-note]');
const warn = () => { if (note && (progress.state === 'memory-only' || progress.state === 'recovered')) { note.hidden = false; note.textContent = progress.state === 'recovered' ? 'Some saved data could not be read and was set aside. Export a backup now if you can.' : 'This browser is not saving data. Export a backup before you leave.'; } };
warn(); progress.subscribe(warn);
