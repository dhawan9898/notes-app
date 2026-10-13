---
title: "The C Standard Library: A Tour of What to Use, What to Avoid and Why"
track: c
module: Systems-Level C
moduleOrder: 7
order: 8
level: intermediate
summary: "A working tour of the C library by header: number parsing with strtol (not atoi), the qsort comparator overflow, string function traps (strncpy, snprintf, strtok), memmove, time and time zones, locale effects, errno rules, printf formats and getline, with every claim backed by a small program and its output."
prerequisites:
  - "C From Zero: Your First Programs"
  - Strings and Buffer Safety
  - "File I/O in C: stdio vs POSIX, Buffering, Short Reads and Durability"
sources:
  - label: ISO/IEC 9899:2018 (C17) §7 — Library (N2176 draft)
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf
  - label: POSIX.1-2017 — System Interfaces (strtol, strsep-like functions, getline, strftime, timegm status)
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: glibc manual — Character Handling, String and Array Utilities, Date and Time, Locales
    url: https://www.gnu.org/software/libc/manual/html_node/index.html
  - label: SEI CERT C Coding Standard — INT, STR, ERR and MSC rules
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: man7.org — errno(3), strtol(3), snprintf(3), strtok(3), qsort(3), getline(3), strftime(3)
    url: https://man7.org/linux/man-pages/dir_section_3.html
---

The C standard library is small, old and sharp. Many of its functions were designed before anyone worried about buffer sizes, threads or non-English text, and the standard keeps them as they were. Knowing which of them are safe in which situations is a large part of writing reliable C. This note walks the library by area and runs the code that demonstrates each trap. For the headers' contents, `man 3 <function>` and the glibc manual are the references; here the focus is on **decisions**: what to call, what to avoid, and what each failure looks like.

## The headers at a glance

| Header | Contents | One thing to remember |
|---|---|---|
| `<stdio.h>` | `printf` family, `fopen`/`fread`/`fgets`, `FILE` buffering | `printf` formats are not type-checked at compile time unless you enable `-Wformat`; see I/O note for buffering |
| `<stdlib.h>` | `malloc` family, `strtol`/`strtod`, `qsort`/`bsearch`, `rand`, `exit`/`atexit`, `getenv` | `atoi` and `atof` cannot report errors |
| `<string.h>` | `mem*` and `str*` functions | the `strn*` functions are *not* "safe versions" |
| `<ctype.h>` | `isalpha`, `toupper` ... | arguments must be `unsigned char` values or `EOF` |
| `<math.h>` | `sqrt`, `pow`, `fabs`, `isnan`... | link with `-lm`; floating-point is inexact and `NaN != NaN` |
| `<time.h>` | `time`, `strftime`, `mktime`, `clock_gettime` (POSIX) | `localtime` and `gmtime` return shared static storage |
| `<errno.h>` | `errno`, error constants | meaningful only immediately after a failing call |
| `<stdint.h>`, `<inttypes.h>`, `<limits.h>`, `<stddef.h>`, `<stdbool.h>` | fixed-width types, limits, `size_t`, `bool` | use `PRId64`-style macros to print `int64_t` |
| `<assert.h>` | `assert` | compiled out when `NDEBUG` is defined: never put side effects inside |
| `<signal.h>`, `<setjmp.h>`, `<locale.h>`, `<stdarg.h>` | see their own notes | |

## Converting text to numbers

`atoi`, `atol` and `atof` return 0 for garbage and have undefined behavior on overflow; they cannot tell "0" from "abc". Use `strtol`, `strtoul`, `strtod` and friends, which report the position where parsing stopped (`endptr`) and signal range errors through `errno`. A correct wrapper checks three things: that at least one digit was consumed, that nothing but whitespace follows, and that `errno` did not become `ERANGE`.

{{include c/stdlib/numbers.c}}

{{run c/stdlib :: gcc -O2 -Wall -Wextra numbers.c -o numbers && ./numbers}}

Observations:

- `atoi("abc")` and `atoi("12abc")` give 0 and 12 with no way to detect the problem.
- `strtol` with **base 0** recognises `0x` (hex) and leading `0` (octal); with base 10 the `0x1f` string stops at the `x`. Be careful when parsing user input that might start with a zero: with base 0 `"010"` means 8.
- The **`qsort` comparator `a - b`** is a classic bug: for `INT_MAX` and `INT_MIN` the subtraction overflows (undefined behavior), so the first output is not sorted. Return `(a > b) - (a < b)` instead. Note too that `qsort` is not guaranteed stable, and `bsearch` requires an array sorted with the *same* comparator.

## Strings and memory functions

{{include c/stdlib/strings.c}}

{{run c/stdlib :: gcc -O2 -Wall -Wextra -Wno-stringop-truncation -Wno-format-truncation strings.c -o strings && ./strings}}

Lessons from each line:

- **`strncpy` is not a safe `strcpy`.** It pads with zeros if the source is shorter than the count, but if the source is as long or longer it copies exactly *n* bytes and writes **no terminator**; the result is an unterminated string. Use `snprintf(dst, sizeof dst, "%s", src)` or `memccpy`/`strlcpy` where available (`strlcpy` is in glibc 2.38 and later and in BSD libcs), and always terminate explicitly.
- **`snprintf` returns the length it *would* have written**, not the length it stored. Compare the return value to the buffer size to detect truncation. Never use the return value as the number of bytes written without that check. It never writes past the buffer and always terminates (for size > 0).
- **`strtok` keeps hidden state** (not thread-safe, not reentrant), modifies its input and collapses runs of delimiters, so empty fields disappear. Use `strtok_r` in threaded code, or `strsep` (BSD and glibc, not ISO C) when empty fields matter, or write the few lines of `strchr`-based splitting yourself.
- **`memcpy` requires non-overlapping regions;** `memmove` handles overlap. Using `memcpy` on overlapping buffers is undefined behavior that often works until an optimizer or a different libc breaks it.
- **`<ctype.h>` functions take an `int` that must be representable as `unsigned char` or equal to `EOF`.** Passing a plain `char` holding a negative value (any byte above 127 when `char` is signed) is undefined behavior. Cast: `isalpha((unsigned char)c)`. They also depend on the current locale.
- `strlen` walks the string: calling it in a loop condition (`for (i = 0; i < strlen(s); i++)`) is quadratic. `sizeof("hello")` is 6 because it counts the terminator, but `sizeof` of a pointer parameter is the pointer's size.
- Also: `strcpy`, `strcat` and `sprintf` and `gets` (removed in C11) have no length checks; compare strings with `strcmp` and test the result against 0, not against 1 or -1; `strerror` returns a shared buffer on some libcs, `strerror_r` has two incompatible versions (GNU and POSIX), so check which one you get.

## Time, time zones and locale

{{include c/stdlib/timeloc.c}}

{{run c/stdlib :: gcc -O2 -Wall -Wextra timeloc.c -o timeloc && ./timeloc}}

- `time_t` is seconds since 1970-01-01 UTC (on Linux a 64-bit value, so the year-2038 problem applies only to 32-bit `time_t` systems). The same instant prints differently in different zones: the **zone is a property of the formatting, not of the instant**. Store and transmit instants as UTC timestamps; convert for display.
- `gmtime`/`localtime` return a pointer to a shared static `struct tm`: two calls overwrite each other and threads race. Use `gmtime_r` and `localtime_r`.
- **`mktime` treats the `struct tm` as local time**; `timegm` (a common extension, not in ISO C) treats it as UTC. Mixing them is a frequent off-by-timezone bug. `mktime` also **normalizes** out-of-range fields (30 February becomes 1 March), which is useful for date arithmetic but means it never reports an invalid date.
- Measure durations with `CLOCK_MONOTONIC` (`clock_gettime`), which never jumps; the wall clock (`CLOCK_REALTIME`) can be stepped by NTP or an administrator. `clock()` measures CPU time, not elapsed time. `time()` has one-second resolution.
- **Locale.** Programs start in the `"C"` locale, but `setlocale(LC_ALL, "")` adopts the user's environment. Then `printf("%f")` and `strtod` use the locale's decimal separator, so a program that works in `en_US` can fail to parse `3.14` in a German locale (the example above prints what happens when that locale is installed; here it is not). Parse and write machine-readable numbers with `LC_NUMERIC` set to `"C"` (or use `uselocale` per thread), and keep locale-sensitive code (`toupper`, `strcoll`, `strftime` names) separate from protocol code.

