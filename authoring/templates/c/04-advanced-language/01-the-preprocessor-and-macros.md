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
  - label: ISO/IEC 9899:2018 (C17) §6.10 Preprocessing directives; §6.10.3 Macro replacement; §6.10.8 Predefined macro names
  - label: GNU CPP manual (The C Preprocessor)
    url: https://gcc.gnu.org/onlinedocs/cpp/
  - label: SEI CERT C — PRE00-C to PRE32-C (preprocessor rules)
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+01.+Preprocessor+%28PRE%29
  - label: Linux kernel coding style — Chapter 12 Macros, Enums and RTL; include/linux/minmax.h
    url: https://docs.kernel.org/process/coding-style.html#macros-enums-and-rtl
  - label: "ISO/IEC 9899:2024 (C23) — typeof, __VA_OPT__, #embed, __has_include, static_assert as a keyword"
  - label: "GNU CPP manual — Search Path; Once-Only Headers (the multiple-include optimization)"
    url: https://gcc.gnu.org/onlinedocs/cpp/Once-Only-Headers.html
  - label: "GCC manual — Preprocessor Options (-H, -M, -MM, -MMD, -MP, -iquote, -isystem, -Wundef)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
  - label: GCC manual — Statement Exprs, Typeof, Variadic Macros, Predefined Macros
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

{{include c/preproc/expand.c}}

{{run c/preproc :: cc -E -P expand.c | grep -v '^$'}}

What each line shows, in the order the standard's algorithm (§6.10.3) applies the rules:

- **Argument prescan.** An argument is fully macro-expanded *before* it is substituted, **unless** the parameter is used with `#` or `##`. `XSTR(VERSION)` stringifies the spelling `VERSION`; `STR(VERSION)` first expands its argument to `3` (because `STR`'s own body does not use `#`), then hands `3` to `XSTR`. The same two-level trick makes `XCAT` paste the *value* of a macro.
- **No self-recursion.** While `foo` is being expanded, the name `foo` is disabled (GCC's documentation calls such a token "painted blue"), so `#define foo foo + 1` expands once and stops. This is what lets a macro wrap a function of the same name: `#define malloc(n) malloc_logged(n, __FILE__, __LINE__)` style tricks rely on it.
- **Rescanning.** After substitution the result is scanned again for more macros, which is why `TWICE(TWICE(ONE))` and `F(F(2))` expand all the way down. Each nesting level multiplies the text: `TWICE` nested three deep would paste its argument eight times, which is how innocent-looking macro chains blow up compile times and evaluate an argument many times.

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

### Reading an error that happens inside a macro

A type error inside an expansion is reported at the call site, followed by the chain of macros it came through. Read the chain from the bottom up to find where the bad text was pasted:

{{include c/preproc/macroerr.c}}

{{run c/preproc :: cc -c macroerr.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"}}

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

{{include c/preproc/xmacro.c}}

{{run c/preproc :: gcc -std=gnu11 -Wall -Wextra xmacro.c -o xmacro && ./xmacro 2>&1}}

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

{{run c/preproc :: cc -Iinclude -H -c deps.c -o deps.o 2>&1 | grep -v -e stddef -e /usr/; cc -Iinclude -MMD -c deps.c -o deps.o && cat deps.d}}

- **`-H`** prints each header as it is opened, indented by nesting depth. `deps.c` includes `config.h` twice (through `util.h` and directly), yet it appears once: the header is wrapped in an include guard, and GCC and clang remember that. When the guard macro is still defined they do not even reopen the file (the *multiple-include optimization*), so a properly guarded header costs nothing the second time.
- **`-MMD`** writes a Make rule (`deps.d`) listing every non-system header the object depends on, as a side effect of the normal compile. A Makefile adds `-include $(OBJS:.o=.d)`, and from then on editing `config.h` rebuilds exactly the objects that use it. Add `-MP` so that deleting a header does not break the build. Build systems such as CMake and Meson do the same automatically.

What happens without a guard:

{{run c/preproc :: cc -Iinclude -c noguard.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"}}

`struct point` is defined twice in one translation unit, which is a constraint violation. Real projects hit this through indirect inclusion (`a.h` and `b.h` both include `point.h`), never by writing the same line twice. Use a guard (`#ifndef PROJECT_PATH_POINT_H`) or `#pragma once`. `#pragma once` is shorter and supported everywhere that matters, but it identifies "the same file" by the file system, so it can be fooled by one header reachable through two paths (copies in two install trees, some network mounts). Guards cannot be fooled that way, but two headers can accidentally share a guard name, which silently hides the second one; derive the name from the path.

## Conditional compilation done well

{{include c/preproc/cond.c}}

{{run c/preproc :: gcc -std=c11 -Wall -DBUILD_ID='"ci-1234"' cond.c -o cond && ./cond | head -1; gcc -std=c23 cond.c -o cond23 && ./cond23 | head -1}}

### The silent typo in `#if`

In `#if`, an identifier that is not a defined macro is replaced by `0` (§6.10.1). That is convenient (`#if HAVE_FOO` works whether or not the build system defined it) and dangerous: a misspelled name turns a feature off without any error.

{{include c/preproc/ifundef.c}}

{{run c/preproc :: cc -Wall ifundef.c -o ifundef && ./ifundef; cc -Wall -Wundef ifundef.c -o ifundef 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"}}

`-Wall` says nothing, the program silently takes the `#else` branch, and only `-Wundef` points at the typo. Two defenses: turn on `-Wundef` (and `-Werror=undef` in CI), and prefer `#if FEATURE` with the build system always defining `FEATURE` to `0` or `1` over `#ifdef FEATURE`, where a typo in either place is invisible even to `-Wundef`.

- Feature detection (`__has_include`, `__has_attribute`, `__STDC_VERSION__`) beats compiler-name sniffing; use `configure`/CMake feature checks for library features.
- `-DNAME=value` defines macros from the command line (versioning, feature toggles); quote string values for the shell (`-DBUILD_ID='"ci-1234"'`).
- Keep `#if` blocks small and symmetric; each configuration must be compiled in CI, otherwise the unused branches rot.
- **Include guards**: `#ifndef FOO_H / #define FOO_H / ... / #endif` (portable) or `#pragma once` (universal in practice). Name guards uniquely (path-based) to avoid collisions.
- **Include hygiene**: headers should be self-contained (include what they need), never define variables, and avoid including other headers unnecessarily (use forward declarations of structs). Include-what-you-use tools help; `-H` prints the include tree.

## Modern C reduces the need for tricks

C11 and C23 standardized several things that used to need compiler extensions or contorted macros. The program below uses four of them (compiled with `-std=c23`; GCC 14 and clang 18 or later accept it, and `({ … })` statement expressions are a GNU extension that both support):

{{include c/preproc/modern.c}}

{{run c/preproc :: cc -std=c23 -Wall -Wextra modern.c -o modern && ./modern; cc -std=c23 -DSHOW_TYPE_CHECK modern.c -o /dev/null 2>&1 | grep -E "^[a-z/.]+:[0-9]+:[0-9]+: (warning|error|note)"}}

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
