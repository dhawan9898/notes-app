---
title: The Preprocessor and Macros
track: c
module: Advanced Language
moduleOrder: 4
order: 1
level: intermediate
summary: What the C preprocessor does and doesn't understand, the classic macro bugs with measurements, safe macro idioms (do-while(0), statement expressions, _Generic), X-macros, conditional compilation, and when to prefer inline functions and enums.
prerequisites:
  - Functions, Headers and Multi-File Programs
  - The Compilation Pipeline and the ELF Object Model
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.10 Preprocessing directives; §6.10.3 Macro replacement; §6.10.8 Predefined macro names"
  - label: "GNU CPP manual (The C Preprocessor)"
    url: https://gcc.gnu.org/onlinedocs/cpp/
  - label: "SEI CERT C — PRE00-C to PRE32-C (preprocessor rules)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+01.+Preprocessor+%28PRE%29
  - label: "Linux kernel coding style — Chapter 12 Macros, Enums and RTL; include/linux/minmax.h"
    url: https://docs.kernel.org/process/coding-style.html#macros-enums-and-rtl
  - label: "ISO/IEC 9899:2024 (C23) — typeof, __VA_OPT__, #embed, __has_include, static_assert as a keyword"
  - label: "GNU CPP manual — Search Path; Once-Only Headers (the multiple-include optimization)"
    url: https://gcc.gnu.org/onlinedocs/cpp/Once-Only-Headers.html
  - label: "GCC manual — Preprocessor Options (-H, -M, -MM, -MMD, -MP, -iquote, -isystem, -Wundef)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
  - label: "GCC manual — Statement Exprs, Typeof, Variadic Macros, Predefined Macros"
    url: https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
---

The preprocessor is a separate text-processing pass that runs *before* the compiler (see *The Compilation Pipeline and the ELF Object Model*). It knows tokens, not C: no types, no scopes, no expressions. That makes it powerful for conditional compilation, code generation and compile-time constants, and treacherous for anything that looks like a function.

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

### Watching expansion step by step

The rules above are easiest to learn by looking at real output. (The labs added in this revision were captured with clang 21 on 64-bit ARM Linux; the others with GCC 14.2 on x86-64. Preprocessor output is the same on both; diagnostics differ only in wording.) `cc -E -P` stops after preprocessing and prints what the compiler proper will see (`-P` drops the `# line` markers):

```c
// Expansion order, step by step. Run through `cc -E -P` to see what the compiler receives.
#define VERSION 3
#define XSTR(x) #x              // stringifies its argument exactly as written
#define STR(x) XSTR(x)          // the argument is macro-expanded first, then passed on
const char *a = XSTR(VERSION);  // "VERSION"
const char *b = STR(VERSION);   // "3"

#define CAT(a, b) a ## b
#define XCAT(a, b) CAT(a, b)
int CAT(var_, VERSION);         // var_VERSION: ## pastes before expansion
int XCAT(var_, VERSION);        // var_3

#define foo foo + 1             // a macro is not re-expanded inside its own expansion
int foo_result = foo;           // foo + 1, not infinite recursion

#define TWICE(x) ((x) + (x))
#define ONE 1
int r = TWICE(TWICE(ONE));      // arguments are fully expanded first, then rescanned

#define F(x) G(x)
#define G(x) [x]
int arr[] = F(F(2));            // each level of the chain is rescanned
```

```text
$ cc -E -P expand.c | grep -v '^$'
const char *a = "VERSION";
const char *b = "3";
int var_VERSION;
int var_3;
int foo_result = foo + 1;
int r = ((((1) + (1))) + (((1) + (1))));
int arr[] = [[2]];
```

What each line shows, in the order the standard's algorithm (§6.10.3) applies the rules:

- **Argument prescan.** An argument is fully macro-expanded *before* it is substituted, **unless** the parameter is used with `#` or `##`. `XSTR(VERSION)` stringifies the spelling `VERSION`; `STR(VERSION)` first expands its argument to `3` (because `STR`'s own body does not use `#`), then hands `3` to `XSTR`. The same two-level trick makes `XCAT` paste the *value* of a macro.
- **No self-recursion.** While `foo` is being expanded, the name `foo` is disabled (GCC's documentation calls such a token "painted blue"), so `#define foo foo + 1` expands once and stops. This is what lets a macro wrap a function of the same name: `#define malloc(n) malloc_logged(n, __FILE__, __LINE__)` style tricks rely on it.
- **Rescanning.** After substitution the result is scanned again for more macros, which is why `TWICE(TWICE(ONE))` and `F(F(2))` expand all the way down. Each nesting level multiplies the text: `TWICE` nested three deep would paste its argument eight times, which is how innocent-looking macro chains blow up compile times and evaluate an argument many times.

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

### Reading an error that happens inside a macro

A type error inside an expansion is reported at the call site, followed by the chain of macros it came through. Read the chain from the bottom up to find where the bad text was pasted:

```c
// How compilers report an error that is inside a macro expansion.
#define SQUARE(x) ((x) * (x))
#define AREA(r) (3 * SQUARE(r))

struct circle { int radius; };

int area_of(struct circle c) {
    return AREA(c);             /* bug: should be AREA(c.radius) */
}
```

```text
$ cc -c macroerr.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"
macroerr.c:8:12: error: invalid operands to binary expression ('struct circle' and 'struct circle')
macroerr.c:3:22: note: expanded from macro 'AREA'
macroerr.c:2:24: note: expanded from macro 'SQUARE'
```

The error is on line 8 (the call), the operator that failed is in `SQUARE` on line 2, and `AREA` on line 3 is how it got there. clang prints "expanded from macro"; GCC prints the same chain as "in expansion of macro". When the chain is long, `cc -E` on the one function shows the final text in which the error makes sense.

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

## Headers, search paths and dependency files

`#include` pastes a file, and where the preprocessor looks for it depends on the spelling:

| Form | Searched, in order (GCC and clang) |
|---|---|
| `#include "f.h"` | the directory of the file containing the `#include`; then `-iquote` directories; then everything `<f.h>` searches |
| `#include <f.h>` | `-I` directories (left to right); `-isystem` directories; the standard system directories; `-idirafter` directories |

`cc -v -E -x c /dev/null` prints the actual list ("#include <...> search starts here"). Headers found through `-isystem` and the system directories are *system headers*: most warnings inside them are suppressed, which is right for third-party headers and wrong for your own.

Two flags answer the questions "what does this file pull in?" and "what must be rebuilt when a header changes?":

```text
$ cc -Iinclude -H -c deps.c -o deps.o 2>&1 | grep -v -e stddef -e /usr/; cc -Iinclude -MMD -c deps.c -o deps.o && cat deps.d
. include/util.h
.. include/config.h
deps.o: deps.c include/util.h include/config.h
```

- **`-H`** prints each header as it is opened, indented by nesting depth. `deps.c` includes `config.h` twice (through `util.h` and directly), yet it appears once: the header is wrapped in an include guard, and GCC and clang remember that. When the guard macro is still defined they do not even reopen the file (the *multiple-include optimization*), so a properly guarded header costs nothing the second time.
- **`-MMD`** writes a Make rule (`deps.d`) listing every non-system header the object depends on, as a side effect of the normal compile. A Makefile adds `-include $(OBJS:.o=.d)`, and from then on editing `config.h` rebuilds exactly the objects that use it. Add `-MP` so that deleting a header does not break the build. Build systems such as CMake and Meson do the same automatically.

What happens without a guard:

```text
$ cc -Iinclude -c noguard.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"
include/point.h:2:8: error: redefinition of 'point'
noguard.c:1:10: note: 'include/point.h' included multiple times, additional include site here
noguard.c:2:10: note: 'include/point.h' included multiple times, additional include site here
include/point.h:2:8: note: unguarded header; consider using #ifdef guards or #pragma once
```

`struct point` is defined twice in one translation unit, which is a constraint violation. Real projects hit this through indirect inclusion (`a.h` and `b.h` both include `point.h`), never by writing the same line twice. Use a guard (`#ifndef PROJECT_PATH_POINT_H`) or `#pragma once`. `#pragma once` is shorter and supported everywhere that matters, but it identifies "the same file" by the file system, so it can be fooled by one header reachable through two paths (copies in two install trees, some network mounts). Guards cannot be fooled that way, but two headers can accidentally share a guard name, which silently hides the second one; derive the name from the path.

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

### The silent typo in `#if`

In `#if`, an identifier that is not a defined macro is replaced by `0` (§6.10.1). That is convenient (`#if HAVE_FOO` works whether or not the build system defined it) and dangerous: a misspelled name turns a feature off without any error.

```c
// A misspelled macro name in #if is silently 0: the feature quietly disappears.
#include <stdio.h>
#define FEATURE_LOGGING 1

int main(void) {
#if FEATURE_LOGING                 /* typo: one G missing */
    puts("logging enabled");
#else
    puts("logging disabled");
#endif
    return 0;
}
```

```text
$ cc -Wall ifundef.c -o ifundef && ./ifundef; cc -Wall -Wundef ifundef.c -o ifundef 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"
logging disabled
ifundef.c:6:5: warning: 'FEATURE_LOGING' is not defined, evaluates to 0 [-Wundef]
```

`-Wall` says nothing, the program silently takes the `#else` branch, and only `-Wundef` points at the typo. Two defenses: turn on `-Wundef` (and `-Werror=undef` in CI), and prefer `#if FEATURE` with the build system always defining `FEATURE` to `0` or `1` over `#ifdef FEATURE`, where a typo in either place is invisible even to `-Wundef`.

- Feature detection (`__has_include`, `__has_attribute`, `__STDC_VERSION__`) beats compiler-name sniffing; use `configure`/CMake feature checks for library features.
- `-DNAME=value` defines macros from the command line (versioning, feature toggles); quote string values for the shell (`-DBUILD_ID='"ci-1234"'`).
- Keep `#if` blocks small and symmetric; each configuration must be compiled in CI, otherwise the unused branches rot.
- **Include guards**: `#ifndef FOO_H / #define FOO_H / ... / #endif` (portable) or `#pragma once` (universal in practice). Name guards uniquely (path-based) to avoid collisions.
- **Include hygiene**: headers should be self-contained (include what they need), never define variables, and avoid including other headers unnecessarily (use forward declarations of structs). Include-what-you-use tools help; `-H` prints the include tree.

## Modern C reduces the need for tricks

C11 and C23 standardized several things that used to need compiler extensions or contorted macros. The program below uses four of them (compiled with `-std=c23`; GCC 14 and clang 18 or later accept it, and `({ … })` statement expressions are a GNU extension that both support):

```c
// Macro techniques from C11 and C23: _Generic dispatch, __VA_OPT__, typeof temporaries, static_assert.
#include <stdio.h>

// One PRINT macro for several types, chosen at compile time by the type of the argument.
#define FMT(x) _Generic((x), int: "%d", long: "%ld", double: "%g", char *: "%s", const char *: "%s")
#define PRINT(x) (printf(#x " = "), printf(FMT(x), (x)), putchar('\n'))

// __VA_OPT__(,) emits the comma only when arguments follow the format (C23; replaces GNU ", ##__VA_ARGS__").
#define LOG(fmt, ...) printf("[log] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

// Single evaluation with typeof temporaries; comparing the addresses makes mixed types a diagnostic.
#define MIN(a, b) ({ typeof(a) a_ = (a); typeof(b) b_ = (b); (void)(&a_ == &b_); a_ < b_ ? a_ : b_; })

// A compile-time check that fails the build, not the program.
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
static int table[] = {4, 8, 15, 16, 23, 42};
static_assert(ARRAY_LEN(table) == 6, "table must have six entries");

static int counter;
static int next(void) { return ++counter; }

int main(void) {
    PRINT(42);
    PRINT(42L);
    PRINT(2.5);
    PRINT("text");
    LOG("no arguments");
    LOG("%zu entries in table, last = %d", ARRAY_LEN(table), table[ARRAY_LEN(table) - 1]);
    int m = MIN(next(), 10);
    printf("MIN(next(), 10) = %d, next() was called %d time(s)\n", m, counter);
#ifdef SHOW_TYPE_CHECK
    long big = 5;
    (void)MIN(big, 3);          /* int vs long: comparison of distinct pointer types */
#endif
    return 0;
}
```

```text
$ cc -std=c23 -Wall -Wextra modern.c -o modern && ./modern; cc -std=c23 -DSHOW_TYPE_CHECK modern.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"
42 = 42
42L = 42
2.5 = 2.5
"text" = text
[log] no arguments
[log] 6 entries in table, last = 42
MIN(next(), 10) = 1, next() was called 1 time(s)
modern.c:33:11: warning: comparison of distinct pointer types ('typeof (big) *' (aka 'long *') and 'typeof (3) *' (aka 'int *')) [-Wcompare-distinct-pointer-types]
modern.c:12:73: note: expanded from macro 'MIN'
```

- **`_Generic`** (C11) picks an expression by the *type* of its controlling expression, so one `PRINT` works for `int`, `long`, `double` and strings, with no runtime cost. `#x` supplies the source text. A type with no matching association is a compile-time error (add a `default:` to make it a fallback).
- **`__VA_OPT__(,)`** (C23) produces the comma only when variadic arguments are present, so `LOG("no arguments")` expands correctly. It replaces the GNU `, ##__VA_ARGS__` extension.
- **`typeof`** (standard in C23, long a GNU extension) gives single evaluation: `MIN(next(), 10)` calls `next()` once, as the counter shows. The line `(void)(&a_ == &b_)` is the Linux kernel's type check: comparing pointers to two different types produces the diagnostic shown when `SHOW_TYPE_CHECK` mixes `long` and `int`, catching the signed/unsigned and width mismatches that make a naive `MIN` return the wrong value.
- **`static_assert`** is a keyword in C23 (`_Static_assert` in C11, with `<assert.h>` providing the macro). Combined with `ARRAY_LEN`, it turns "someone added a table entry but not the matching enum" into a build failure.

When a macro needs all four of these to be safe, it is usually a sign that a `static inline` function, perhaps one per type behind a `_Generic` front end, is the better design.

## Troubleshooting checklist

| Symptom | Likely cause | First step |
|---|---|---|
| Wrong arithmetic result from a macro | missing parentheses around a parameter or the expansion | `cc -E -P` the line; parenthesize |
| A side effect happens twice | argument used twice in the expansion | `typeof` temporaries or an inline function |
| `else` without a previous `if` | multi-statement macro without `do { } while (0)` | wrap it |
| Error message points at a line that looks fine | error inside an expansion | read the "expanded from" / "in expansion of" chain bottom-up |
| A feature is silently off | misspelled name in `#if`, or `#ifdef` on a name defined as `0` | `-Wundef`; `cc -dM -E file.c \| grep NAME` |
| "redefinition of struct" | header without a guard reached twice | add a guard or `#pragma once`; `-H` shows the path |
| Change to a header not picked up | missing dependency tracking | `-MMD -MP` and `-include` the `.d` files |
| Different behavior in two builds | different `-D` flags or include order | compare `cc -dM -E` output of both builds |

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
- Arguments are expanded before substitution except next to `#`/`##` (hence the two-level `STR`/`XSTR`); a macro never re-expands itself; the result is rescanned.
- Read macro errors from the "expanded from" chain; `cc -E` shows the final text.
- Guard every header (or `#pragma once`), see the include tree with `-H`, and let `-MMD -MP` track header dependencies.
- An undefined name in `#if` is `0`: build with `-Wundef`.
- `_Generic`, `__VA_OPT__`, `typeof` and `static_assert` replace most of the old macro tricks.
- Detect features, not compilers; compile every configuration in CI.
