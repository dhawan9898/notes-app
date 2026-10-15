// The Linux terminal page: start the virtual machine on demand, show its console with xterm.js, a key row for phones, and a queue of
// commands handed over by lessons, labs and the examples on the page.
import '@xterm/xterm/css/xterm.css';
import { commandsFromQuery, KEY_CODES, ctrlOf } from './lib/terminal-queue';
import type { Engine } from './lib/terminal-engine';
import { $, $$, esc } from './lib/ui';

const startBox = $('[data-t-start]')!, termBox = $('[data-t-term]')!, statusEl = $('[data-t-status]')!, dot = $('[data-t-dot]')!;
const bootBtn = $<HTMLButtonElement>('[data-t-boot]')!, restartBtn = $<HTMLButtonElement>('[data-t-restart]')!;
const queueBox = $('[data-t-queue]')!, queueList = $('[data-t-queue-list]')!;
let engine: Engine | null = null, term: import('@xterm/xterm').Terminal | null = null, fit: import('@xterm/addon-fit').FitAddon | null = null;
let ready = false, tail = '', ctrl = false, queue: string[] = [], running = false;

const setStatus = (text: string, state: 'idle' | 'busy' | 'ready' | 'error') => { statusEl.textContent = text; dot.dataset.state = state; };
const strip = (s: string) => s.replace(/\x1b\[[0-9;?]*[A-Za-z]/g, '');
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
const promptShown = () => /\$ $/.test(strip(tail));

function send(data: string) { engine?.send(data); }
const sendKey = (name: string) => { const v = KEY_CODES[name]; if (v) { send(v); term?.focus(); } };

function renderQueue() {
  queueBox.hidden = queue.length === 0;
  queueList.innerHTML = queue.map((c, i) => `<li class="${i === 0 ? 'next' : ''}"><code>${esc(c)}</code></li>`).join('');
}
async function runNext() {
  if (!ready || !queue.length) return;
  const cmd = queue.shift()!; renderQueue(); tail = '';
  send(cmd + '\n'); term?.focus();
  for (let t = 0; t < 400 && !promptShown(); t++) await sleep(50);          // wait up to 20 s for the prompt to come back
}
async function runAll() { if (running) return; running = true; while (queue.length && ready) await runNext(); running = false; }
function enqueue(cmds: string[], runNow = false) { queue = [...queue, ...cmds]; renderQueue(); if (runNow && ready) void runAll(); }

async function boot() {
  bootBtn.disabled = true; startBox.hidden = true; termBox.hidden = false; restartBtn.hidden = true;
  setStatus('Starting Linux (about 10 seconds, the first time a little longer)…', 'busy');
  const [{ Terminal }, { FitAddon }, { startEngine }] = await Promise.all([import('@xterm/xterm'), import('@xterm/addon-fit'), import('./lib/terminal-engine')]);
  term = new Terminal({ fontFamily: 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace', fontSize: 14, cursorBlink: true, scrollback: 2000, theme: terminalTheme(), allowProposedApi: false });
  fit = new FitAddon(); term.loadAddon(fit); termBox.innerHTML = ''; term.open(termBox); fit.fit();
  let booting = true, hold = '';
  engine = await startEngine((bytes) => {
    const text = new TextDecoder().decode(bytes); tail = (tail + text).slice(-400);
    if (booting) { hold += text; if (/~% ?$/.test(strip(hold))) { booting = false; finishBoot(); } return; }      // hide the boot messages
    term!.write(bytes);
  });
  term.onData((d) => {
    if (!ready) return;
    if (ctrl && d.length === 1) { const c = ctrlOf(d); ctrl = false; $$('[data-key="Ctrl"]').forEach((b) => b.classList.remove('on')); if (c) return send(c); }
    send(d);
  });
  addEventListener('resize', refit);
}
function finishBoot() {
  ready = true; refit();
  // a clean, familiar shell: "$ " prompt, a real terminal type, the window size, and the lab programs as the working directory
  send(`export PS1='$ ' TERM=xterm-256color; stty rows ${term!.rows} cols ${term!.cols}; cd /labs; clear\n`);
  setTimeout(() => {
    term!.reset(); term!.writeln('\x1b[1mLinux\x1b[0m (kernel 5.6, BusyBox) running in your browser. You are root in a throw-away machine: nothing here touches your device,');
    term!.writeln('and files disappear when you close or restart it. The lab programs are in /labs (try ./hello).\r\n');
    send('\n'); setStatus('Linux is running', 'ready'); restartBtn.hidden = false; term!.focus();
    if (queue.length) void runAll();
  }, 400);
}
function refit() { if (!term || !fit) return; try { fit.fit(); if (ready) send(`stty rows ${term.rows} cols ${term.cols}\n`); } catch { /* not visible yet */ } }
function terminalTheme() {
  const dark = document.documentElement.dataset.theme === 'dark';
  return dark ? { background: '#0e1424', foreground: '#eef1f8', cursor: '#5aa2ff', selectionBackground: '#2b3a5f' } : { background: '#f7f9fd', foreground: '#131822', cursor: '#0a62e6', selectionBackground: '#cfe0ff' };
}
function shutdown() { engine?.stop(); engine = null; term?.dispose(); term = null; ready = false; running = false; }

bootBtn.addEventListener('click', () => { void boot(); });
restartBtn.addEventListener('click', () => { shutdown(); startBox.hidden = true; void boot(); });
$$('[data-key]').forEach((b) => {
  b.addEventListener('pointerdown', (e) => e.preventDefault());          // keep focus (and the on-screen keyboard) in the terminal
  b.addEventListener('click', () => {
    const k = b.dataset.key!;
    if (k === 'Ctrl') { ctrl = !ctrl; b.classList.toggle('on', ctrl); term?.focus(); } else sendKey(k);
  });
});
$('[data-t-run-next]')?.addEventListener('click', () => { if (ready) void runNext(); });
$('[data-t-run-all]')?.addEventListener('click', () => { if (ready) void runAll(); });
$('[data-t-clear]')?.addEventListener('click', () => { queue = []; renderQueue(); });
$$('[data-example]').forEach((b) => b.addEventListener('click', () => {
  enqueue(JSON.parse(b.dataset.example!) as string[], true);
  if (!ready && !engine) void boot();
  $('[data-t-screen]')?.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
}));

// commands from a lesson or lab link: queue them and start Linux
const fromLink = commandsFromQuery(location.search);
if (fromLink.length) { enqueue(fromLink); void boot(); } else setStatus('Linux is not started', 'idle');
