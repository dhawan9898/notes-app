// Test doubles for the terminal page: a fake xterm.js and a fake Linux engine, so scripts/test-ui.mjs can drive src/scripts/terminal.ts in jsdom.
export class Terminal {
  rows = 24; cols = 80;
  constructor(_o?: unknown) { void _o; }
  loadAddon() {} open() {} focus() {} dispose() {} reset() {} writeln() {} write() {}
  onData(cb: (d: string) => void) { (window as any).__onData = cb; }
}
export class FitAddon { fit() {} }
export async function startEngine(onOutput: (b: Uint8Array) => void) {
  const w = window as any; w.__sent = [];
  if (w.__failBoot) throw new Error('WebAssembly is not available');
  const enc = (s: string) => new TextEncoder().encode(s);
  setTimeout(() => onOutput(enc('Welcome\r\n~% ')), 10);                       // the BusyBox prompt the page waits for
  return { send(d: string) { w.__sent.push(d); if (d.endsWith('\n')) setTimeout(() => onOutput(enc('output\r\n$ ')), 5); }, stop() { w.__stopped = true; }, onOutput };
}
