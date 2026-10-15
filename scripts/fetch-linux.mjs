// Prepares the in-browser Linux (public/linux/): downloads the kernel image and BIOS (SHA-256 pinned) and builds the lab programs.
// Usage: node scripts/fetch-linux.mjs [--no-labs]
import { download, buildLabs } from './lib/vm-assets.mjs';
for (const k of ['kernel', 'seabios', 'vgabios']) await download(k);
if (!process.argv.includes('--no-labs')) await buildLabs();
