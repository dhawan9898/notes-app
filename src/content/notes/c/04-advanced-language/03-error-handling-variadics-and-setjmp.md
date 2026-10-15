---
title: Error Handling, Variadic Functions and setjmp/longjmp
track: c
module: Advanced Language
moduleOrder: 4
order: 3
level: advanced
summary: Conventions for reporting and propagating errors in a language without exceptions — errno, return codes, the goto-cleanup idiom — plus how variadic functions work and fail, and what setjmp/longjmp really do.
prerequisites:
  - Stack Frames and the x86-64 System V Calling Convention
  - "Dynamic Memory: malloc Internals, Classic Bugs and Custom Allocators"
sources:
  - label: "ISO/IEC 9899:2018 (C17) §7.5 errno.h; §7.16 stdarg.h; §7.13 setjmp.h; §7.22.4 Communication with the environment; §7.2 assert.h"
  - label: "man 3 errno, man 3 strerror, man 3 perror, man 3 va_start, man 3 setjmp, man 3 atexit, man 3 exit, man 2 _exit"
    url: https://man7.org/linux/man-pages/man3/errno.3.html
  - label: "SEI CERT C — ERR30-C (set errno to zero before calling a library function), ERR33-C (detect and handle standard library errors), MSC22-C (setjmp/longjmp)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+12.+Error+Handling+%28ERR%29
  - label: "Linux kernel coding style — Chapter 7 Centralized exiting of functions (goto)"
    url: https://docs.kernel.org/process/coding-style.html#centralized-exiting-of-functions
  - label: "Hoare/Dijkstra era design notes; Kernighan & Pike — The Practice of Programming, Ch. 4 Interfaces"
---

C has no exceptions. Errors are reported by return values and the global-ish `errno`, and propagated by hand. The craft is to make that systematic enough that every failure is detected, every resource released, and every message useful.

## `errno`

`<errno.h>` provides `errno`, an `int` lvalue (a **thread-local** variable in glibc; a macro expanding to `(*__errno_location())`). Rules:

