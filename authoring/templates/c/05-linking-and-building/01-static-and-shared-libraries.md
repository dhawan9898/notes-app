---
title: Static and Shared Libraries, Symbols, Dynamic Loading
track: c
module: Linking and Building
moduleOrder: 5
order: 1
level: advanced
summary: Building and using .a and .so libraries, sonames and version scripts, visibility and exported symbols, how the PLT/GOT and ld.so bind symbols, RPATH/RUNPATH, LD_PRELOAD interposition and dlopen plugins — all demonstrated with real commands.
sources:
  - label: Drepper — How To Write Shared Libraries (2011)
    url: https://www.akkadia.org/drepper/dsohowto.pdf
  - label: man 8 ld.so, man 1 ld, man 3 dlopen, man 1 ar, man 5 elf
    url: https://man7.org/linux/man-pages/man8/ld.so.8.html
  - label: GNU ld manual — Version Scripts; -Bsymbolic; -z options
    url: https://sourceware.org/binutils/docs/ld/VERSION.html
  - label: System V ABI — Dynamic linking (PLT/GOT); ELF symbol versioning (LSB)
    url: https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.dynamic.html
  - label: GCC manual — Visibility Pragmas; Function Attributes (visibility)
    url: https://gcc.gnu.org/wiki/Visibility
  - label: Levine — Linkers and Loaders (Morgan Kaufmann, 1999)
---

A library packages compiled code for reuse. On Linux you choose between **static archives** (`libfoo.a`: copied into the executable at link time) and **shared objects** (`libfoo.so`: mapped into each process at run time). The choice affects binary size, security patching, startup time, memory sharing, ABI constraints and deployment. The running example is a small library:

{{include c/libs/mathx.h}}

{{include c/libs/mathx.c}}

{{include c/libs/app.c}}

## Static libraries

A static library is an `ar` archive of object files. The linker pulls in **only the members that resolve undefined symbols**, in command-line order.

{{run c/libs :: gcc -c -O2 -fPIC -fvisibility=hidden mathx.c -o mathx.o && ar rcs libmathx.a mathx.o && ar t libmathx.a && gcc app.c -L. -l:libmathx.a -o app_static && ./app_static && echo "mathx in ldd: $(ldd app_static | grep -c mathx)" && size app_static}}

- The executable contains the code (`ldd` shows no `mathx` dependency) and is self-contained with respect to this library.
- **Order matters**: `gcc app.o -lmathx` works, `gcc -lmathx app.o` leaves `mathx_*` undefined (the archive is scanned before the symbols are needed). Mutually dependent archives need `-Wl,--start-group ... --end-group`.
- Pros: no runtime dependency or version skew, simple deployment (single file), slightly faster calls (no PLT), link-time optimization across the boundary. Cons: bigger binaries, every program has its own copy (disk and page cache), **security fixes require relinking and redeploying every application**. Fully static executables (`-static`) are common for containers (`FROM scratch`) and Go/Rust; glibc static linking has caveats (NSS, `dlopen`, locales), so prefer musl for static builds.

## Shared libraries

A shared object must be compiled as **position-independent code** (`-fPIC`) because it can be mapped at any address in each process; it carries a **soname** identifying its ABI version.

{{run c/libs :: gcc -shared -fPIC -Wl,-soname,libmathx.so.1 mathx.o -o libmathx.so.1.2.0 && ln -sf libmathx.so.1.2.0 libmathx.so.1 && ln -sf libmathx.so.1 libmathx.so && ls -l libmathx.so* | awk '{print $1, $9, $10, $11}'}}

The three names have distinct jobs:

| Name | Purpose |
|---|---|
| `libmathx.so.1.2.0` | the real file: `major.minor.patch` |
| `libmathx.so.1` | **soname**: recorded in the library (`SONAME`) and in every executable linked against it (`NEEDED`); bump the *major* only for incompatible ABI changes |
| `libmathx.so` | the **linker name** used by `-lmathx` at build time (usually installed by the `-dev` package) |

{{run c/libs :: gcc app.c -L. -lmathx -o app_dyn; ./app_dyn 2>&1 | head -n 1; echo "ldd:"; ldd app_dyn | grep mathx; readelf -d app_dyn | grep -E 'NEEDED|RUNPATH|RPATH'; readelf -d libmathx.so | grep SONAME}}

The link succeeded (`-L.` found `libmathx.so` for the build), but the program **fails at startup**: the loader looks for the soname `libmathx.so.1` only in its search path. The loader (`ld.so`) searches, in order: `DT_RPATH` (deprecated, if no RUNPATH), `LD_LIBRARY_PATH`, `DT_RUNPATH`, the cache `/etc/ld.so.cache` (built by `ldconfig` from `/etc/ld.so.conf.d/*`), then default directories (`/lib`, `/usr/lib`). Fixes:

