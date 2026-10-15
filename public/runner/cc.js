/*
 * DeepNotes in-app C runner: compiles C with clang (WebAssembly build) and runs the result, all on the device.
 *
 * The compiler and linker binaries (clang.wasm, lld.wasm, memfs.wasm, sysroot.tar) are the prebuilt artifacts of
 * https://github.com/binji/wasm-clang (clang/LLVM 8.0.1 targeting wasm32-wasi, wasi-libc sysroot); this file adapts that
 * project's host code (Apache-2.0, Copyright 2020 WebAssembly Community Group participants) for running C programs
 * with captured output. See public/runner/NOTICE.md.
 *
 * Works in a browser Worker (importScripts) and in Node (require), so the same code validates snippets at build time.
 */
(function (root) {
  'use strict';

  class ProcExit extends Error { constructor(code) { super(`exit ${code}`); this.code = code; } }
  class Unsupported extends Error { constructor(n) { super(`${n} is not supported`); } }
  const ESUCCESS = 0;

  function readStr(u8, o, len = -1) {
    let s = ''; let end = u8.length; if (len !== -1) end = o + len;
    for (let i = o; i < end && u8[i] !== 0; ++i) s += String.fromCharCode(u8[i]);
    return s;
  }
  const bind = (obj, names) => Object.fromEntries(names.map((n) => [n, obj[n].bind(obj)]));

  class Memory {
    constructor(memory) { this.memory = memory; this.refresh(); }
    refresh() { this.buffer = this.memory.buffer; this.u8 = new Uint8Array(this.buffer); this.u32 = new Uint32Array(this.buffer); }
    check() { if (this.buffer.byteLength === 0 || this.buffer !== this.memory.buffer) this.refresh(); }
    read32(o) { return this.u32[o >> 2]; }
    write8(o, v) { this.u8[o] = v; }
    write32(o, v) { this.u32[o >> 2] = v; }
    write64(o, lo, hi = 0) { this.write32(o, lo); this.write32(o + 4, hi); }
    readStr(o, len) { return readStr(this.u8, o, len); }
    write(o, buf) {
      if (buf instanceof ArrayBuffer) return this.write(o, new Uint8Array(buf));
      if (typeof buf === 'string') return this.write(o, Array.from(buf, (c) => c.charCodeAt(0) & 255));
      new Uint8Array(this.buffer, o, buf.length).set(buf); return buf.length;
    }
    writeStr(o, str) { o += this.write(o, str); this.write8(o, 0); return str.length + 1; }
  }

  // Virtual file system implemented by memfs.wasm; the host provides stdout/stderr/stdin and copies between memories.
  class MemFS {
    constructor({ compileModule, memfsBytes, onOutput }) {
      this.onOutput = onOutput; this.stdin = ''; this.stdinPos = 0; this.hostMem_ = null;
      const env = bind(this, ['abort', 'host_write', 'host_read', 'memfs_log', 'copy_in', 'copy_out']);
      this.ready = compileModule(memfsBytes).then((m) => WebAssembly.instantiate(m, { env })).then((inst) => {
        this.exports = inst.exports; this.mem = new Memory(inst.exports.memory); inst.exports.init();
      });
    }
    set hostMem(m) { this.hostMem_ = m; }
    setStdin(s) { this.stdin = s; this.stdinPos = 0; }
    addDirectory(path) { this.mem.check(); this.mem.write(this.exports.GetPathBuf(), path); this.exports.AddDirectoryNode(path.length); }
    addFile(path, contents) {
      const length = contents instanceof ArrayBuffer ? contents.byteLength : contents.length;
      this.mem.check(); this.mem.write(this.exports.GetPathBuf(), path);
      const inode = this.exports.AddFileNode(path.length, length);
      const addr = this.exports.GetFileNodeAddress(inode); this.mem.check(); this.mem.write(addr, contents);
    }
    getFileContents(path) {
      this.mem.check(); this.mem.write(this.exports.GetPathBuf(), path);
      const inode = this.exports.FindNode(path.length);
      return new Uint8Array(this.mem.buffer, this.exports.GetFileNodeAddress(inode), this.exports.GetFileNodeSize(inode));
    }
    abort() { throw new Error('abort'); }
    host_write(fd, iovs, iovsLen, nwrittenOut) {
      this.hostMem_.check(); let size = 0; let str = '';
      for (let i = 0; i < iovsLen; ++i) {
        const buf = this.hostMem_.read32(iovs); iovs += 4; const len = this.hostMem_.read32(iovs); iovs += 4;
        str += this.hostMem_.readStr(buf, len); size += len;
      }
      this.hostMem_.write32(nwrittenOut, size); this.onOutput(fd === 2 ? 'stderr' : 'stdout', str); return ESUCCESS;
    }
    host_read(fd, iovs, iovsLen, nread) {
      this.hostMem_.check(); let size = 0;
      for (let i = 0; i < iovsLen; ++i) {
        const buf = this.hostMem_.read32(iovs); iovs += 4; const len = this.hostMem_.read32(iovs); iovs += 4;
        const n = Math.min(len, this.stdin.length - this.stdinPos); if (n === 0) break;
        this.hostMem_.write(buf, this.stdin.substr(this.stdinPos, n)); size += n; this.stdinPos += n; if (n !== len) break;
      }
      this.hostMem_.write32(nread, size); return ESUCCESS;
    }
    memfs_log() {}
    copy_out(dst, src, size) { this.hostMem_.check(); this.mem.check(); new Uint8Array(this.hostMem_.buffer, dst, size).set(new Uint8Array(this.mem.buffer, src, size)); }
    copy_in(dst, src, size) { this.mem.check(); this.hostMem_.check(); new Uint8Array(this.mem.buffer, dst, size).set(new Uint8Array(this.hostMem_.buffer, src, size)); }
  }

  class Tar {
    constructor(buffer) { this.u8 = new Uint8Array(buffer); this.offset = 0; }
    str(len) { const s = readStr(this.u8, this.offset, len); this.offset += len; return s; }
    octal(len) { return parseInt(this.str(len), 8); }
    align() { this.offset = (this.offset + 511) & ~511; }
    entry() {
      if (this.offset + 512 > this.u8.length) return null;
      const e = { filename: this.str(100), mode: this.octal(8), owner: this.octal(8), group: this.octal(8), size: this.octal(12), mtim: this.octal(12), checksum: this.octal(8), type: this.str(1), linkname: this.str(100) };
      if (this.str(8) !== 'ustar  ') return null;
      this.str(32); this.str(32); this.str(8); this.str(8); this.str(155); this.align();
      if (e.type === '0') { e.contents = this.u8.subarray(this.offset, this.offset + e.size); this.offset += e.size; this.align(); }
      return e;
    }
    untar(fs) { let e; while ((e = this.entry())) { if (e.type === '0') fs.addFile(e.filename, e.contents); else if (e.type === '5') fs.addDirectory(e.filename); } }
  }

  // A running wasm program (clang, lld, or the user's program).
  class App {
    constructor(module, memfs, now, ...argv) {
      this.argv = argv; this.memfs = memfs; this.now = now;
      const wasi_unstable = bind(this, ['proc_exit', 'environ_sizes_get', 'environ_get', 'args_sizes_get', 'args_get', 'random_get', 'clock_time_get', 'poll_oneoff']);
      Object.assign(wasi_unstable, memfs.exports);
      // clang's wasm build also imports canvas hooks from the browser demo; none are used for plain C.
      const env = new Proxy({}, { get: () => () => 0 });
      this.ready = WebAssembly.instantiate(module, { wasi_unstable, env }).then((inst) => {
        this.exports = inst.exports; this.mem = new Memory(inst.exports.memory); memfs.hostMem = this.mem;
      });
    }
    async run() {
      await this.ready;
      try { this.exports._start(); } catch (e) { if (e instanceof ProcExit) return e.code; throw e; }
      return 0;
    }
    proc_exit(code) { throw new ProcExit(code); }
    environ_sizes_get(c, s) { this.mem.check(); this.mem.write64(c, 0); this.mem.write64(s, 0); return ESUCCESS; }
    environ_get(p) { this.mem.check(); this.mem.write32(p, 0); return ESUCCESS; }
    args_sizes_get(c, s) { this.mem.check(); this.mem.write64(c, this.argv.length); this.mem.write64(s, this.argv.reduce((a, x) => a + x.length + 1, 0)); return ESUCCESS; }
    args_get(ptrs, buf) { this.mem.check(); for (const a of this.argv) { this.mem.write32(ptrs, buf); ptrs += 4; buf += this.mem.writeStr(buf, a); } this.mem.write32(ptrs, 0); return ESUCCESS; }
    random_get(buf, len) { this.mem.check(); const d = new Uint8Array(this.mem.buffer, buf, len); for (let i = 0; i < len; ++i) d[i] = (Math.random() * 256) | 0; return ESUCCESS; }
    clock_time_get(id, _precision, out) {
      this.mem.check();
      const ns = id === 0 ? Math.floor(Date.now()) * 1e6 : Math.floor(this.now() * 1e6);          // 0 realtime, others monotonic/cpu
      this.mem.write64(out, ns % 4294967296 >>> 0, Math.floor(ns / 4294967296)); return ESUCCESS;
    }
    poll_oneoff() { throw new Unsupported('poll_oneoff'); }
  }

  const CLANG_ARGS = ['-disable-free', '-isysroot', '/', '-internal-isystem', '/include', '-internal-isystem', '/lib/clang/8.0.1/include', '-ferror-limit', '10', '-fmessage-length', '100',
    '-fno-common'];   // uninitialised globals as common symbols crash this LLVM 8 wasm toolchain ("unreachable")

  class CRunner {
    /**
     * opts.loadBytes(name) -> Promise<ArrayBuffer>      reads clang.wasm, lld.wasm, memfs.wasm, sysroot.tar
     * opts.compileModule(bytes) -> Promise<WebAssembly.Module>
     * opts.now() -> milliseconds (monotonic)
     * opts.onStatus(msg)                                 progress ("Loading compiler…")
     */
    constructor(opts) {
      this.opts = opts; this.out = []; this.modules = {};
      this.memfs = null; this.ready = null;
    }
    async init() {
      if (this.ready) return this.ready;
      this.ready = (async () => {
        const o = this.opts;
        this.memfs = new MemFS({ compileModule: o.compileModule, memfsBytes: await o.loadBytes('memfs.wasm'), onOutput: (k, s) => this.out.push([k, s]) });
        await this.memfs.ready;
        o.onStatus && o.onStatus('Unpacking C library…');
        new Tar(await o.loadBytes('sysroot.tar')).untar(this.memfs);
      })();
      return this.ready;
    }
    async module(name) {
      if (!this.modules[name]) { this.opts.onStatus && this.opts.onStatus(`Loading ${name === 'clang.wasm' ? 'compiler' : 'linker'}…`); this.modules[name] = this.opts.compileModule(await this.opts.loadBytes(name)); }
      return this.modules[name];
    }
    async exec(module, ...argv) { const app = new App(module, this.memfs, this.opts.now, ...argv); return app.run(); }

    /** Compile, link and run one C program. Returns { ok, phase, diagnostics, stdout, stderr, exitCode, timings }. */
    async runC(source, { stdin = '', std = 'gnu11', opt = '1', files = {} } = {}) {
      await this.init();
      const t0 = this.opts.now(); const result = { ok: false, phase: 'compile', diagnostics: '', stdout: '', stderr: '', exitCode: null, timings: {} };
      this.out = [];
      this.memfs.addFile('main.c', source);
      for (const [name, text] of Object.entries(files)) this.memfs.addFile(name, text);          // headers next to main.c (flat)
      const clang = await this.module('clang.wasm');
      this.opts.onStatus && this.opts.onStatus('Compiling…');
      let code = await this.exec(clang, 'clang', '-cc1', '-emit-obj', ...CLANG_ARGS, `-O${opt}`, `-std=${std}`, '-o', 'main.o', '-x', 'c', 'main.c');
      result.diagnostics = this.out.map((x) => x[1]).join(''); this.out = []; result.timings.compile = this.opts.now() - t0;
      if (code !== 0) return result;
      const t1 = this.opts.now(); const lld = await this.module('lld.wasm');
      this.opts.onStatus && this.opts.onStatus('Linking…'); result.phase = 'link';
      code = await this.exec(lld, 'wasm-ld', '--no-threads', '--export-dynamic', '-z', `stack-size=${1024 * 1024}`, '-Llib/wasm32-wasi', 'lib/wasm32-wasi/crt1.o', 'main.o', '-lc', '-o', 'main.wasm');
      result.diagnostics += this.out.map((x) => x[1]).join(''); this.out = []; result.timings.link = this.opts.now() - t1;
      if (code !== 0) return result;
      const prog = await this.opts.compileModule(this.memfs.getFileContents('main.wasm').slice());
      result.phase = 'run'; this.opts.onStatus && this.opts.onStatus('Running…'); this.memfs.setStdin(stdin);
      const t2 = this.opts.now();
      try { result.exitCode = await this.exec(prog, 'main'); } catch (e) { result.stderr += `\n[runtime error] ${e.message}\n`; result.exitCode = -1; }
      result.timings.run = this.opts.now() - t2;
      for (const [k, s] of this.out) result[k] += s;
      this.out = []; result.ok = result.exitCode === 0; return result;
    }
  }

  root.CRunner = CRunner;
  if (typeof module !== 'undefined') module.exports = { CRunner };
})(typeof self !== 'undefined' ? self : globalThis);