1. **Library calls set `errno` only on failure**, and only certain failures; success does *not* clear it. So the value is meaningful only **immediately after a call that reported failure** (a `-1`, `NULL`, `EOF` return). Reading `errno` after a successful call, or after an intervening call (even `printf`, which may itself fail and overwrite it), gives stale values.
2. For functions whose failure is ambiguous (`strtol`, `strtod`, `readdir`, `getc` variants, `ftell`), **set `errno = 0` before** the call and check afterwards.
3. Save it before cleanup that might modify it: `int saved = errno; close(fd); errno = saved;`.
4. Print with `strerror(errno)` (not thread-safe on old libcs; use `strerror_r`, or glibc's `%m` in `printf`/`syslog`) or `perror("open")`.
5. Common values: `ENOENT` (2) no such file, `EACCES` (13) permission denied, `EINTR` (4) interrupted by a signal (retry), `EAGAIN`/`EWOULDBLOCK` (11) try again, `ENOMEM` (12), `EINVAL` (22), `EEXIST`, `ENOSPC`, `EPIPE`, `ECONNRESET`, `ERANGE` (34) result out of range, `EIO`.

```text
fopen failed: errno=2 (No such file or directory)
strtol: v=9223372036854775807 errno=34 (Numerical result out of range)
```

(`errors.c` below). Note `strtol` clamped to `LONG_MAX` and set `ERANGE`; only `errno` distinguishes a genuine `LONG_MAX` from overflow.

## Conventions for reporting errors

| Convention | Used by | Notes |
|---|---|---|
| Return `-1` (or `NULL`) and set `errno` | POSIX syscalls, libc | one code space shared with the OS |
| Return `-errno` (negative errno) | Linux kernel | no global state, thread-safe, easy to propagate |
| Return an error enum / `status_t` and output through pointers | libraries (zlib `Z_*`, sqlite `SQLITE_*`, OpenSSL) | explicit, extensible; provide `xxx_strerror()` |
| Return a result struct `{ value, error }` | modern style | by-value, no out-parameters |
| Out-parameter + `bool` | simple helpers | |
| Sticky error state in an object, queried later | streams (`ferror`), `GError**` in GLib, sqlite handles | reduces clutter, delays detection |
| Abort/exit | programs for unrecoverable errors | libraries should not call `exit` |

Design guidelines:

- **Check every call that can fail** (`ERR33-C`): `malloc`, `fopen`, `fclose` (it can fail writing buffered data!), `fwrite`, `close`, `snprintf`, system calls. Unchecked errors turn into silent corruption.
- Make the *happy path* straight-line and errors early-exit.
- **Return codes vs. output**: don't mix valid results and error sentinels (`-1`) in one value unless the range excludes them; prefer `int f(T *out)`.
- Add context as errors bubble up: `open config /etc/app.conf: No such file or directory`; capture file/line/function in logging macros.
- Use `__attribute__((warn_unused_result))` (C23 `[[nodiscard]]`) on functions whose errors must not be ignored.
- Handle `EINTR` and short reads/writes in I/O loops (see the I/O note).
- Distinguish **recoverable** errors (bad input, I/O failure) from **bugs** (violated preconditions). Use `assert` for the latter in debug builds; return errors for the former. Don't `assert` on conditions influenced by input in code compiled with `-DNDEBUG` (asserts vanish, and so would any side effects inside them).

## The single-exit `goto cleanup` idiom

Early returns leak resources; deeply nested `if`s hide the logic. C programmers (the kernel coding style is explicit about it) use **one cleanup label** with all-or-nothing resource variables initialized to "not acquired":

```c
static status_t load_number(const char *path, long *out) {
    status_t rc = OK;
    FILE *f = NULL;
    char *line = NULL;
    size_t cap = 0;

    f = fopen(path, "r");
    if (!f) { rc = E_IO; goto out; }
    if (getline(&line, &cap, f) < 0) { rc = E_IO; goto out; }
    char *end;
    errno = 0;
    long v = strtol(line, &end, 10);
    if (end == line || errno == ERANGE) { rc = E_PARSE; goto out; }
    *out = v;
out:
    free(line);                      /* free(NULL) and the NULL check on f make the cleanup unconditional */
    if (f) fclose(f);
    return rc;
}
```

Why this works: every failure sets `rc` and jumps to `out`; cleanup code runs once, in reverse acquisition order; `free(NULL)` is a no-op and the `if (f)` guards the `fclose`. For several resources, use a ladder of labels (`err_free_b: free(b); err_free_a: free(a);`) to unwind precisely, or the single-label style if all handles are initialized to invalid values. `goto` is fine for forward jumps to cleanup; avoid it for loops and backward control flow. Don't jump over variable-length-array declarations or initializations.

The complete program, with its output:

```text
$ gcc -std=gnu11 -Wall -Wextra errors.c -o errors && ./errors
fopen failed: errno=2 (No such file or directory)
strtol: v=9223372036854775807 errno=34 (Numerical result out of range)
load_number -> ok, n=1234
load_number(/no/such/file) -> I/O error
average(3, 1.0, 2.0, 6.0) = 3
error: code 7 at startup
```

(`getline` returns −1 on error or EOF; an empty file would also take the `E_IO` branch, so decide whether that distinction matters.)

### Other idioms

- **Wrapper functions** for fatal-in-application errors: `void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { fputs("out of memory\n", stderr); abort(); } return p; }`. Appropriate in programs, wrong in libraries.
- **Check-and-propagate macros** (use sparingly; keep them obvious): `#define TRY(expr) do { int _e = (expr); if (_e) return _e; } while (0)`.
- **Defer-style cleanup**: `__attribute__((cleanup(fn)))` (GCC/Clang) runs `fn(&var)` when the variable leaves scope (used by systemd and GLib's `g_autoptr`); C2y proposes `defer`. Not portable to MSVC.
- **Exit codes**: `0` success, small positive integers for failures (0–255); `EXIT_SUCCESS`/`EXIT_FAILURE`. Shells treat 126/127/128+N specially.

## Terminating the program

| Call | Effect |
|---|---|
| `return n;` from `main` | same as `exit(n)` |
| `exit(n)` | runs `atexit` handlers (reverse order) and flushes/closes stdio streams, then `_exit` |
| `_exit(n)` / `_Exit(n)` | terminate immediately; **no** handlers, **no** stdio flush (use in children after `fork`, in signal handlers) |
| `abort()` | raises `SIGABRT` (core dump); no handlers; for unrecoverable internal errors |
| `quick_exit`, `at_quick_exit` (C11) | exit without full cleanup |
| `assert(cond)` | if false (and `NDEBUG` undefined): prints `file:line: func: Assertion ... failed` and `abort()`s |

Verified behavior (`assertdemo.c`):

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void bye(void) { puts("atexit handler ran"); }

int main(int argc, char **argv) {
    atexit(bye);
    int x = argc > 1 ? atoi(argv[1]) : 1;
    assert(x > 0 && "x must be positive");      /* compiled out with -DNDEBUG: never put side effects in assert */
    printf("x=%d\n", x);
    if (x == 2) exit(3);                        /* runs atexit handlers, flushes stdio */
    if (x == 3) _Exit(4);                       /* does NOT */
    if (x == 4) abort();                        /* raises SIGABRT; no handlers, core dump */
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra assertdemo.c -o assertdemo && for a in 2 3; do ./assertdemo $a; echo "exit status: $?"; done; ./assertdemo -1 2>&1 | head -n 1; echo "exit status: $?"
x=2
atexit handler ran
exit status: 3
exit status: 4
assertdemo: assertdemo.c:10: main: Assertion `x > 0 && "x must be positive"' failed.
exit status: 0
```

With `exit(3)` the `atexit` handler ran and output was flushed. With `_Exit(4)` **nothing was printed** — even the `x=3` line, since stdout was a pipe (fully buffered) and the buffer was discarded. The failed `assert` printed the expression (note the trick `&& "message"` to attach text) and aborted with status 134 (128 + `SIGABRT`).

## Variadic functions

`<stdarg.h>` lets a function accept a variable number of arguments (`printf`, `open`, `execl`). The callee has no type or count information beyond what you encode in the fixed parameters (a format string, a count, or a sentinel `NULL`):

```c
double average(int count, ...) {
    va_list ap;
    va_start(ap, count);                 /* last fixed parameter */
    double sum = 0;
    for (int i = 0; i < count; i++) sum += va_arg(ap, double);   /* you must name the correct type */
    va_end(ap);
    return count ? sum / count : 0;
}
```

How it works on x86-64 System V: the callee's prologue spills the argument registers into a save area, and `va_list` is a struct walking that area then the stack args. The caller sets `%al` to the number of vector registers used (so `call printf` is preceded by `mov $0,%eax`).

Hazards:

- **Default argument promotions** apply to the variable part: `float` → `double`, `char`/`short` → `int`. Hence `va_arg(ap, double)` (never `float`), `va_arg(ap, int)` for `char`/`short`. Passing `1` (an `int`) where `double` is expected, as in `average(2, 1, 2)`, is **undefined behavior**: the bits are read from the wrong register class (FP register vs integer register), producing garbage or the accidental right answer.
- No type checking: wrong format specifiers in `printf` are UB. Annotate your own printf-like wrappers with `__attribute__((format(printf, fmtindex, firstarg)))` so GCC checks callers (`-Wformat`), as `errorf` does in the example.
- `va_arg` past the end, `va_list` reuse after passing it to another function (`vprintf`), or missing `va_end` are UB. Use `va_copy` to traverse twice (the two-pass `vsnprintf` size-then-write pattern).
- Pass `NULL` sentinels as `(char *)NULL` (a bare `0` or `NULL` defined as `0` may be an `int`, wrong width in the variadic part); `execl("/bin/ls", "ls", "-l", (char *)NULL)`.
- Variadic functions can't be inlined as easily or checked statically; prefer a fixed API (arrays, structs) for new designs, or variadic *macros* that add type-safe layers.

## `setjmp` and `longjmp`

`setjmp(jmp_buf)` records the current execution context (stack pointer, callee-saved registers, program counter) and returns 0. `longjmp(buf, val)` restores it, so execution continues as if `setjmp` returned `val` (1 if `val` is 0): a **non-local goto** across function frames. It is C's closest thing to exceptions, used for error recovery in parsers/interpreters (libpng, Lua, libjpeg error handlers), coroutine tricks and signal recovery (`sigsetjmp`/`siglongjmp` to restore the signal mask).

```c
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf on_error;

static void parse_level(int depth) {
    if (depth == 3) longjmp(on_error, 42);          /* unwinds 3 frames at once; NO destructors/cleanup run */
    if (depth < 10) parse_level(depth + 1);
}

int main(void) {
    int plain_counter = 0;
    volatile int safe_counter = 0;                  /* locals modified after setjmp and read after longjmp must be volatile */
    char *buf = NULL;

    int code = setjmp(on_error);                    /* returns 0 first, then the longjmp value */
    if (code == 0) {
        buf = malloc(100);                          /* will leak: longjmp skips free */
        plain_counter = 1;
        safe_counter = 1;
        printf("first pass: calling parse_level\n");
        parse_level(0);
        puts("not reached");
    } else {
        printf("recovered with code %d, safe_counter=%d (plain_counter=%d is indeterminate in theory)\n", code, safe_counter, plain_counter);
        free(buf);                                  /* we kept the pointer, so we can clean up here */
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra longjmp.c -o longjmp && ./longjmp
first pass: calling parse_level
recovered with code 42, safe_counter=1 (plain_counter=1 is indeterminate in theory)
```

The jump unwound three nested `parse_level` frames at once. Rules and dangers:

1. **No cleanup runs** in the skipped frames: no `free`, `fclose`, unlocking, or restoring invariants. Resources acquired between `setjmp` and `longjmp` leak unless you record them somewhere reachable (the example kept `buf` in the frame that called `setjmp`).
2. Automatic variables modified after `setjmp` and read after `longjmp` have **indeterminate values** unless declared `volatile` (the compiler may have kept them in registers that `longjmp` restored). `plain_counter` happened to print `1`, but the standard doesn't promise it.
3. You may only `longjmp` to a function **that has not returned** (the frame must still exist), and the `jmp_buf` must have been set in the same thread. Jumping into a returned function is UB.
4. `setjmp` can only appear in restricted contexts (a whole controlling expression of `if`/`switch`/`while`, compared with a constant, or an expression statement), not as an arbitrary subexpression.
5. Signal masks: `setjmp` may not save them; use `sigsetjmp(buf, 1)`/`siglongjmp`. `longjmp` out of a signal handler is only safe if the signal didn't interrupt non-async-signal-safe code.
6. Combined with threads, locks and `malloc`, `longjmp` is a source of subtle corruption; many security guidelines (CERT MSC22-C) discourage it. Modern code prefers explicit error returns; in libraries, avoid `longjmp` across your public API.
7. `__builtin_setjmp` and `-fcf-protection`/shadow stacks (CET) treat `longjmp` specially (shadow-stack unwinding): it is supported by glibc.

## Logging errors usefully

- Log at the point where you have the most context, and **once** (avoid logging at every layer as the error propagates).
- Include: what operation, which object (path, host, id), the error string, and a stable error code; avoid logging secrets.
- Rate-limit repetitive messages; send to stderr (or syslog/journald) with severity.
- For libraries, return error info instead of printing, or invoke a user-provided logging callback.

## Key takeaways

- `errno` is valid only right after a failing call; clear it before ambiguous calls such as `strtol`; save it before cleanup; print with `strerror`/`%m`.
- Pick one error-reporting convention per library, check *every* fallible call (including `fclose`), and propagate with context.
- Use the single-exit `goto out` cleanup idiom (or `__attribute__((cleanup))`) so resources are released on every path.
- `exit` flushes and runs handlers; `_Exit`/`_exit` skip them (buffered output is lost); `abort` dumps core; `assert` is for bugs, not input errors.
- Variadic functions rely on correct types (promotions!) and conventions; annotate with `format`; `setjmp/longjmp` skips cleanup and needs `volatile` locals — use it sparingly.