{{run c/libs :: LD_LIBRARY_PATH=. ./app_dyn; gcc app.c -L. -lmathx -Wl,-rpath,'$ORIGIN' -o app_rpath && ./app_rpath && readelf -d app_rpath | grep -E 'RUNPATH|RPATH'}}

- `LD_LIBRARY_PATH` is for testing and wrappers, **not** for production (it affects every child process and is ignored for setuid programs).
- `-Wl,-rpath,'$ORIGIN'` (or `$ORIGIN/../lib`) embeds a search path relative to the executable's own directory: relocatable application bundles. Quote the `$`. Modern linkers emit `RUNPATH`, which can be overridden by `LD_LIBRARY_PATH` (unlike `RPATH`).
- System libraries: install to `/usr/lib/<triplet>` and run `ldconfig`; `ldconfig -p | grep mathx` lists the cache.

Debug tools: `ldd` (**never run `ldd` on untrusted binaries**: some implementations execute them; use `objdump -p | grep NEEDED` or `readelf -d`), `LD_DEBUG=libs,files,bindings,symbols,statistics ./prog`, `lsof -p PID | grep .so`, `/proc/PID/maps`.

### Compatibility and the soname rule

The soname version expresses ABI. Compatible changes (adding functions, fixing bugs) keep the soname; incompatible ones (removing/renaming functions, changing struct layout or function signatures or semantics visible to clients) require a new major (`libmathx.so.2`) so old programs keep using `.so.1` side by side. Struct layout leaks through public headers: that is why libraries use opaque handles (previous note).

## Symbol visibility: export only the API

By default, every non-static function in a shared library is **exported** and can be called, interposed, or collide with symbols in other libraries. The library above deliberately contains an accidental global, `internal_but_global`. Compiling with `-fvisibility=hidden` and marking only the API `visibility("default")` (the `MATHX_API` macro) trims the export table:

{{run c/libs :: echo "exported with -fvisibility=hidden:"; nm -D --defined-only libmathx.so | awk '{print $2, $3}'; gcc -shared -fPIC mathx.c -o libmathx_all.so && echo "exported by default:" && nm -D --defined-only libmathx_all.so | awk '{print $2, $3}'}}

Benefits: smaller dynamic symbol tables, faster loading, calls within the library bind directly (no PLT indirection, enabling inlining), no accidental interposition, a smaller ABI surface to maintain. Alternatives: a **version script** or an export list (`-Wl,--version-script`, `-Wl,--exclude-libs,ALL`). `-Bsymbolic` binds internal references locally (faster, but breaks interposition).

### Symbol versioning

A version script both restricts exports and attaches **version nodes**, so glibc-style compatibility is possible (`memcpy@GLIBC_2.2.5` vs `memcpy@@GLIBC_2.14`): a library can keep an old implementation under the old version tag while new links bind to the new one.

{{include c/libs/ver.map}}

{{run c/libs :: gcc -shared -fPIC -Wl,--version-script=ver.map -Wl,-soname,libmathxv.so.1 mathx.c -o libmathxv.so && nm -D --defined-only libmathxv.so | awk '{print $2, $3}'; gcc app.c -L. -lmathxv -o app_v && readelf -rW app_v | grep mathx_square | awk '{print $3, $5}'}}

The client records the version it linked against (`mathx_square@MATHX_1.0`); a future `MATHX_2.0` can change `mathx_square`'s behavior while old binaries keep resolving to the 1.0 implementation (`.symver` directives pick which implementation each tag maps to). Check the symbols a binary requires with `objdump -T app | grep GLIBC_` to find the minimum glibc version it needs (a binary built on a new distro often won't run on an older one: build on the oldest platform you support, or in a container).

## How calls are bound: PLT and GOT

Code in a shared library or PIE can't contain absolute addresses of functions in other objects, since they're unknown until load time. The ELF mechanism:

- The **GOT** (global offset table) is a writable table of pointers, one slot per imported function/variable.
- The **PLT** (procedure linkage table) is a table of small stubs; a call to `mathx_square` is compiled as `call mathx_square@plt`, and the stub does `jmp *GOT[slot]`.
- **Lazy binding** (default unless `-z now`): the GOT slot initially points back into the PLT resolver; the first call asks `ld.so` to look up the symbol and patches the slot; later calls go straight through.

{{run c/libs :: objdump -d --no-show-raw-insn app_dyn | awk '/<mathx_square@plt>:/,/^$/' | head -n 4; echo "--- relocations that fill the GOT slots:"; readelf -rW app_dyn | grep -E 'mathx|printf' | awk '{print $1, $3, $5}'; echo "--- bindings performed by ld.so:"; LD_LIBRARY_PATH=. LD_DEBUG=bindings ./app_dyn 2>&1 | grep -E 'mathx_(square|cube)' | sed 's/^ *[0-9]*:\s*//' | cut -c1-110}}

