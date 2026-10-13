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
  - label: "ISO/IEC 9899:2018 (C17) §7 — Library (N2176 draft)"
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf
  - label: "POSIX.1-2017 — System Interfaces (strtol, strsep-like functions, getline, strftime, timegm status)"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "glibc manual — Character Handling, String and Array Utilities, Date and Time, Locales"
    url: https://www.gnu.org/software/libc/manual/html_node/index.html
  - label: "SEI CERT C Coding Standard — INT, STR, ERR and MSC rules"
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: "man7.org — errno(3), strtol(3), snprintf(3), strtok(3), qsort(3), getline(3), strftime(3)"
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

```c
// Converting text to numbers: atoi vs strtol, and the comparator bug in qsort.
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

// Correct parse: reports whether the whole string was a valid in-range integer.
static int parse_long(const char *s, long *out) {
    char *end; errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s) return -1;                       // no digits at all
    if (*end != '\0') return -2;                   // trailing garbage
    if (errno == ERANGE) return -3;                // overflow/underflow: v is LONG_MAX/LONG_MIN
    *out = v; return 0;
}

static int cmp_bad(const void *a, const void *b)  { return *(const int *)a - *(const int *)b; }          // overflows for large values
static int cmp_good(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return (x > y) - (x < y); }

int main(void) {
    const char *inputs[] = {"42", "  -17", "12abc", "abc", "", "99999999999999999999", "0x1f"};
    printf("atoi(\"abc\") = %d, atoi(\"12abc\") = %d  (atoi cannot report errors)\n", atoi("abc"), atoi("12abc"));
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
        long v = 0; int rc = parse_long(inputs[i], &v);
        printf("parse_long(\"%s\") -> %s", inputs[i], rc == 0 ? "ok" : rc == -1 ? "no digits" : rc == -2 ? "trailing garbage" : "out of range");
        if (rc == 0) printf(" %ld", v);
        printf("\n");
    }
    printf("strtol(\"0x1f\", base 16) = %ld, base 0 (auto-detect) = %ld\n", strtol("0x1f", NULL, 16), strtol("0x1f", NULL, 0));

    int v[] = {INT_MAX, -5, 3, INT_MIN, 0, 100};
    int n = sizeof v / sizeof *v, w[6]; for (int i = 0; i < n; i++) w[i] = v[i];
    qsort(v, n, sizeof *v, cmp_bad);  qsort(w, n, sizeof *w, cmp_good);
    printf("qsort with 'a - b' comparator: "); for (int i = 0; i < n; i++) printf("%d ", v[i]);
    printf("\nqsort with (a>b)-(a<b):        "); for (int i = 0; i < n; i++) printf("%d ", w[i]);
    printf("\n");
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra numbers.c -o numbers && ./numbers
atoi("abc") = 0, atoi("12abc") = 12  (atoi cannot report errors)
parse_long("42") -> ok 42
parse_long("  -17") -> ok -17
parse_long("12abc") -> trailing garbage
parse_long("abc") -> no digits
parse_long("") -> no digits
parse_long("99999999999999999999") -> out of range
parse_long("0x1f") -> trailing garbage
strtol("0x1f", base 16) = 31, base 0 (auto-detect) = 31
qsort with 'a - b' comparator: 2147483647 -2147483648 -5 0 3 100 
qsort with (a>b)-(a<b):        -2147483648 -5 0 3 100 2147483647 
```

Observations:

- `atoi("abc")` and `atoi("12abc")` give 0 and 12 with no way to detect the problem.
- `strtol` with **base 0** recognises `0x` (hex) and leading `0` (octal); with base 10 the `0x1f` string stops at the `x`. Be careful when parsing user input that might start with a zero: with base 0 `"010"` means 8.
- The **`qsort` comparator `a - b`** is a classic bug: for `INT_MAX` and `INT_MIN` the subtraction overflows (undefined behavior), so the first output is not sorted. Return `(a > b) - (a < b)` instead. Note too that `qsort` is not guaranteed stable, and `bsearch` requires an array sorted with the *same* comparator.

## Strings and memory functions

