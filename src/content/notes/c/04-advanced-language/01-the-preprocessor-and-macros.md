---
title: The Preprocessor and Macros
track: c
module: Advanced Language
moduleOrder: 4
order: 1
level: intermediate
summary: What the C preprocessor does and doesn't understand, the classic macro bugs with measurements, safe macro idioms (do-while(0), statement expressions, _Generic), X-macros, conditional compilation, and when to prefer inline functions and enums.
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.10 Preprocessing directives; §6.10.3 Macro replacement; §6.10.8 Predefined macro names"
  - label: "GNU CPP manual (The C Preprocessor)"
    url: https://gcc.gnu.org/onlinedocs/cpp/
  - label: "SEI CERT C — PRE00-C to PRE32-C (preprocessor rules)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+01.+Preprocessor+%28PRE%29
  - label: "Linux kernel coding style — Chapter 12 Macros, Enums and RTL; include/linux/minmax.h"
    url: https://docs.kernel.org/process/coding-style.html#macros-enums-and-rtl
  - label: "GCC manual — Statement Exprs, Typeof, Variadic Macros, Predefined Macros"
    url: https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
---

The preprocessor is a separate text-processing pass that runs *before* the compiler (see the pipeline note). It knows tokens, not C: no types, no scopes, no expressions. That makes it powerful for conditional compilation, code generation and compile-time constants, and treacherous for anything that looks like a function.

## Directives

| Directive | Purpose |
|---|---|
| `#include <f.h>` / `"f.h"` | paste a file (angle brackets: system paths; quotes: current dir first) |
| `#define NAME value` / `#define F(x) ...` / `#undef` | object-like and function-like macros |
| `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `defined(X)` | conditional compilation; `#if` evaluates integer constant expressions (undefined names are 0) |
| `#error "msg"`, `#warning` | diagnostics |
| `#pragma ...` | implementation-specific (`#pragma once`, `#pragma GCC diagnostic`, `#pragma omp`) |
| `#line`, `#embed` (C23) | line control; include binary file contents as an initializer list |
| `__has_include(<h>)`, `__has_attribute`, `__has_c_attribute` | feature detection |

Predefined: `__FILE__`, `__LINE__`, `__func__` (a function-local identifier, not a macro), `__DATE__`, `__TIME__` (break reproducible builds; set `-Wdate-time`), `__STDC_VERSION__` (`201112L` C11, `201710L` C17, `202311L` C23; GCC < 15 reports `202000L` for `-std=c23`), `__GNUC__`, `__linux__`, `__x86_64__`, `__BYTE_ORDER__`, `__COUNTER__`.

## Macro expansion rules (and why bugs happen)

1. Arguments are substituted **textually**, after being fully macro-expanded (unless used with `#` or `##`).
2. The result is rescanned, but a macro isn't re-expanded inside its own expansion.
3. No evaluation happens: `SQUARE(i++)` pastes `i++` wherever `x` appears.

A set of the classic traps (`pitfalls.c`):

```c
#include <stdio.h>

#define SQUARE_BAD(x)   x * x
#define SQUARE_OK(x)    ((x) * (x))
#define MAX_BAD(a, b)   ((a) > (b) ? (a) : (b))               /* parenthesized, but evaluates arguments twice */
#define MAX_SAFE(a, b)  ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })   /* GNU statement expression */
#define SWAP_BAD(a, b)  int t = a; a = b; b = t;
#define SWAP_OK(T, a, b) do { T t_ = (a); (a) = (b); (b) = t_; } while (0)

static int calls;
static int next(void) { return ++calls; }

int main(void) {
    printf("SQUARE_BAD(1+2) = %d (expected 9)\n", SQUARE_BAD(1 + 2));      /* expands to 1 + 2 * 1 + 2 */
    printf("SQUARE_OK(1+2)  = %d\n", SQUARE_OK(1 + 2));

    int i = 3, j = 5;
    int m = MAX_BAD(i++, j++);                                              /* the larger argument is incremented twice */
    printf("MAX_BAD(i++, j++) = %d, i=%d, j=%d (j incremented twice)\n", m, i, j);

    calls = 0;
    int r = MAX_BAD(next(), 0);
    printf("MAX_BAD(next(), 0) = %d, next() was called %d times\n", r, calls);
    calls = 0;
    r = MAX_SAFE(next(), 0);
    printf("MAX_SAFE(next(), 0) = %d, next() was called %d time\n", r, calls);

    int a = 1, b = 2;
    SWAP_OK(int, a, b);
    printf("after SWAP_OK a=%d b=%d\n", a, b);

    if (a > b)
        SWAP_OK(int, a, b);       /* safe in an if/else because of do { } while (0); a naive two-statement macro would break the else */
    else
        puts("else branch reached correctly");
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra pitfalls.c -o pitfalls && ./pitfalls
SQUARE_BAD(1+2) = 5 (expected 9)
SQUARE_OK(1+2)  = 9
MAX_BAD(i++, j++) = 6, i=4, j=7 (j incremented twice)
MAX_BAD(next(), 0) = 2, next() was called 2 times
MAX_SAFE(next(), 0) = 1, next() was called 1 time
after SWAP_OK a=2 b=1
```

