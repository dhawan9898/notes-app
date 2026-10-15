// Verifies what the Linux terminal promises, by running it for real in the in-browser Linux:
//   node scripts/verify-terminal.mjs            the example sessions on the terminal page and the terminal: lists of the labs
//   node scripts/verify-terminal.mjs --blocks   also finds the command blocks of lessons and labs that work there, and writes
//                                               src/data/terminal-blocks.json (only those blocks get a "Run in terminal" button)
import fs from 'node:fs'; import path from 'node:path'; import crypto from 'node:crypto';
import { bootVm } from './lib/vm.mjs';
import { ROOT } from './lib/vm-assets.mjs';
import { loadLessons, loadLabs } from './lib/content.mjs';
import { commandsFromBlock } from '../src/scripts/lib/terminal-queue.ts';

const wantBlocks = process.argv.includes('--blocks');
const clean = (s) => s.split('\n').filter((l) => !/host9p|9p on \/mnt/.test(l)).join('\n');     // a boot-time kernel message can arrive late
const BAD_OUTPUT = /not found|applet not found|invalid option|unrecognized option|bad address|can't open/i;
const vm = await bootVm();
console.log(`booted in ${(vm.bootMs / 1000).toFixed(1)} s`);
let bad = 0;

/** Runs a list of commands in one shell session starting from a fresh directory; returns a list of problems. */
async function session(commands, dir) {
  const problems = [];
  await vm.run(`rm -rf ${dir}; mkdir -p ${dir}; cd /labs`);
  for (const cmd of commands) {
    let r; try { r = await vm.run(cmd, { timeout: 20000 }); } catch (e) { problems.push(`timeout: ${cmd}`); break; }
    const out = clean(r.output);
    if (r.rc !== 0) problems.push(`exit ${r.rc}: ${cmd}\n      ${out.split('\n').slice(0, 3).join('\n      ')}`);
    else if (BAD_OUTPUT.test(out)) problems.push(`suspicious output for: ${cmd}\n      ${out.split('\n')[0]}`);
  }
  return problems;
}

/* ---- the examples on the page */
const examples = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/terminal-examples.json'), 'utf8'));
for (const ex of examples) {
  const problems = await session(ex.commands, `/tmp/ex-${ex.id}`);
  if (problems.length) { bad++; console.log(`FAIL example ${ex.id}\n    ${problems.join('\n    ')}`); } else console.log(`ok   example ${ex.id} (${ex.commands.length} commands)`);
}
/* ---- the labs that run in the app */
for (const lab of loadLabs().filter((l) => l.data.env === 'browser')) {
  const problems = await session(lab.data.terminal, `/tmp/lab-${lab.slug}`);
  if (problems.length) { bad++; console.log(`FAIL lab ${lab.id}\n    ${problems.join('\n    ')}`); } else console.log(`ok   lab ${lab.id} (${lab.data.terminal.length} commands)`);
}

/* ---- blocks in lessons and labs */
if (wantBlocks) {
  const applets = new Set((await vm.run('busybox --list')).output.split('\n').map((s) => s.trim()).filter(Boolean));
  const builtins = ['cd', 'echo', 'export', 'test', '[', 'set', 'true', 'false', 'printf', 'pwd', 'type', 'unset', 'umask', 'ulimit', 'exit', 'time'];
  const labPrograms = new Set(fs.readdirSync(path.join(ROOT, 'authoring/examples/vm')).map((f) => f.replace(/\.(c|sh)$/, '')));
  const NEVER = /\b(sudo|systemctl|journalctl|apt|dnf|yum|dpkg|rpm|ssh|scp|curl|wget|git|gcc|cc|clang|make|cmake|gdb|python3?|tcpdump|nft|iptables|unshare|nsenter|ip netns|tc |top|watch|tail -f|less|more|vi|vim|nano|reboot|shutdown|halt|mkfs|passwd|su\b|docker|podman|strace|perf|ping6?(?! -c))\b/;
  const allowedWord = (w) => applets.has(w) || builtins.includes(w) || /^\.\/[\w.-]+$/.test(w) && labPrograms.has(w.slice(2)) || /^[A-Za-z_]\w*=/.test(w) || ['for', 'do', 'done', 'if', 'then', 'else', 'fi', 'while'].includes(w);
  const table = {}; let tried = 0;
  const hash = (t) => crypto.createHash('sha1').update(t.trim()).digest('hex').slice(0, 16);
  for (const it of [...loadLessons(), ...loadLabs()]) for (const m of it.body.matchAll(/^```(\w*)\n([\s\S]*?)^```/gm)) {
    const cmds = commandsFromBlock(m[2]);
    if (!cmds.length || cmds.length > 12 || cmds.some((c) => c.length > 300)) continue;
    const words = cmds.flatMap((c) => c.split(/\s*(?:\|\||&&|\||;)\s*/)).map((seg) => seg.trim().split(/\s+/)[0]).filter(Boolean);
    if (NEVER.test(cmds.join('\n')) || !words.every(allowedWord)) continue;
    const h = hash(m[2]); if (table[h]) continue;
    tried++;
    const problems = await session(cmds, `/tmp/blk-${h}`);
    if (!problems.length) { table[h] = { ok: true, where: it.id, n: cmds.length }; console.log(`ok   block ${it.id} (${cmds.length} commands)`); }
    else console.log(`skip block ${it.id}: ${problems[0].split('\n')[0]}`);
  }
  fs.writeFileSync(path.join(ROOT, 'src/data/terminal-blocks.json'), JSON.stringify(table, null, 1) + '\n');
  console.log(`terminal-blocks.json: ${Object.keys(table).length} of ${tried} candidate blocks work in the terminal`);
}
await vm.close(); console.log(bad ? `${bad} failure(s)` : 'terminal checks passed'); process.exit(bad ? 1 : 0);