```c
// The string-function traps: strncpy, snprintf's return value, strtok, memcpy vs memmove, and isalpha(char).
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char dst[8]; memset(dst, 'X', sizeof dst);
    strncpy(dst, "overlong text", sizeof dst);                       // copies 8 bytes, writes NO terminator
    printf("strncpy into char[8]: terminated within the buffer? %s\n", memchr(dst, '\0', sizeof dst) ? "yes" : "NO");

    char small[8];
    int would_need = snprintf(small, sizeof small, "%s-%d", "overlong", 12345);
    printf("snprintf into char[8]: stored \"%s\", returned %d (the length it WOULD have needed; truncated: %s)\n", small, would_need, would_need >= (int)sizeof small ? "yes" : "no");

    char line[] = "a,b,,c";
    printf("strtok on \"a,b,,c\": ");
    for (char *t = strtok(line, ","); t; t = strtok(NULL, ",")) printf("[%s]", t);
    printf("   <- the empty field vanished and the original string was modified\n");
    char copy[] = "a,b,,c";
    printf("strsep on the same input: ");
    char *cursor = copy, *tok;
    while ((tok = strsep(&cursor, ",")) != NULL) printf("[%s]", tok);
    printf("   <- strsep (BSD/glibc) keeps empty fields\n");

    char buf[] = "0123456789";
    memmove(buf + 2, buf, 6);                                       // overlapping regions: memmove is defined, memcpy is not
    printf("memmove(buf+2, buf, 6) on \"0123456789\": %s\n", buf);

    signed char ch = (signed char)0xE9;                             // 'e acute' in Latin-1: negative as a signed char
    printf("isalpha((unsigned char)0xE9) in the C locale: %d   (passing a negative char instead is undefined behavior)\n", isalpha((unsigned char)ch) != 0);
    printf("strlen is O(n): strlen(\"\") = %zu, strlen(\"hello\") = %zu; sizeof(\"hello\") = %zu (includes the terminator)\n", strlen(""), strlen("hello"), sizeof("hello"));
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -Wno-stringop-truncation -Wno-format-truncation strings.c -o strings && ./strings
strncpy into char[8]: terminated within the buffer? NO
snprintf into char[8]: stored "overlon", returned 14 (the length it WOULD have needed; truncated: yes)
strtok on "a,b,,c": [a][b][c]   <- the empty field vanished and the original string was modified
strsep on the same input: [a][b][][c]   <- strsep (BSD/glibc) keeps empty fields
memmove(buf+2, buf, 6) on "0123456789": 0101234589
isalpha((unsigned char)0xE9) in the C locale: 0   (passing a negative char instead is undefined behavior)
strlen is O(n): strlen("") = 0, strlen("hello") = 5; sizeof("hello") = 6 (includes the terminator)
```

Lessons from each line:

- **`strncpy` is not a safe `strcpy`.** It pads with zeros if the source is shorter than the count, but if the source is as long or longer it copies exactly *n* bytes and writes **no terminator**; the result is an unterminated string. Use `snprintf(dst, sizeof dst, "%s", src)` or `memccpy`/`strlcpy` where available (`strlcpy` is in glibc 2.38 and later and in BSD libcs), and always terminate explicitly.
- **`snprintf` returns the length it *would* have written**, not the length it stored. Compare the return value to the buffer size to detect truncation. Never use the return value as the number of bytes written without that check. It never writes past the buffer and always terminates (for size > 0).
- **`strtok` keeps hidden state** (not thread-safe, not reentrant), modifies its input and collapses runs of delimiters, so empty fields disappear. Use `strtok_r` in threaded code, or `strsep` (BSD and glibc, not ISO C) when empty fields matter, or write the few lines of `strchr`-based splitting yourself.
- **`memcpy` requires non-overlapping regions;** `memmove` handles overlap. Using `memcpy` on overlapping buffers is undefined behavior that often works until an optimizer or a different libc breaks it.
- **`<ctype.h>` functions take an `int` that must be representable as `unsigned char` or equal to `EOF`.** Passing a plain `char` holding a negative value (any byte above 127 when `char` is signed) is undefined behavior. Cast: `isalpha((unsigned char)c)`. They also depend on the current locale.
- `strlen` walks the string: calling it in a loop condition (`for (i = 0; i < strlen(s); i++)`) is quadratic. `sizeof("hello")` is 6 because it counts the terminator, but `sizeof` of a pointer parameter is the pointer's size.
- Also: `strcpy`, `strcat` and `sprintf` and `gets` (removed in C11) have no length checks; compare strings with `strcmp` and test the result against 0, not against 1 or -1; `strerror` returns a shared buffer on some libcs, `strerror_r` has two incompatible versions (GNU and POSIX), so check which one you get.

## Time, time zones and locale

```c
// Time and locale: struct tm, timegm vs mktime, strftime, and how setlocale changes number parsing.
#define _GNU_SOURCE
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    time_t t = 1700000000;                                          // a fixed instant: 2023-11-14 22:13:20 UTC
    char buf[64];
    struct tm utc; gmtime_r(&t, &utc);                              // thread-safe variant; gmtime() returns a shared static buffer
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &utc);
    printf("time_t %ld = %s\n", (long)t, buf);

    setenv("TZ", "Asia/Kolkata", 1); tzset();
    struct tm local; localtime_r(&t, &local);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %Z (%z)", &local);
    printf("same instant in Asia/Kolkata: %s\n", buf);
    printf("timegm(&utc) == t: %s;   mktime(&utc) == t: %s  (mktime interprets the fields as LOCAL time)\n", timegm(&utc) == t ? "yes" : "no", mktime(&utc) == t ? "yes" : "no");

    struct tm feb = {0}; feb.tm_year = 2024 - 1900; feb.tm_mon = 1; feb.tm_mday = 30; feb.tm_hour = 12;       // 30 February
    mktime(&feb); strftime(buf, sizeof buf, "%Y-%m-%d", &feb);
    printf("mktime normalizes 2024-02-30 to %s\n", buf);

    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); clock_gettime(CLOCK_MONOTONIC, &b);
    printf("CLOCK_MONOTONIC never goes backwards (diff >= 0: %s); CLOCK_REALTIME can jump when the clock is set\n", (b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec) >= 0 ? "yes" : "no");

    printf("strtod(\"3.14\") in the C locale: %g\n", strtod("3.14", NULL));
    if (setlocale(LC_NUMERIC, "de_DE.UTF-8")) printf("after setlocale(de_DE): strtod(\"3.14\") = %g   (the decimal separator is now a comma)\n", strtod("3.14", NULL));
    else printf("de_DE.UTF-8 locale is not installed here; with it, strtod(\"3.14\") would stop at the '.' and return 3\n");
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra timeloc.c -o timeloc && ./timeloc
time_t 1700000000 = 2023-11-14T22:13:20Z
same instant in Asia/Kolkata: 2023-11-15 03:43:20 IST (+0530)
timegm(&utc) == t: yes;   mktime(&utc) == t: no  (mktime interprets the fields as LOCAL time)
mktime normalizes 2024-02-30 to 2024-03-01
CLOCK_MONOTONIC never goes backwards (diff >= 0: yes); CLOCK_REALTIME can jump when the clock is set
strtod("3.14") in the C locale: 3.14
de_DE.UTF-8 locale is not installed here; with it, strtod("3.14") would stop at the '.' and return 3
```

