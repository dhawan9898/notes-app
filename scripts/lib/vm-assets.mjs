// Everything the in-browser Linux needs, built or downloaded into public/vm/ (git-ignored):
//   bzImage, seabios.bin, vgabios.bin   downloaded, verified by SHA-256
//   labs.cpio                           the lab programs and data files (authoring/examples/vm: *.c compiled, *.sh copied executable,
//                                       *.log *.txt *.csv *.conf copied as data) as an initramfs archive unpacked under /labs
// The programs are compiled here as static 32-bit Linux executables (the VM is an i686 PC) with clang, lld and a musl libc that is
// built once from source into .cache/vm (needs clang with the x86 target, ld.lld and llvm-ar).
import fs from 'node:fs'; import path from 'node:path'; import os from 'node:os'; import crypto from 'node:crypto';
import { spawnSync } from 'node:child_process';

export const ROOT = path.resolve(new URL('../..', import.meta.url).pathname);
export const OUT = path.join(ROOT, 'public/vm'), CACHE = path.join(ROOT, '.cache/vm'), SRC = path.join(ROOT, 'authoring/examples/vm');
const DOWNLOADS = {
  kernel: { url: 'https://i.copy.sh/buildroot-bzimage.bin', file: path.join(OUT, 'bzImage'), sha: '7befbaea31e249d9a518c4b95fa42b2a193d0e3de46250d617cbdeb866ee28b0' },   // Linux 5.6.15 + BusyBox 1.31, built with Buildroot by the v86 project
  seabios: { url: 'https://raw.githubusercontent.com/copy/v86/master/bios/seabios.bin', file: path.join(OUT, 'seabios.bin'), sha: '73e3f359102e3a9982c35fce98eb7cd08f18303ac7f1ba6ebfbe6cdc1c244d98' },
  vgabios: { url: 'https://raw.githubusercontent.com/copy/v86/master/bios/vgabios.bin', file: path.join(OUT, 'vgabios.bin'), sha: 'a4bc0d80cc3ca028c73dafa8fee396b8d054ce87ebd8abfbd31b06b437607880' },
  musl: { url: 'https://musl.libc.org/releases/musl-1.2.5.tar.gz', file: path.join(CACHE, 'musl-1.2.5.tar.gz'), sha: 'a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4' },
  compilerRt: { url: 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/compiler-rt-21.1.8.src.tar.xz', file: path.join(CACHE, 'compiler-rt-21.1.8.src.tar.xz'), sha: 'dd54ae21aee1780fac59445b51ebff601ad016b31ac3a7de3b21126fd3ccb229' },
};
const sha = (f) => crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');
const log = (m) => console.log(m);
function run(cmd, args, opts = {}) {
  const r = spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: 1 << 28, ...opts });
  if (r.status !== 0) throw new Error(`${cmd} ${args.slice(0, 4).join(' ')} failed:\n${(r.stderr || r.stdout || r.error?.message || '').slice(-1500)}`);
  return r.stdout;
}

export async function download(key) {
  const d = DOWNLOADS[key];
  fs.mkdirSync(path.dirname(d.file), { recursive: true });
  if (fs.existsSync(d.file) && sha(d.file) === d.sha) { log(`ok        ${path.relative(ROOT, d.file)}`); return d.file; }
  process.stdout.write(`download  ${path.relative(ROOT, d.file)} … `);
  let res; for (let i = 0; i < 3; i++) { try { res = await fetch(d.url); if (res.ok) break; } catch { /* retry */ } }
  if (!res?.ok) throw new Error(`could not download ${d.url}`);
  fs.writeFileSync(d.file, Buffer.from(await res.arrayBuffer()));
  if (sha(d.file) !== d.sha) { fs.unlinkSync(d.file); throw new Error(`CHECKSUM MISMATCH for ${d.url}: the upstream file changed; refusing to use it`); }
  log('done'); return d.file;
}

/* ---------------------------------------------------------------- the 32-bit toolchain */
const CLANG = process.env.CC || 'clang';
const TARGET = ['--target=i386-linux-musl'];
const SYSROOT = path.join(CACHE, 'sysroot'), BUILTINS = path.join(CACHE, 'libbuiltins.a');
const resourceInclude = () => path.join(run(CLANG, ['-print-resource-dir']).trim(), 'include');
const incArgs = () => ['-nostdinc', '-isystem', path.join(SYSROOT, 'include'), '-isystem', resourceInclude()];

