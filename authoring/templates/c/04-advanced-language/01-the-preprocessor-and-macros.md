---
title: The Preprocessor and Macros
track: c
module: Advanced Language
moduleOrder: 4
order: 1
level: intermediate
summary: What the C preprocessor does and doesn't understand, the classic macro bugs with measurements, safe macro idioms (do-while(0), statement expressions, _Generic), X-macros, conditional compilation, and when to prefer inline functions and enums.
sources:
  - label: ISO/IEC 9899:2018 (C17) §6.10 Preprocessing directives; §6.10.3 Macro replacement; §6.10.8 Predefined macro names
  - label: GNU CPP manual (The C Preprocessor)
    url: https://gcc.gnu.org/onlinedocs/cpp/
  - label: SEI CERT C — PRE00-C to PRE32-C (preprocessor rules)
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+01.+Preprocessor+%28PRE%29
  - label: Linux kernel coding style — Chapter 12 Macros, Enums and RTL; include/linux/minmax.h
    url: https://docs.kernel.org/process/coding-style.html#macros-enums-and-rtl
  - label: GCC manual — Statement Exprs, Typeof, Variadic Macros, Predefined Macros
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

{{include c/preproc/pitfalls.c}}

{{run c/preproc :: gcc -std=gnu11 -Wall -Wextra pitfalls.c -o pitfalls && ./pitfalls}}

| Bug | Cause | Fix |
|---|---|---|
| `SQUARE_BAD(1+2)` = **5**, not 9 | expands to `1 + 2 * 1 + 2` | parenthesize every parameter and the whole expansion |
| `MAX_BAD(i++, j++)` increments `j` twice | argument pasted twice, evaluated twice | use an inline function, or a GNU statement expression with `__typeof__` temporaries (`MAX_SAFE`) |
| `next()` called twice | same | same |
| `if (c) SWAP_BAD(a,b); else ...` fails to compile / misbehaves | multi-statement macro breaks the `if/else` | wrap in **`do { ... } while (0)`**, which acts as exactly one statement requiring a semicolon |
| Name collision: temp `t` shadows the caller's `t` | macro-local variable names aren't hygienic | use unlikely names (`t_`, `_a`), or `__COUNTER__`/`__LINE__` pasting |

Look at the real preprocessor output for the first bug:

{{run c/preproc :: printf '#define SQUARE_BAD(x) x * x\nint v = SQUARE_BAD(1 + 2);\n' | gcc -E -P - | tail -1}}

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

{{include c/preproc/xmacro.c}}

{{run c/preproc :: gcc -std=gnu11 -Wall -Wextra xmacro.c -o xmacro && ./xmacro 2>&1}}

This program also shows:

- **`LOG`** capturing `__FILE__`, `__LINE__`, `__func__` at the call site (`[WARN] xmacro.c:42 main(): ...`), with the optional variadic tail `##__VA_ARGS__`.
- **`_Generic`** (C11): selection by the *type* of an expression at compile time; basis for type-generic macros like `<tgmath.h>`. `TYPE_NAME(1)` → `int`, `2.5` → `double`; note that a string literal selects `char*` after array-to-pointer decay.
- **`_Static_assert`** with a stringified message: compile-time checks of assumptions, in a macro.
- **`ARRAY_LEN(a)`**: `sizeof(a)/sizeof((a)[0])` is only valid for real arrays, not pointers (silently wrong for decayed parameters; the kernel's version adds a `__must_be_array` check, and GCC's `-Wsizeof-pointer-div` warns).

## Conditional compilation done well

{{include c/preproc/cond.c}}

{{run c/preproc :: gcc -std=c11 -Wall -DBUILD_ID='"ci-1234"' cond.c -o cond && ./cond | head -1; gcc -std=c23 cond.c -o cond23 && ./cond23 | head -1}}

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