`R_X86_64_JUMP_SLOT` relocations describe those GOT slots. Consequences:

- Each cross-library call costs an extra indirect jump (a few cycles); data accesses to imported variables go through the GOT.
- **Security**: a writable GOT lets an attacker with a write primitive redirect calls. Hardening: `-Wl,-z,relro,-z,now` (**full RELRO**) resolves all symbols at startup and makes the GOT read-only after relocation, at the cost of slightly slower startup. Many distributions now default to this (PIE + full RELRO + BIND_NOW).
- Startup cost scales with the number of relocations and libraries; `LD_DEBUG=statistics` prints them. Prelinking is obsolete; mitigations are `-fvisibility=hidden`, fewer libraries, `-Wl,-O1,--hash-style=gnu`, lazy loading via `dlopen`.

## Interposition with `LD_PRELOAD`

Because symbol lookup searches libraries in order, a library listed in `LD_PRELOAD` is searched **first**, letting it override functions of other libraries (including libc). With `dlsym(RTLD_NEXT, ...)` it can wrap the original:

{{include c/libs/hook.c}}

{{run c/libs :: gcc -O0 target.c -o target && gcc -shared -fPIC hook.c -o hook.so -ldl && LD_PRELOAD=./hook.so ./target}}

The wrapper counted six `malloc` calls (the loop's five plus one made by stdio for `puts`'s buffer), without recompiling the program. Uses: leak and call tracing, fault injection, `libfaketime`, allocators (`jemalloc`, `tcmalloc`), `libSegFault`. Limits: it doesn't affect statically linked binaries or direct syscalls, is disabled for setuid programs, and interposing from within a library only works if calls cross the PLT (not inlined or `-Bsymbolic`). Security tools must treat `LD_PRELOAD` and `/etc/ld.so.preload` as persistence/rootkit vectors.

## Run-time loading: `dlopen`

Plugins and optional features are loaded by name at run time:

{{include c/libs/host.c}}

{{run c/libs :: gcc -shared -fPIC plugin.c -o plugin.so && gcc host.c -o host -ldl && ./host && ./host ./missing.so; echo "exit status: $?"}}

API: `dlopen(path, flags)` (`RTLD_NOW` resolves everything immediately and reports errors early, `RTLD_LAZY`, `RTLD_LOCAL` keeps the plugin's symbols private, `RTLD_GLOBAL` makes them available to later loads), `dlsym(handle, "name")`, `dlerror()` (returns a message once), `dlclose` (unloads when the refcount reaches 0: unloading code that left threads, `atexit` handlers or registered callbacks running is a classic crash). Rules for plugin systems: define a small stable C ABI (a registration struct with a version field), use `extern "C"` for C++ plugins, avoid sharing memory ownership across the boundary (plugin allocates → plugin frees, or use a common allocator), mind different `libc`/runtime copies, and check the `ELF` `NEEDED` set. Constructors/destructors in libraries (`__attribute__((constructor))`, `.init_array`) run at load/unload.

## Choosing static vs. shared

| Consideration | Static | Shared |
|---|---|---|
| Deployment | one self-contained file | must ship/verify the right `.so` versions |
| Security patching | rebuild everything | replace the library once (distros rely on this) |
| Memory | duplicated per program | code pages shared across processes |
| Startup | fastest | dynamic loading and relocation |
| Plugin support | no | `dlopen` |
| Licensing | LGPL static linking implies obligations | LGPL friendly |
| Version skew risk | none | "DLL hell" (`GLIBC_2.xx not found`) |
| Container images | great for `scratch`/distroless | needs matching base image |

Guidelines: libraries meant to be shared across many programs and updated independently → shared with a stable ABI; self-contained tools and deployment simplicity → static (or bundle the `.so` files with `$ORIGIN` rpath); always build with hardening flags (PIE, full RELRO, stack protector) and strip debug info into separate files for release.

## Key takeaways

- Static archives are linked member-by-member in command-line order; shared libraries use `-fPIC`, a **soname** for ABI identity, and the loader's search order (`RUNPATH`, `LD_LIBRARY_PATH`, `ldconfig` cache).
- Export only the public API (`-fvisibility=hidden`, version scripts); symbol versioning lets one library serve old and new binaries.
- Calls to shared functions go through the PLT/GOT with lazy binding; full RELRO (`-z relro -z now`) hardens the GOT.
- `LD_PRELOAD` and `dlopen` provide interposition and plugins; use them knowingly, and treat them as attack surface.
- Use `readelf -d`, `nm -D`, `objdump -T`, `ldd` (on trusted files), and `LD_DEBUG` to diagnose loading and binding problems.