export async function ensureToolchain() {
  if (fs.existsSync(path.join(SYSROOT, 'lib/libc.a')) && fs.existsSync(BUILTINS)) return;
  const musl = await download('musl'), rt = await download('compilerRt');
  const work = path.join(CACHE, 'build'); fs.rmSync(work, { recursive: true, force: true }); fs.mkdirSync(work, { recursive: true });
  log('building musl libc for 32-bit x86 (once) …');
  run('tar', ['xzf', musl, '-C', work]); const msrc = path.join(work, 'musl-1.2.5');
  const env = { ...process.env, CC: `${CLANG} ${TARGET.join(' ')}`, AR: 'llvm-ar', RANLIB: 'llvm-ranlib', CFLAGS: '-O2', LDFLAGS: '-fuse-ld=lld' };
  run('./configure', ['--target=i386', `--prefix=${SYSROOT}`, '--disable-shared'], { cwd: msrc, env });
  run('make', [`-j${Math.max(1, os.cpus().length)}`], { cwd: msrc, env }); run('make', ['install'], { cwd: msrc, env });
  log('building the 64-bit integer helpers (compiler-rt builtins) …');
  run('tar', ['xJf', rt, '-C', work]); const bsrc = path.join(work, 'compiler-rt-21.1.8.src/lib/builtins'), objs = [];
  for (const f of ['udivdi3', 'umoddi3', 'divdi3', 'moddi3', 'udivmoddi4', 'divmoddi4', 'muldi3', 'ashldi3', 'ashrdi3', 'lshrdi3', 'cmpdi2', 'ucmpdi2', 'floatdidf', 'floatundidf', 'fixdfdi', 'fixunsdfdi', 'floatdisf', 'floatundisf', 'fixsfdi', 'fixunssfdi', 'divsi3', 'udivsi3', 'modsi3', 'umodsi3', 'udivmodsi4', 'clzdi2', 'ctzdi2', 'popcountdi2', 'bswapdi2']) {
    const o = path.join(work, `${f}.o`); run(CLANG, [...TARGET, '-O2', ...incArgs(), `-I${bsrc}`, '-c', path.join(bsrc, `${f}.c`), '-o', o]); objs.push(o);
  }
  run('llvm-ar', ['rc', BUILTINS, ...objs]);
  fs.rmSync(work, { recursive: true, force: true });
}

export function compile(srcFile, outFile) {
  const args = [...TARGET, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-static', ...incArgs(), '-nostdlib', '-fuse-ld=lld', srcFile, path.join(SYSROOT, 'lib/crt1.o'), path.join(SYSROOT, 'lib/libc.a'), BUILTINS, '-o', outFile];
  const r = spawnSync(CLANG, args, { encoding: 'utf8' });
  if (r.status !== 0) throw new Error(`compile ${path.relative(ROOT, srcFile)} failed:\n${r.stderr}`);
  return r.stderr.trim();                                              // warnings, if any
}

/* ---------------------------------------------------------------- initramfs archive (cpio "newc") */
export function cpio(entries) {                       // entries: [name, data, mode?]
  const chunks = []; let ino = 1; const pad4 = (n) => (4 - (n % 4)) % 4, hex = (n) => n.toString(16).padStart(8, '0');
  const add = (name, data, mode) => {
    const nm = Buffer.from(name + '\0');
    chunks.push(Buffer.from('070701' + [ino++, mode, 0, 0, 1, 0, data.length, 0, 0, 0, 0, nm.length, 0].map(hex).join('')), nm, Buffer.alloc(pad4(110 + nm.length)), data, Buffer.alloc(pad4(data.length)));
  };
  add('labs', Buffer.alloc(0), 0o040755);
  for (const [name, data, mode] of entries) add(`labs/${name}`, data, mode ?? 0o100755);
  add('TRAILER!!!', Buffer.alloc(0), 0);
  return Buffer.concat(chunks);
}

export async function buildLabs({ quiet = false } = {}) {
  const sources = fs.existsSync(SRC) ? fs.readdirSync(SRC).filter((f) => /\.(c|sh|log|txt|csv|conf)$/.test(f)).sort() : [];
  const entries = [];
  if (sources.some((f) => f.endsWith('.c'))) await ensureToolchain();
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'vm-labs-'));
  for (const f of sources) {
    if (!f.endsWith('.c')) { entries.push([f, fs.readFileSync(path.join(SRC, f)), f.endsWith('.sh') ? 0o100755 : 0o100644]); continue; }      // scripts keep their name, data files too
    const base = f.replace(/\.c$/, '');
    const out = path.join(tmp, base), warn = compile(path.join(SRC, f), out);
    if (warn && !quiet) console.log(`warnings in ${f}:\n${warn}`);
    entries.push([base, fs.readFileSync(out), 0o100755]);
  }
  fs.rmSync(tmp, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });
  fs.writeFileSync(path.join(OUT, 'labs.cpio'), cpio(entries));
  if (!quiet) log(`built     public/vm/labs.cpio (${entries.length} files: ${entries.map((e) => e[0]).join(', ') || 'none'})`);
  return entries.map((e) => e[0]);
}