- `time_t` is seconds since 1970-01-01 UTC (on Linux a 64-bit value, so the year-2038 problem applies only to 32-bit `time_t` systems). The same instant prints differently in different zones: the **zone is a property of the formatting, not of the instant**. Store and transmit instants as UTC timestamps; convert for display.
- `gmtime`/`localtime` return a pointer to a shared static `struct tm`: two calls overwrite each other and threads race. Use `gmtime_r` and `localtime_r`.
- **`mktime` treats the `struct tm` as local time**; `timegm` (a common extension, not in ISO C) treats it as UTC. Mixing them is a frequent off-by-timezone bug. `mktime` also **normalizes** out-of-range fields (30 February becomes 1 March), which is useful for date arithmetic but means it never reports an invalid date.
- Measure durations with `CLOCK_MONOTONIC` (`clock_gettime`), which never jumps; the wall clock (`CLOCK_REALTIME`) can be stepped by NTP or an administrator. `clock()` measures CPU time, not elapsed time. `time()` has one-second resolution.
- **Locale.** Programs start in the `"C"` locale, but `setlocale(LC_ALL, "")` adopts the user's environment. Then `printf("%f")` and `strtod` use the locale's decimal separator, so a program that works in `en_US` can fail to parse `3.14` in a German locale (the example above prints what happens when that locale is installed; here it is not). Parse and write machine-readable numbers with `LC_NUMERIC` set to `"C"` (or use `uselocale` per thread), and keep locale-sensitive code (`toupper`, `strcoll`, `strftime` names) separate from protocol code.

## errno, printf and line input

```c
// errno rules, printf return/format traps and getline.
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *f = fopen("/nonexistent/file", "r");
    int saved = errno;                                              // copy at once: any later library call may overwrite errno
    printf("fopen failed: %s (errno %d); errno is only meaningful right after a call that reported failure\n", strerror(saved), saved);
    (void)f;
    errno = ENOENT; strtol("1", NULL, 10);                          // a successful call
    printf("a successful call does not clear errno: still %d (%s). Set errno = 0 before calls whose only error signal is errno\n", errno, strerror(errno));

    int n = printf("%s %d\n", "printf returns the number of characters written:", 42);
    printf("(that was %d characters)\n", n);
    printf("sizeof(long)=%zu sizeof(size_t)=%zu: use %%zu for size_t, %%ld for long, PRId64 for int64_t\n", sizeof(long), sizeof(size_t));

    FILE *tmp = tmpfile();
    fputs("first line\nsecond, longer line than the first\n\nlast", tmp); rewind(tmp);
    char *line = NULL; size_t cap = 0; ssize_t len; int i = 0;
    while ((len = getline(&line, &cap, tmp)) != -1) printf("getline #%d: %zd bytes: \"%.*s\"\n", ++i, len, (int)(len && line[len - 1] == '\n' ? len - 1 : len), line);
    free(line); fclose(tmp);

    volatile size_t huge = (size_t)-1; errno = 0; char *big = malloc(huge);
    printf("malloc(SIZE_MAX) -> %s, errno %d (%s)\n", big ? "non-NULL" : "NULL", errno, strerror(errno));
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra errno_io.c -o errno_io && ./errno_io
fopen failed: No such file or directory (errno 2); errno is only meaningful right after a call that reported failure
a successful call does not clear errno: still 2 (No such file or directory). Set errno = 0 before calls whose only error signal is errno
printf returns the number of characters written: 42
(that was 52 characters)
sizeof(long)=8 sizeof(size_t)=8: use %zu for size_t, %ld for long, PRId64 for int64_t
getline #1: 11 bytes: "first line"
getline #2: 35 bytes: "second, longer line than the first"
getline #3: 1 bytes: ""
getline #4: 4 bytes: "last"
malloc(SIZE_MAX) -> NULL, errno 12 (Cannot allocate memory)
```

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
