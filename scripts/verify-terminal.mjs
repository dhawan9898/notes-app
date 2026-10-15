// Runs every example command of the Linux terminal page (src/data/terminal-examples.json) inside the in-browser Linux and fails if any
// command exits non-zero or prints "not found". Keeps the promises the page makes. Usage: node scripts/verify-terminal.mjs
import fs from 'node:fs'; import path from 'node:path';
import { bootVm } from './lib/vm.mjs';
import { ROOT } from './lib/vm-assets.mjs';
const examples = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/data/terminal-examples.json'), 'utf8'));
const vm = await bootVm(); let bad = 0;
console.log(`booted in ${(vm.bootMs / 1000).toFixed(1)} s`);
for (const ex of examples) {
  const problems = [];
  await vm.run('cd /labs');                                             // every example starts in /labs, like the terminal page; state carries over between its commands
  for (const cmd of ex.commands) {
    let { output, rc } = await vm.run(cmd, { timeout: 40000 });
    output = output.split('\n').filter((l) => !/host9p|9p on \/mnt/.test(l)).join('\n');   // a boot-time kernel message can arrive late
    // the last command of a pipeline decides rc; "grep -c" style commands may legitimately exit 1 only if the example says so (none do)
    if (rc !== 0) problems.push(`exit ${rc}: ${cmd}\n      ${output.split('\n').slice(0, 3).join('\n      ')}`);
    else if (/not found|applet not found|invalid option|unrecognized option|usage:/i.test(output)) problems.push(`suspicious output for: ${cmd}\n      ${output.split('\n')[0]}`);
  }
  if (problems.length) { bad++; console.log(`FAIL ${ex.id}\n    ${problems.join('\n    ')}`); } else console.log(`ok   ${ex.id} (${ex.commands.length} commands)`);
}
await vm.close(); console.log(bad ? `${bad} example(s) failed` : 'all terminal examples work'); process.exit(bad ? 1 : 0);
