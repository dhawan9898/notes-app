// Web Worker: owns the WebAssembly C compiler so the page stays responsive. Messages in: { id, source, stdin, files }.
// Messages out: { id, status } while working, then { id, result } or { id, error }.
importScripts('cc.js');
const base = self.location.href.replace(/[^/]*$/, '');
let runner = null, current = 0;
async function loadBytes(name) {
  const res = await fetch(base + name);
  if (!res.ok) throw new Error(`Could not load ${name} (${res.status})`);
  return res.arrayBuffer();
}
onmessage = async (e) => {
  const { id, source, stdin, files } = e.data;
  current = id;
  try {
    runner = runner || new CRunner({
      loadBytes, compileModule: (b) => WebAssembly.compile(b), now: () => performance.now(),
      onStatus: (m) => postMessage({ id: current, status: m }),
    });
    postMessage({ id, status: 'Starting…' });
    postMessage({ id, result: await runner.runC(source, { stdin, files }) });
  } catch (err) {
    postMessage({ id, error: String((err && err.message) || err) });
  }
};
