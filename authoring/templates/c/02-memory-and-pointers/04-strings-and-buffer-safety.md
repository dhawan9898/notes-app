---
title: Strings and Buffer Safety
track: c
module: Memory and Pointers
moduleOrder: 2
order: 4
level: intermediate
summary: How C strings really work, why the classic library functions are dangerous, safe replacements (snprintf, strtol, strlcpy, length-tracking builders), format-string vulnerabilities and UTF-8 pitfalls, with tested code.
prerequisites:
  - "Arrays, Strings, Command-Line Arguments and Files"
sources:
  - label: ISO/IEC 9899:2018 (C17) §7.24 String handling; §7.21.6 Formatted output; §6.4.5 String literals
  - label: "SEI CERT C - STR31-C, STR32-C, STR38-C, FIO30-C (exclude user input from format strings), ARR38-C"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+07.+Characters+and+Strings+%28STR%29
  - label: man 3 string, man 3 snprintf, man 3 strtol, man 3 strlcpy (glibc 2.38+), man 3 explicit_bzero
    url: https://man7.org/linux/man-pages/man3/strlcpy.3.html
  - label: Miller & de Raadt — strlcpy and strlcat - consistent, safe, string copy and concatenation (USENIX 1999)
  - label: The Unicode Standard; RFC 3629 — UTF-8, a transformation format of ISO 10646
    url: https://www.rfc-editor.org/rfc/rfc3629
---

C has no string type. A "string" is a convention: a sequence of `char` ended by a **NUL byte** (`'\0'`). Every string function walks memory until it finds that byte, and every buffer overflow involving text comes from the gap between "how long the string is" and "how big the buffer is".

## Representation

```c
char a[] = "hello";          /* array of 6 chars (including NUL), writable copy */
const char *p = "hello";     /* pointer to a string literal in read-only memory */
```

- A **string literal** has static storage and type `char[N]` (but is not `const` in the type system, a historical wart). **Modifying a literal is undefined behavior** (typically `SIGSEGV`); declare pointers to literals `const char *` (`-Wwrite-strings`).
- `sizeof` gives the array size (6) or pointer size (8); `strlen` counts characters before the NUL (5) in O(n). `sizeof(arr)` is wrong for a parameter (decayed pointer).
- Literals that look identical may or may not share storage (`"a" == "a"` is unspecified); compare with `strcmp`, not `==`.
- Embedded NULs make a "string" end early; binary data is not a string: track lengths and use `memcpy`/`memcmp`.
- Strings are `char`; for text bytes with values ≥ 128 cast to `unsigned char` before `isalpha`, `toupper`, etc. (`isalpha(c)` with negative `c` is UB).

Real behavior of the common functions (`basics.c`):

{{include c/strings/basics.c}}

{{run c/strings :: gcc -std=gnu11 -w basics.c -o basics && ./basics}}

(Compiling that file with `-Wall -Wextra` prints `-Wstringop-truncation` and `-Wformat-truncation` warnings for the deliberately unsafe lines: those warnings are valuable, so keep them on.)

## The dangerous functions

| Function | Problem | Safer choice |
|---|---|---|
| `gets` | cannot limit input; **removed in C11** | `fgets(buf, sizeof buf, stdin)`, `getline` (POSIX, allocates) |
| `strcpy`, `strcat`, `sprintf`, `vsprintf` | no bounds | `snprintf`, `strlcpy/strlcat` (glibc ≥ 2.38, BSD), length-checked `memcpy` |
| `strncpy` | **does not terminate** if the source is as long as the limit, and pads with NULs up to `n` (slow); designed for fixed-width fields, not "safe strcpy" | `snprintf(dst, n, "%s", src)` or `strlcpy` |
| `strncat` | the limit is the *remaining space for new chars*, not the buffer size; easy to misuse | track lengths yourself |
| `scanf("%s")` | unbounded; `%s` without width | `scanf("%15s")` with width = size−1, or `fgets` + `strtol` |
| `strtok` | modifies the input, hidden global state (not thread-safe) | `strtok_r`, `strsep`, or manual parsing |
| `atoi`, `atol`, `atof` | no error reporting (`atoi("abc") == 0`) | `strtol`, `strtoul`, `strtod` with error checks |
| `printf(user_string)` | format-string vulnerability | `printf("%s", user_string)` |
| `memcpy` with overlap | UB | `memmove` |
| `strlen` on untrusted unterminated data | reads past the buffer | `strnlen(s, max)` or `memchr` |
| `tmpnam`, `mktemp`, `getwd` | races/unbounded | `mkstemp`, `getcwd` |

### Reading the output above

