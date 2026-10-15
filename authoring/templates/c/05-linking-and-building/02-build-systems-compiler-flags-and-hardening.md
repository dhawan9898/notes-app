---
title: Build Systems, Compiler Flags and Hardening Options
track: c
module: Linking and Building
moduleOrder: 5
order: 2
level: intermediate
summary: A correct, incremental Makefile with automatic header dependencies, the equivalent CMake project, the warning and optimization flags worth using, and a measured comparison of default versus hardened builds (PIE, RELRO, canaries, FORTIFY, NX).
prerequisites:
  - Functions, Headers and Multi-File Programs
  - Static and Shared Libraries, Symbols, Dynamic Loading
sources:
  - label: GNU Make Manual
    url: https://www.gnu.org/software/make/manual/make.html
  - label: CMake documentation — Tutorial; Modern CMake targets and properties
    url: https://cmake.org/cmake/help/latest/guide/tutorial/index.html
  - label: GCC manual — Warning Options; Optimize Options; Instrumentation Options; Link Options
    url: https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
  - label: OpenSSF — Compiler Options Hardening Guide for C and C++
    url: https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
  - label: Reproducible Builds project — SOURCE_DATE_EPOCH, -ffile-prefix-map
    url: https://reproducible-builds.org/docs/source-date-epoch/
  - label: pkg-config guide; Meson and Ninja manuals
    url: https://www.freedesktop.org/wiki/Software/pkg-config/
---

A build system answers one question precisely: *given what changed, what must be rebuilt, and with which commands?* Getting it right gives fast incremental builds, reproducible results, and a place to enforce warnings, sanitizers and hardening flags consistently.

## The project

A tiny two-file program (`src/main.c`, `src/util.c`, `include/util.h`) used throughout:

{{include c/build/src/main.c}}

{{include c/build/src/util.c}}

## Make

`make` reads rules `target: prerequisites` + recipe, and rebuilds a target when any prerequisite is newer. For C the prerequisites of an object file must include **every header it includes**, or editing a header silently leaves stale objects (the classic "I changed the struct and the program crashes" bug). Compilers can generate these dependencies themselves (`-MMD -MP` write a `.d` file next to each object):

{{include c/build/Makefile}}

{{run c/build :: make 2>&1 | tail -n 4; echo '--- second make:'; make; echo '--- touch a header, rebuild:'; touch include/util.h; make 2>&1 | grep -c 'cc '; echo '--- generated dependency file:'; head -n 3 build/util.d; make clean >/dev/null}}

What to notice:

- Only changed translation units are recompiled; touching the header rebuilt both objects that include it (the `.d` files record that dependency automatically, `-include $(DEPS)` loads them).
- `$@` target, `$<` first prerequisite, `$^` all prerequisites, `%` pattern rules, `$(wildcard ...)`, `| $(BUILD)` order-only prerequisite (the directory must exist but its timestamp doesn't trigger rebuilds), `.PHONY` for non-file targets.
- `CFLAGS ?=` lets users override from the environment (`make CFLAGS="-O0 -g"`); `+=` appends project requirements; follow conventions (`CC`, `CFLAGS`, `CPPFLAGS`, `LDFLAGS`, `LDLIBS`, `DESTDIR`, `PREFIX`) so packagers can drive your build.
- Parallelism: `make -j$(nproc)` (correct only if dependencies are complete). `make -n` shows what would run; `make V=1` conventions; `make -p` dumps the database.
- Out-of-tree builds (`build/`), `clean`, `install`, `check` targets, and `compile_commands.json` (via `bear -- make` or CMake) feed editors, clang-tidy and clangd.

## CMake

CMake generates build files (Makefiles, Ninja) from a declarative description centred on **targets** with usage requirements that propagate:

{{include c/build/CMakeLists.txt}}

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # configure (generate)
cmake --build build -j                                     # build
ctest --test-dir build --output-on-failure                 # tests
cmake --install build --prefix /usr/local                  # install
```

{{run c/build :: cmake -S . -B cb -G 'Unix Makefiles' >/dev/null 2>&1 && cmake --build cb 2>&1 | tail -n 3 && ctest --test-dir cb 2>&1 | grep -E 'tests passed|Test +#'; rm -rf cb}}

Good practice: `target_*` commands over global flags; `PUBLIC/PRIVATE/INTERFACE` to describe what dependents inherit; `find_package`/`pkg_check_modules` for dependencies; build types (`Debug`, `Release`, `RelWithDebInfo`) set `-O`/`-g`; `CMAKE_EXPORT_COMPILE_COMMANDS=ON`; sanitizer and hardening options as cache variables; `FetchContent` or a package manager (Conan, vcpkg) for third-party code. Alternatives: **Meson + Ninja** (fast, readable), **Bazel** (monorepos, hermetic), **Autotools** (`./configure && make`, legacy but ubiquitous), plain Make for small projects.

**pkg-config** finds compile/link flags of installed libraries: `pkg-config --cflags --libs zlib` → `-lz`; use `$(shell pkg-config --cflags --libs libfoo)` in Make, `pkg_check_modules` in CMake. Libraries ship `.pc` files in `/usr/lib/x86_64-linux-gnu/pkgconfig/`.

## Compiler flags that pay for themselves

### Warnings

A project should compile warning-free with at least:

```text
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Wformat-security
-Wnull-dereference -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-align -Wwrite-strings
-Wvla -Wdouble-promotion -Wswitch-enum -Walloca -Wstack-usage=65536
```

- Treat warnings as errors (`-Werror`) in CI for your own code, not for end-user source builds (new compiler versions add warnings; use `-Wno-error` or leave it to CI).
- `-fanalyzer` (GCC 10+) performs interprocedural static analysis (use-after-free, double free, leaks, null dereference).
- Fix warnings rather than silencing them; use targeted `#pragma GCC diagnostic` or `(void)` casts with a comment.

### Optimization and debug info

| Flag | Effect |
|---|---|
| `-O0` | no optimization (fast compile, debug-friendly) |
| `-O1`, `-O2` | standard optimization levels; `-O2` is the usual release choice |
| `-O3` | adds aggressive vectorization/unrolling; sometimes slower due to code size |
| `-Os`, `-Oz` | optimize for size |
| `-Og` | optimize for debugging experience |
| `-march=native` / `-mtune=generic` | use the host's instruction set (not portable!) / tune only. Distribute binaries with a baseline (`-march=x86-64-v2/v3`) |
| `-flto` | link-time optimization across files (slower link, better inlining) |
| `-fprofile-generate` / `-fprofile-use` | profile-guided optimization |
| `-g`, `-g3`, `-gz`, `-ggdb` | debug info (does not change generated code); keep it in a separate file for release (`objcopy --only-keep-debug`) |
| `-fno-omit-frame-pointer` | keep frame pointers for profiling/unwinding |
| `-ffunction-sections -fdata-sections -Wl,--gc-sections` | let the linker drop unused code |
| `-fno-strict-aliasing`, `-fwrapv`, `-fno-delete-null-pointer-checks` | restrict UB-based optimization (legacy code, kernels) |
| `-pipe`, `-ccache`, `-j` | faster builds |

Optimization level changes behavior only for programs with undefined behavior or unspecified results (floating-point `-ffast-math` aside), so a program that "works at -O0 and breaks at -O2" almost always contains UB; use sanitizers.

### Sanitizers (build variants)

`-fsanitize=address,undefined -fno-omit-frame-pointer -g` (ASan + UBSan) for tests; `-fsanitize=thread` separately (cannot be combined with ASan); `-fsanitize=leak`; Clang adds `memory` and `integer`. Expect ~2× slowdown. Make them one CMake option or Make variable and run the test suite under each in CI.

## Hardening: default vs. hardened builds

Modern toolchains can make exploitation of memory bugs harder (they don't fix the bugs). The checking script reads the ELF headers and symbols (`checksec.sh`, a minimal subset of the `checksec` tool):

{{include c/build/checksec.sh}}

Three builds of the same program:

```bash
gcc -O0 -no-pie -fno-stack-protector -Wl,-z,norelro -Wl,-z,execstack ...             # deliberately weak ("weak")
gcc -O2 ...                                                                           # distribution defaults on this host ("default")
gcc -O2 -fstack-protector-strong -D_FORTIFY_SOURCE=3 -fPIE -pie -fstack-clash-protection \
    -fcf-protection=full -Wl,-z,relro,-z,now,-z,noexecstack ...                       # recommended ("hardened")
```

{{run c/build :: S='src/main.c src/util.c -Iinclude'; gcc -O0 -fno-pie -no-pie -fno-stack-protector -U_FORTIFY_SOURCE -Wl,-z,norelro -Wl,-z,execstack $S -o weak 2>/dev/null; gcc -O2 $S -o default; gcc -O2 -fstack-protector-strong -D_FORTIFY_SOURCE=3 -fPIE -pie -fstack-clash-protection -fcf-protection=full -Wl,-z,relro,-z,now,-z,noexecstack $S -o hardened; for b in weak default hardened; do ./checksec.sh $b; done; rm -f weak default hardened}}

| Flag | Mitigation | Stops / hinders |
|---|---|---|
| `-fPIE -pie` | position-independent executable → ASLR of the program image | return-to-code attacks needing fixed addresses |
| `-Wl,-z,relro` (`-z now` = full) | GOT/`.init_array` read-only after relocation | GOT overwrite |
| `-fstack-protector-strong` | stack canaries | linear stack-buffer overflows |
| `-D_FORTIFY_SOURCE=2/3` (requires `-O1+`) | bounds-checked `memcpy/strcpy/sprintf/read` when sizes are known; `=3` also uses dynamic object sizes (GCC 12+) | overflows in libc string/memory calls, `%n` in writable formats |
| `-Wl,-z,noexecstack` | non-executable stack | shellcode on the stack |
| `-fstack-clash-protection` | probes pages of large frames | stack clash |
| `-fcf-protection=full` | Intel CET: indirect-branch tracking (ENDBR64) + shadow stack compatibility | ROP/JOP |
| `-ftrivial-auto-var-init=zero` | zero uninitialized locals | info leaks, uninit bugs |
| `-fsanitize=bounds,undefined` with minimal runtime (`-fsanitize-minimal-runtime`) / `-fsanitize=cfi` (Clang+LTO) / `-fsanitize=shadow-call-stack` (arm64) | production-grade runtime checks | UB exploitation, indirect call hijacking |
| `-Wl,-z,separate-code`, `-Wl,--as-needed`, `-Wl,-z,noexecheap` | tighter segments, smaller deps | |
| `-fno-plt`, `-fvisibility=hidden` | fewer PLT stubs/exports | attack surface |
| `-D_GLIBCXX_ASSERTIONS` (C++), `-fstrict-flex-arrays=3` | container bounds assertions, stricter flexible arrays | |

Many distributions (Debian, Ubuntu, Fedora, Arch) already enable PIE, stack protector, FORTIFY and relro by default, as the `default` row shows partially on this Debian host (PIE and partial RELRO, but no canary: Debian doesn't enable `-fstack-protector-strong` by default whereas Ubuntu does). Use `hardening-check`, `checksec`, or `scanelf` to audit release artifacts, and make the flags part of your build presets so developers can't forget them.

## Reproducible and cacheable builds

- Embedded paths, timestamps and ordering leak into binaries: use `-ffile-prefix-map=$PWD=.`, avoid `__DATE__/__TIME__`, set `SOURCE_DATE_EPOCH`, fix archive ordering (`ar D`), and sort inputs.
- `ccache`/`sccache` cache object files by preprocessed input and flags; `distcc` distributes compilation; unity builds and precompiled headers reduce parsing time.
- Pin the toolchain (container image, `nix`, `guix`), record flags (`gcc -frecord-gcc-switches`), embed a version (`git describe`) via `-DVERSION=...`.

## Cross-compilation basics

Use a target triple toolchain (`aarch64-linux-gnu-gcc`), a sysroot (`--sysroot=`), a CMake toolchain file (`CMAKE_SYSTEM_NAME`, `CMAKE_C_COMPILER`, `CMAKE_FIND_ROOT_PATH`), and `qemu-user` (`qemu-aarch64 -L /usr/aarch64-linux-gnu ./a.out`) to run tests. Beware host/target differences in `sizeof(long)`, endianness, `char` signedness, alignment, and `configure` checks that try to run target binaries.

## A suggested project layout

```text
.
├── CMakeLists.txt / Makefile
├── include/mylib/*.h        # public headers
├── src/*.c  src/*.h         # implementation, private headers
├── tests/*.c                # unit tests, run under sanitizers
├── tools/ examples/ docs/
├── .clang-format  .clang-tidy  .editorconfig
└── .github/workflows/ci.yml # matrix: gcc/clang × Debug/Release × sanitizers × OS
```

## Key takeaways

- Make rebuilds based on timestamps: generate header dependencies with `-MMD -MP` or edits to headers won't recompile dependents. CMake (with Ninja) describes targets and propagates usage requirements; pkg-config supplies library flags.
- Compile with a strong warning set, run sanitizers in CI, and treat optimization-dependent bugs as undefined behavior.
- Harden releases: PIE, full RELRO, stack protector, `_FORTIFY_SOURCE=3`, non-executable stack, CET, stack-clash protection; verify with a script like `checksec.sh`.
- Make builds reproducible (`-ffile-prefix-map`, `SOURCE_DATE_EPOCH`) and cache them (ccache); pin toolchains.
