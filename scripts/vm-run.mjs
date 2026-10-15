// Runs shell commands in the in-browser Linux and prints them like a terminal transcript ("$ cmd" then the output).
// Usage: node scripts/vm-run.mjs [--rc] "cmd 1" "cmd 2" …      (used by authoring/build.py for the {{vm :: cmd}} directive)
import { bootVm } from './lib/vm.mjs';
const showRc = process.argv.includes('--rc'), cmds = process.argv.slice(2).filter((a) => a !== '--rc');
const vm = await bootVm();
for (const c of cmds) { const { output, rc } = await vm.run(c); console.log(`$ ${c}${output ? '\n' + output : ''}${showRc ? `\n[exit ${rc}]` : ''}`); }
await vm.close(); process.exit(0);
