// Boots the in-browser Linux headlessly (v86 in Node) so builds and tests can run commands in it and capture real output.
//   const vm = await bootVm(); const { output, rc } = await vm.run('ls /labs'); await vm.close();
import path from 'node:path'; import { V86 } from 'v86';
import { OUT, ROOT } from './vm-assets.mjs';

const strip = (s) => s.replace(/\x1b\[[0-9;?]*[A-Za-z]/g, '').replace(/\r/g, '');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
export const CMDLINE = 'tsc=reliable mitigations=off random.trust_cpu=on console=ttyS0 quiet loglevel=3';

export async function bootVm({ memoryMb = 128, timeoutMs = 180000 } = {}) {
  const emu = new V86({
    wasm_path: path.join(ROOT, 'node_modules/v86/build/v86.wasm'), memory_size: memoryMb * 1024 * 1024, vga_memory_size: 2 * 1024 * 1024,
    bios: { url: path.join(OUT, 'seabios.bin') }, vga_bios: { url: path.join(OUT, 'vgabios.bin') },
    bzimage: { url: path.join(OUT, 'bzImage') }, initrd: { url: path.join(OUT, 'labs.cpio') }, cmdline: CMDLINE,
    autostart: true, disable_keyboard: true, disable_speaker: true, disable_mouse: true,
  });
  let buf = '';
  emu.add_listener('serial0-output-byte', (b) => { buf += String.fromCharCode(b); });
  const until = async (test, ms) => { const t0 = Date.now(); while (!test(buf)) { if (Date.now() - t0 > ms) throw new Error(`timeout after ${ms} ms; output so far:\n${strip(buf).slice(-600)}`); await sleep(50); } };
  const t0 = Date.now();
  await until((s) => /~% ?$/m.test(strip(s)), timeoutMs);
  const bootMs = Date.now() - t0;
  const send = (line) => emu.serial0_send(line + '\n');
  // a quiet, predictable shell: no echo, plain prompt
  buf = ''; send("stty -echo; export PS1='' TERM=dumb; cd /labs"); await sleep(300); buf = '';
  let n = 0;
  return {
    bootMs,
    /** Runs one shell command line; returns its combined output and exit status. */
    async run(cmd, { timeout = 60000 } = {}) {
      const id = `${Date.now()}${n++}`, start = `<<S${id}>>`, end = `<<E${id}:`;
      buf = ''; send(`echo '${start}'; ${cmd}\necho "${end}$?>>"`);
      await until((s) => strip(s).includes(end) && /<<E\d+:\d+>>/.test(strip(s)), timeout);
      const text = strip(buf), a = text.indexOf(start), b = text.lastIndexOf(end);
      const rc = Number((text.slice(b).match(/:(\d+)>>/) || [])[1]);
      return { output: text.slice(a + start.length, b).replace(/^\n/, '').replace(/\n+$/, ''), rc };
    },
    async close() { try { await emu.stop(); emu.destroy?.(); } catch { /* ignore */ } },
  };
}