- `strncpy(buf, "0123456789", 8)` filled all 8 bytes and left **no NUL**: any later `printf("%s", buf)` reads past the buffer.
- `snprintf` always terminates (when size > 0) and **returns the length it would have written** (12), not the number actually written: compare the return value with the buffer size to detect truncation (`need >= (int)sizeof buf`). It returns negative on encoding error.
- `strtol` is parsed with its three signals: nothing parsed (`end == input`), `errno == ERANGE`, and trailing characters (`*end != '\0'`). Always set `errno = 0` first.
- `memmove(s + 2, s, 4)` handled overlapping regions ("ababcd"); `memcpy` here would be UB.
- UTF-8: "naïve ✓" is **10 bytes, 7 code points**. `strlen` counts bytes. Don't index into UTF-8 strings by character, don't truncate in the middle of a multibyte sequence, and note that "characters" shown to users are *grapheme clusters* (even code points aren't enough). Use a library (ICU, utf8proc, libunistring) for normalization, case-folding and width.

## Designing safe string handling

### 1. Always carry the length

NUL-termination makes length an O(n) operation and makes embedded data impossible. Robust C APIs pass `(ptr, len)` pairs or use a small string struct. A length-tracking, auto-growing builder (`safestr.c`) that cannot overflow and is always terminated for convenience:

{{include c/strings/safestr.c}}

{{run c/strings :: gcc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined safestr.c -o safestr && ./safestr}}

Techniques used:

- Capacity grows geometrically (amortized O(1) appends).
- The overflow guard `n > SIZE_MAX - s->len - 1` precedes `len + n + 1` (arithmetic overflow in size computations leads to heap overflows).
- `vsnprintf(NULL, 0, ...)` measures first, then a second `vsnprintf` writes exactly: the standard two-pass approach (`va_copy` is required because `va_list` can only be consumed once).
- `__attribute__((format(printf, 2, 3)))` makes GCC check the format string and arguments at every call site (`-Wformat`).
- ASan/UBSan clean.

Mature alternatives: `asprintf`/`vasprintf` (GNU/BSD: allocate the result), `getline`, `open_memstream` (stdio into a growing buffer), SDS (Redis), GLib `GString`, `bstrlib`, C++ `std::string_view`/Rust `&str` as models.

### 2. Make copy truncation explicit

```c
size_t n = strlcpy(dst, src, sizeof dst);       /* returns strlen(src): n >= sizeof dst means truncated */
if (n >= sizeof dst) return -ENAMETOOLONG;
```

`strlcpy`/`strlcat` entered glibc in 2.38 (2023); before that use `snprintf` or a tiny wrapper. They *report* truncation instead of hiding it; ignoring that report silently turns an overflow into a logic bug (a truncated path or SQL fragment can itself be a vulnerability). Decide: reject, or truncate deliberately.

### 3. Validate, don't sanitize at the last minute

Check lengths and character sets at the boundary where untrusted input enters; use allow-lists; reject embedded NULs where unexpected; canonicalize paths before checking (`realpath`), and avoid time-of-check/time-of-use races.

### 4. Numbers from strings

Use `strtol`/`strtoul`/`strtoll`/`strtod`, check all three failure signals, and range-check the result for the target type (`long` → `int`). Avoid `atoi`, `sscanf("%d")` without checking its return value (overflow in `sscanf` is UB).

### 5. Secrets

`memset` before `free` may be removed by dead-store elimination: use `explicit_bzero` (glibc/BSD), `memset_explicit` (C23), or `memset_s`. Don't leave passwords in `argv` (visible in `ps`) or environment.

## Format-string vulnerabilities

If attacker-controlled text reaches the *format* argument of `printf`, they can read the stack (`%x`, `%p`), crash the process (`%s` on a garbage pointer), or even **write memory** (`%n` stores the number of characters printed through a pointer argument). The fix is trivial (`printf("%s", s)`), which makes it embarrassing. GCC warns with `-Wformat-security` (part of `-Wformat=2`):

{{include c/strings/fmt.c}}

{{run c/strings :: gcc -Wall -Wformat=2 -c fmt.c -o /dev/null 2>&1 | grep -E 'warning' | cut -c1-120; gcc -O0 -Wno-format-security -o fmt_bad fmt.c && ./fmt_bad 'AAAA %p %p %p %p %p %p'}}

The `%p` directives printed values taken from the stack and registers (stack pointers, a code address revealing the ASLR base, library addresses): each leak helps an attacker defeat ASLR. `_FORTIFY_SOURCE=2` blocks `%n` in writable format strings. Treat **every** variadic format function (`printf`, `fprintf`, `snprintf`, `syslog`, `err`, your own logging macros) the same way, and mark your wrappers with `__attribute__((format(printf, fmtindex, firstarg)))`.

## Common patterns done right

```c
/* Read a line of any length, strip newline */
char *line = NULL; size_t cap = 0;
ssize_t len = getline(&line, &cap, stdin);
if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
free(line);

/* Split "key=value" without modifying the input */
const char *eq = strchr(s, '=');
if (eq) { size_t klen = (size_t)(eq - s); printf("%.*s -> %s\n", (int)klen, s, eq + 1); }   /* %.*s: precision from an argument */

/* Case-insensitive compare, locale-independent */
strcasecmp(a, b);          /* POSIX <strings.h> */

/* Join with a delimiter without a manual pointer dance: use a string builder */

/* Sorting an array of strings */
static int cmp(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
qsort(names, count, sizeof *names, cmp);
```

## Wide characters, encodings and locale

- `char` is a byte; `wchar_t` is 32-bit on Linux (UTF-32), 16-bit on Windows: avoid it. C11 `char16_t`/`char32_t` (`<uchar.h>`); C23 `char8_t`, `u8` literals.
- Treat text as UTF-8 internally and at boundaries; `setlocale(LC_ALL, "")` changes `isalpha`, `toupper`, `strcoll`, `printf` decimal point (a `,` in some locales can break number parsing/writing): in libraries, use the "C" locale or locale-independent functions (`strtod_l`, `uselocale`).
- File names are byte strings (not necessarily valid UTF-8) on Unix.

## Key takeaways

- A C string is bytes ending at the first NUL; every operation is only as safe as your knowledge of the buffer size.
- Avoid `gets`, `strcpy`, `strcat`, `sprintf`, `strncpy` (as a safe copy), `atoi`, `strtok`; use `snprintf` (check the return value), `strlcpy`, `strtol` (check `errno`/`end`), `getline`, and length-tracking builders.
- Guard size arithmetic against overflow; use `memmove` for overlap; compare with `strcmp`.
- Never pass user data as a format string; annotate wrappers with the `format` attribute and compile with `-Wformat=2`.
- Bytes ≠ characters ≠ graphemes; handle UTF-8 deliberately; wipe secrets with `explicit_bzero`.