## errno, printf and line input

{{include c/stdlib/errno_io.c}}

{{run c/stdlib :: gcc -O2 -Wall -Wextra errno_io.c -o errno_io && ./errno_io}}

- **`errno` is a per-thread variable that library functions set on failure and never clear on success.** Rules: only read it right after a call that *reported* failure (a NULL return, -1, ...); copy it before calling anything else (`printf` can change it); set it to 0 before calls such as `strtol` and `strtod` whose only error signal is `errno` (they return a valid value on overflow, e.g. `LONG_MAX`).
- `printf` returns the number of characters written, or a negative value on error. Format specifiers must match the argument types exactly: `%d` for `int`, `%ld` for `long`, `%zu` for `size_t`, `%lld` for `long long`, `PRId64` (from `<inttypes.h>`) for `int64_t`, and `%s` with `NULL` is undefined (glibc prints "(null)", others crash). Compile with `-Wformat=2`. Never pass user-controlled text as the format string (`printf(user_input)` is a format-string vulnerability); use `printf("%s", user_input)`.
- **`gets` is gone; `fgets` has the trailing newline and truncation problems; `scanf("%s")` has no limit.** `getline` (POSIX 2008) allocates and grows the buffer for you and returns the length (including the newline); free the buffer once at the end. It handles lines of any length, an empty line (a 1-byte result), and a last line without a newline.
- `malloc` can fail: a huge request returns `NULL` with `errno` set to `ENOMEM`. On Linux with overcommit, small requests rarely fail at allocation time and trouble appears later as the OOM killer; see the memory notes.

## Other functions worth knowing, in one place

- **`rand()`/`srand()`**: low-quality, global state, not thread-safe; quality varies by libc. For simulations use `random_r`/`drand48_r` or a small PRNG of your own (xorshift, PCG); for anything security-related use `getrandom(2)` or `/dev/urandom`, never `rand`.
- **`exit` vs `_exit` vs `abort`**: `exit` runs `atexit` handlers and flushes `stdio`; `_exit` terminates immediately (use it in a forked child after `exec` fails); `abort` raises `SIGABRT` and produces a core dump. Returning from `main` is `exit(return value)`.
- **`getenv` returns a pointer into the environment**; do not modify it, copy before `setenv`/`putenv` calls.
- **`system()`** runs `/bin/sh -c` and is a command-injection risk if any part of the string is user input; prefer `fork`+`execve` with an argument vector.
- **`qsort_r`, `bsearch`, `lfind`**: comparator signature rules above; sorting an array of structs by a `double` field needs care with `NaN` (comparison must be a consistent ordering).
- **`atexit` order** is last-registered-first-called; handlers must not call `exit`.
- **`<stdbool.h>`**: `bool` is `_Bool` (C23 makes `bool` a keyword); conversion to it normalizes to 0 or 1.
- **`<math.h>`**: `sqrt(-1)` returns `NaN` and sets `errno` only if `math_errhandling` says so; test with `isnan`; use `fabs` not `abs` for doubles (`abs` takes `int`).

## Key takeaways

- Parse numbers with `strtol`/`strtod` and check `endptr` and `ERANGE`; never `atoi`.
- `strncpy` does not guarantee termination, `strtok` has hidden state and drops empty fields, `snprintf` returns the length it wanted; know each function's real contract.
- `memcpy` forbids overlap, `<ctype.h>` wants `unsigned char`, and a `qsort` comparator must not subtract.
- Keep timestamps in UTC, convert for display, use `*_r` time functions and `CLOCK_MONOTONIC` for durations; keep protocol code out of the user's locale.
- `errno` is only valid right after a reported failure; copy it first and clear it before calls that signal errors through it alone.
