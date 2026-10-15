// The in-browser Linux: a PC emulator (v86, WebAssembly) running a small Linux (kernel + BusyBox) with the lab programs under /labs.
// Everything stays on the device. Serial console in, serial console out.
import wasmUrl from 'v86/build/v86.wasm?url';
import { href } from './ui';

export interface Engine { send(data: string): void; stop(): void; onOutput: (bytes: Uint8Array) => void }
// keep in step with CMDLINE in scripts/lib/vm.mjs (the headless runner used by the build and the tests)
const CMDLINE = 'tsc=reliable mitigations=off random.trust_cpu=on console=ttyS0 quiet loglevel=3';

export async function startEngine(onOutput: (bytes: Uint8Array) => void): Promise<Engine> {
  const { V86 } = await import('v86');
  const emu = new V86({
    wasm_path: wasmUrl, memory_size: 128 * 1024 * 1024, vga_memory_size: 2 * 1024 * 1024,
    bios: { url: href('/vm/seabios.bin') }, vga_bios: { url: href('/vm/vgabios.bin') },
    bzimage: { url: href('/vm/bzImage') }, initrd: { url: href('/vm/labs.cpio') }, cmdline: CMDLINE,
    autostart: true, disable_keyboard: true, disable_mouse: true, disable_speaker: true,
  });
  const engine: Engine = { onOutput, send: (d) => emu.serial0_send(d), stop: () => { try { emu.stop(); emu.destroy(); } catch { /* already stopped */ } } };
  let pending: number[] = [], scheduled = false;
  emu.add_listener('serial0-output-byte', (b: number) => {
    pending.push(b);
    if (!scheduled) { scheduled = true; requestAnimationFrame(() => { scheduled = false; const chunk = Uint8Array.from(pending); pending = []; engine.onOutput(chunk); }); }
  });
  return engine;
}
