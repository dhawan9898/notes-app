---
title: Libraries and the Linker
track: c
module: Linking and Building
moduleOrder: 5
order: 1
level: advanced
minutes: 35
env: machine
summary: "Five exercises on the linking module: building the same library as a static archive and a shared object, reading symbol tables, fixing 'cannot open shared object file', controlling exported symbols with visibility, and the library-order link error, with a build script whose output is shown."
prerequisites:
  - "Static and Shared Libraries, Symbols, Dynamic Loading"
  - "Build Systems, Compiler Flags and Hardening Options"
sources:
  - label: "man 1 ld, man 1 nm, man 8 ld.so, man 1 ldd, man 1 ar"
    url: https://man7.org/linux/man-pages/man8/ld.so.8.html
  - label: Drepper — How To Write Shared Libraries
    url: https://www.akkadia.org/drepper/dsohowto.pdf
  - label: Levine — Linkers and Loaders (Morgan Kaufmann, 2000), Ch. 6-10
  - label: GCC manual — Code Gen Options (-fPIC, -fvisibility)
    url: https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html
---

Linker errors feel mysterious until you have broken things on purpose. These exercises use a tiny library (`mathx`: `mx_square` and `mx_cube`, with a private helper) and a program that calls it. Try each command yourself; the output below comes from one real run of the build script.

## Exercises

**1. A static library.** Compile `mathx.c` into an object file, pack it into `libmathx.a` with `ar`, and link `app.c` against it. Use `nm` to list the symbols. What do `T` and `t` mean, and why is `helper_mul` lowercase?

**2. A shared library.** Build `libmathx.so` and link the program against it. Running it fails with "cannot open shared object file". Show the failing command, `ldd`, and fix it **two** different ways without copying files into system directories. Which is better for something you ship?

**3. What is exported?** List the symbols a shared library exports with `nm -D`. What happens to the list when you compile with `-fvisibility=hidden`, and how do you re-export just the API?

**4. Order matters.** With a static library, why does `gcc -o bad -lmathx -L. app.c` fail with "undefined reference" while `gcc -o ok -L. app.c -lmathx` works?

**5. Short answers.** (a) Name two advantages of shared libraries and two of static linking. (b) Why does a shared library need `-fPIC`? (c) What does the dynamic loader search, in order, to find `libmathx.so`?

## Answers

Try before opening.

<details>
<summary>Show the files, the script and its output</summary>

The library and the program:

{{include c/practice/lib/mathx.h}}

{{include c/practice/lib/mathx.c}}

{{include c/practice/lib/app.c}}

The script that builds everything, with every command echoed:

{{include c/practice/lib/build_demo.sh}}

{{run c/practice/lib :: ./build_demo.sh}}

**1.** `ar rcs` creates the archive (an index of object files); the linker copies **only the object files that resolve unresolved symbols** into the executable, so the program is self-contained. In `nm` output, `T` is a global symbol in the **text** (code) section, available to other object files; lowercase `t` is a **local** symbol, visible only inside its own object file. `helper_mul` is declared `static` in the C source, so the compiler marks it local: it cannot clash with another file's `helper_mul` and nothing outside can call it.

**2.** Building works, but `./app_shared` fails at **run time**: the *link-time* flag `-L.` only tells the linker where to find `libmathx.so`; the dynamic loader (`ld.so`) knows nothing about it, and `ldd` shows `libmathx.so => not found`. Two fixes that need no system directory: set **`LD_LIBRARY_PATH=.`** for one run (convenient for testing, but it is an environment setting that every user and script must remember, and it can be hijacked), or link with **`-Wl,-rpath,'$ORIGIN'`**, which records in the executable "look in my own directory first" (`$ORIGIN` is the directory of the program, so the pair can be moved together). For shipped software the rpath (or installing into a directory known to `ld.so.conf` and running `ldconfig`) is better: it makes the program work without relying on the user's environment. Quote `'$ORIGIN'` so the shell does not expand it.

**3.** Without options every non-static function is exported (`mx_cube` and `mx_square`; the static helper is not). With `-fvisibility=hidden` the count of exported functions drops to **0**: everything is hidden by default, including the API you meant to publish. Re-export deliberately by marking the public functions `__attribute__((visibility("default")))` (usually behind a macro such as `MATHX_API`). A small exported surface gives faster loading (fewer symbols to resolve), prevents programs from depending on internals, and avoids symbol clashes between libraries.

**4.** The linker processes inputs **left to right** and, for a static archive, pulls in only the members needed to satisfy symbols that are *already unresolved* at that point. With `-lmathx` before `app.c` nothing needs `mx_square` yet, so no member is extracted; `app.c` then adds the unresolved references, and no later input provides them: "undefined reference to `mx_cube`". Put libraries **after** the objects that use them (`app.c -lmathx`). With several interdependent libraries, order them so dependents come first (a library that uses another goes before it), or wrap them in `-Wl,--start-group ... -Wl,--end-group`. Shared libraries are less sensitive on most systems, but the same order is the safe habit.

**5.** (a) *Shared:* one copy of the code in memory and on disk for many programs; a security or bug fix needs only the library replaced. *Static:* the executable is self-contained (no missing-library or version problems), and the linker can drop unused code and optimise across the boundary. (b) A shared library is mapped at **different addresses** in different processes, so its code must not contain absolute addresses: `-fPIC` generates **position-independent code** that uses relative addressing and a table of addresses fixed up at load time. (c) The loader searches: `DT_RPATH` (old) / `DT_RUNPATH` entries recorded in the executable (including `$ORIGIN`), the `LD_LIBRARY_PATH` environment variable (ignored for setuid programs), the cache built from `/etc/ld.so.conf` by `ldconfig`, and finally the default directories `/lib` and `/usr/lib`. (The exact order of `LD_LIBRARY_PATH` versus `RUNPATH` differs between the two entry kinds; `ld.so(8)` documents it.) `LD_DEBUG=libs ./app` prints the search as it happens.

</details>

## Key takeaways

1. A static archive contributes only the object files that satisfy unresolved symbols; `nm` shows `T` (global) and `t` (file-local) symbols, and `static` functions stay local.
2. `-L` is for the linker; the loader needs an rpath (`$ORIGIN`), `ldconfig` or `LD_LIBRARY_PATH` to find a shared library at run time; `ldd` shows what is missing.
3. Limit exported symbols with `-fvisibility=hidden` and mark the API explicitly.
4. Libraries go after the objects that need them on the command line.
5. Shared libraries must be position-independent (`-fPIC`); the trade-off is shared updates versus self-contained binaries.