| Bug | Cause | Fix |
|---|---|---|
| `SQUARE_BAD(1+2)` = **5**, not 9 | expands to `1 + 2 * 1 + 2` | parenthesize every parameter and the whole expansion |
| `MAX_BAD(i++, j++)` increments `j` twice | argument pasted twice, evaluated twice | use an inline function, or a GNU statement expression with `__typeof__` temporaries (`MAX_SAFE`) |
| `next()` called twice | same | same |
| `if (c) SWAP_BAD(a,b); else ...` fails to compile / misbehaves | multi-statement macro breaks the `if/else` | wrap in **`do { ... } while (0)`**, which acts as exactly one statement requiring a semicolon |
| Name collision: temp `t` shadows the caller's `t` | macro-local variable names aren't hygienic | use unlikely names (`t_`, `_a`), or `__COUNTER__`/`__LINE__` pasting |

Look at the real preprocessor output for the first bug:

```text
$ printf '#define SQUARE_BAD(x) x * x\nint v = SQUARE_BAD(1 + 2);\n' | gcc -E -P - | tail -1
int v = 1 + 2 * 1 + 2;
```

`gcc -E` (and `-save-temps`) is the best macro debugger: always look at what the compiler actually sees.

## Preferred alternatives

| Instead of | Use | Why |
|---|---|---|
| `#define N 100` | `enum { N = 100 };` or `static const int N = 100;` (C23: `constexpr int N = 100;`) | typed/scoped, visible in debuggers, no pasting surprises (note: C `const int` isn't an integer *constant expression*; use `enum`/`constexpr` for array sizes and `case` labels) |
| function-like macros | `static inline` functions | type checking, single evaluation, debuggable |
| `#define MAX(a,b)` for any type | `_Generic` dispatch or type-specific inline functions | |
| lists of related names | X-macros (below) or `enum` | single source of truth |
| `#ifdef` forests | one platform-abstraction header, small `#if` islands | readability, testability |

Legitimate macro jobs: conditional compilation, stringification/token pasting, `__FILE__/__LINE__` capture, generic code (when `_Generic` isn't enough), compile-time assertions, defining syntax-like constructs (iteration over intrusive lists: `list_for_each_entry`), and wrappers that must expand in the caller (e.g. logging that captures the call site).

## Stringification, pasting and variadic macros

- `#x` turns the macro argument into a string literal (`#name` → `"RED"`); `a ## b` pastes tokens into one (`COLOR_##name` → `COLOR_RED`). For these operators the argument is *not* macro-expanded first; use a two-level macro (`#define STR(x) XSTR(x)` / `#define XSTR(x) #x`) to stringify a macro's value.
- Variadic macros (`...`, `__VA_ARGS__`): `#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)` fails when no extra arguments are given; GNU's `, ##__VA_ARGS__` swallows the comma, C23 standardizes `__VA_OPT__(, )`.

## X-macros: one list, many expansions

Define a list once as a macro taking another macro `X`, then expand it differently each time. It keeps enums, name tables and handlers in sync (adding an entry is a one-line change):

```c
#include <stdio.h>
#include <string.h>

/* X-macro: define the list ONCE, generate enum, names and handlers from it. */
#define COLORS(X) \
    X(RED,   0xFF0000) \
    X(GREEN, 0x00FF00) \
    X(BLUE,  0x0000FF)

enum color {
#define X(name, rgb) COLOR_##name,
    COLORS(X)
#undef X
    COLOR_COUNT
};

static const char *color_name[] = {
#define X(name, rgb) [COLOR_##name] = #name,       /* # stringifies, ## pastes tokens */
    COLORS(X)
#undef X
};

static const unsigned color_rgb[] = {
#define X(name, rgb) [COLOR_##name] = rgb,
    COLORS(X)
#undef X
};

#define LOG(level, fmt, ...) fprintf(stderr, "[%s] %s:%d %s(): " fmt "\n", level, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define TYPE_NAME(x) _Generic((x), int: "int", long: "long", double: "double", char *: "char*", const char *: "const char*", default: "other")

#define STATIC_ASSERT_SIZE(T, n) _Static_assert(sizeof(T) == (n), #T " must be " #n " bytes")
STATIC_ASSERT_SIZE(int, 4);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

int main(void) {
    for (int c = 0; c < COLOR_COUNT; c++) printf("%d %-5s #%06X\n", c, color_name[c], color_rgb[c]);
    printf("ARRAY_LEN(color_name) = %zu\n", ARRAY_LEN(color_name));
    LOG("INFO", "plain message");
    LOG("WARN", "value=%d name=%s", 42, color_name[COLOR_BLUE]);
    printf("%s %s %s %s\n", TYPE_NAME(1), TYPE_NAME(1L), TYPE_NAME(2.5), TYPE_NAME("str"));
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra xmacro.c -o xmacro && ./xmacro 2>&1
[INFO] xmacro.c:41 main(): plain message
[WARN] xmacro.c:42 main(): value=42 name=BLUE
0 RED   #FF0000
1 GREEN #00FF00
2 BLUE  #0000FF
ARRAY_LEN(color_name) = 3
int long double char*
```

This program also shows:

- **`LOG`** capturing `__FILE__`, `__LINE__`, `__func__` at the call site (`[WARN] xmacro.c:42 main(): ...`), with the optional variadic tail `##__VA_ARGS__`.
- **`_Generic`** (C11): selection by the *type* of an expression at compile time; basis for type-generic macros like `<tgmath.h>`. `TYPE_NAME(1)` → `int`, `2.5` → `double`; note that a string literal selects `char*` after array-to-pointer decay.
- **`_Static_assert`** with a stringified message: compile-time checks of assumptions, in a macro.
- **`ARRAY_LEN(a)`**: `sizeof(a)/sizeof((a)[0])` is only valid for real arrays, not pointers (silently wrong for decayed parameters; the kernel's version adds a `__must_be_array` check, and GCC's `-Wsizeof-pointer-div` warns).

## Conditional compilation done well

```c
#include <stdio.h>
#if defined(__has_include)
#  if __has_include(<threads.h>)
#    include <threads.h>
#    define HAVE_THREADS_H 1
#  endif
#endif
#ifndef HAVE_THREADS_H
#  define HAVE_THREADS_H 0
#endif

#if defined(__linux__)
#  define OS "linux"
#elif defined(__APPLE__)
#  define OS "macos"
#else
#  define OS "other"
#endif

#if __STDC_VERSION__ >= 202000L        /* GCC < 15 reports 202000L for -std=c2x/c23 */
#  define STD "C23"
#elif __STDC_VERSION__ >= 201710L
#  define STD "C17"
#elif __STDC_VERSION__ >= 201112L
#  define STD "C11"
#else
#  define STD "older"
#endif

#ifndef BUILD_ID
#  define BUILD_ID "dev"
#endif

int main(void) {
    printf("os=%s std=%s have_threads_h=%d build=%s gcc=%d.%d\n", OS, STD, HAVE_THREADS_H, BUILD_ID, __GNUC__, __GNUC_MINOR__);
    printf("compiled %s %s from %s line %d\n", __DATE__, "", __FILE__, __LINE__);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -DBUILD_ID='"ci-1234"' cond.c -o cond && ./cond | head -1; gcc -std=c23 cond.c -o cond23 && ./cond23 | head -1
os=linux std=C11 have_threads_h=1 build=ci-1234 gcc=14.2
os=linux std=C23 have_threads_h=1 build=dev gcc=14.2
```

- Feature detection (`__has_include`, `__has_attribute`, `__STDC_VERSION__`) beats compiler-name sniffing; use `configure`/CMake feature checks for library features.
- `-DNAME=value` defines macros from the command line (versioning, feature toggles); quote string values for the shell (`-DBUILD_ID='"ci-1234"'`).
- Keep `#if` blocks small and symmetric; each configuration must be compiled in CI, otherwise the unused branches rot.
- **Include guards**: `#ifndef FOO_H / #define FOO_H / ... / #endif` (portable) or `#pragma once` (universal in practice). Name guards uniquely (path-based) to avoid collisions.
- **Include hygiene**: headers should be self-contained (include what they need), never define variables, and avoid including other headers unnecessarily (use forward declarations of structs). Include-what-you-use tools help; `-H` prints the include tree.

## Hygiene rules for macros

1. Prefer inline functions and enums; use macros only when necessary.
2. Parenthesize parameters and the whole expansion; avoid evaluating arguments more than once or document it (`MAX`-like macros should say so; use `__typeof__` temporaries in GNU C).
3. Multi-statement macros: `do { ... } while (0)`.
4. Don't hide control flow (`return`, `goto`, `break`) or declare caller-visible variables in macros unless the name signals it (`FOREACH_...`, `TRY`).
5. Uppercase names for macros (conventionally), unique prefixes in libraries (`LIB_FOO`).
6. `#undef` helper macros (`X`) after use; don't leak macros from headers (`min`/`max`/`bool`/`true` collisions are famous, e.g. Windows headers).
7. Don't rely on `__DATE__`/`__TIME__` in reproducible builds; use `SOURCE_DATE_EPOCH` and explicit version strings.
8. Debug with `gcc -E -P`, `-dM -E` (dump all macros: `echo | gcc -dM -E - | grep __VERSION__`), and `-fdirectives-only`.

## Key takeaways

- The preprocessor substitutes tokens before compilation; it has no types, no evaluation, no scope.
- Parenthesize everything, never pass side effects to function-like macros, wrap statement macros in `do { } while (0)`, and use `gcc -E` to see the expansion.
- Prefer `enum`/`constexpr`, `static inline`, and `_Generic` to macros; use macros for conditional compilation, X-macro tables, `#`/`##` and call-site capture (`__FILE__`, `__LINE__`).
- Detect features, not compilers; compile every configuration in CI.
