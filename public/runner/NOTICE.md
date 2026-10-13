# Third-party components of the in-app C runner

`clang.wasm`, `lld.wasm`, `memfs.wasm` and `sysroot.tar` are prebuilt artifacts from https://github.com/binji/wasm-clang:
- LLVM/Clang/LLD 8.0.1 (Apache License 2.0 with LLVM Exceptions), compiled to WebAssembly.
- wasi-libc sysroot (BSD-style licenses, see the wasi-libc project).
- memfs, the in-memory file system (Apache-2.0).

`cc.js` adapts that project's host code (Apache-2.0, Copyright 2020 WebAssembly Community Group participants).
The binaries are downloaded by `scripts/fetch-runner.mjs` and verified by SHA-256; they are not committed to git.
