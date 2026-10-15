---
title: "Practice: Types, Conversions and Undefined Behavior"
track: c
module: Toolchain and Language Foundations
moduleOrder: 1
order: 4
level: intermediate
summary: "Six exercises on the first module: predicting integer promotion and conversion results, ranges and sizes, overflow-safe addition, telling defined from undefined behaviour with UBSan, the unsigned loop trap, and which build stage reports which error, with verified answers."
prerequisites:
  - "The Compilation Pipeline and the ELF Object Model"
  - "C Standards, the Abstract Machine and Undefined Behavior"
  - "Types, Integer Promotion, Conversions and Overflow"
sources:
  - label: ISO/IEC 9899:2018 §6.3 (Conversions), §6.5 (Expressions), Annex J.2 (Undefined behavior)
  - label: "SEI CERT C Coding Standard — INT30-C, INT31-C, INT32-C (integer rules)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+04.+Integers+%28INT%29
  - label: Clang UndefinedBehaviorSanitizer documentation
    url: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
---

Reading about integer promotion is easy; predicting it is a skill. Try each exercise **before** opening the answers, and compile everything with `gcc -std=c17 -Wall -Wextra -g -fsanitize=undefined,address`. Guessing and being wrong is the point: the surprise is what you will remember.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Predict (paper).** On a typical 64-bit Linux system with 32-bit `int` and signed `char`, what do these print? (a) `uint8_t x = 200, y = 100; printf("%d", x + y);` and the same with `(uint8_t)(x + y)`; (b) `printf("%d", (int)(char)200);` (c) `printf("%d", -1 < 1u);` (d) `-7 / 2`, `-7 % 2`, `7 / -2`; (e) `0.1 + 0.2 == 0.3`.

**2. Ranges.** Without running anything, write the value of `INT_MAX`, `INT_MIN` and `UINT_MAX` for a 32-bit `int`. Then print them and `sizeof` of `char`, `short`, `int`, `long`, `long long` and a pointer on your machine.

**3. Overflow-safe addition.** Write `bool safe_add(int a, int b, int *out)` that stores `a + b` and returns `true` only when the true sum fits in an `int`, **without ever performing an overflowing addition**. Test it with `2000000000 + 2000000000` and `2000000000 + (-5)`.

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/types/ex3-safe-add.c}}

</details>

**4. Defined or undefined?** For each, say whether the behaviour is defined, implementation-defined or undefined, and why: (a) `unsigned u = UINT_MAX; u++;` (b) `int i = INT_MAX; i++;` (c) `int n = 1 << 31;` (d) `int a[4]; a[4] = 0;` (e) `(int)3.99` (f) `char c = 300;` Run (b) under UBSan and read the report.

**5. Fix the countdown.** This loop is meant to visit indexes `n-1` down to `0` for a `size_t n`. It never terminates (it is capped here only by a safety counter). Explain why and write the correct loop. What does the compiler say if you build with `-Wextra`?

```c
for (size_t i = n - 1; i >= 0 && steps < 5; i--) steps++;
```

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/types/ex5-countdown.c}}

</details>

**6. Which stage complains?** For each problem, name the build stage that reports it (preprocessor, compiler, assembler, linker or run time): (a) `#include <stdoi.h>` (b) calling `square(7)` without a prototype under `-std=c17` (c) calling a function that is declared but defined in no file (d) `int x = "hello";` (e) an off-by-one write past a local array.

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.** The answers come from running the program below.

{{include c/practice/p1_types.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o p1 p1_types.c 2>&1 | grep -c warning; ./p1; rm -f p1}}

(The warning count line shows that the compiler flags the signed/unsigned comparison.) Explanations:

- (a) `x + y` is `300`: both operands are promoted to `int` before the addition, so nothing wraps. Casting the result to `uint8_t` keeps only the low 8 bits: 300 mod 256 = `44`.
- (b) `(char)200` is `-56` where `char` is signed (the conversion of an out-of-range value to a signed type is implementation-defined; GCC wraps modulo 256).
- (c) `-1 < 1u` is **0** (false): the usual arithmetic conversions turn the `int` into `unsigned`, so `-1` becomes 4294967295.
- (d) C99 and later *truncate toward zero*: `-7 / 2 = -3` and the remainder takes the sign of the dividend, `-7 % 2 = -1`; `7 / -2 = -3`.
- (e) `0.1 + 0.2 == 0.3` is false: none of the three is exactly representable in binary floating point. Compare with a tolerance.

**2.** `INT_MAX = 2147483647`, `INT_MIN = -2147483648`, `UINT_MAX = 4294967295` (the program above prints them with the sizes: 1, 2, 4, 8, 8 and 8 on LP64 Linux).

**3.** `safe_add` in the program above tests *before* adding: if `b > 0` the sum overflows exactly when `a > INT_MAX - b`; if `b < 0` it underflows when `a < INT_MIN - b`. The subtractions on the right cannot overflow because `b` has the opposite sign. GCC and Clang also provide `__builtin_add_overflow(a, b, out)`, and C23 adds `ckd_add` in `<stdckdint.h>`.

**4.**

- (a) **Defined**: unsigned arithmetic wraps modulo 2^N, so `u` becomes 0.
- (b) **Undefined**: signed overflow. The compiler may assume it never happens. Run it:

{{include c/practice/p1_ub.c}}

{{run c/practice :: gcc -std=c17 -g -fsanitize=undefined -o ub p1_ub.c && ./ub 2>&1 | sed 's#/[^ ]*/##'; rm -f ub}}

UBSan names the exact line and operands. The safe route is to do the arithmetic in `unsigned` and convert back (value implementation-defined) or to use the checked helpers from exercise 3.
- (c) **Undefined** in C17 for a 32-bit `int`: shifting a 1 into the sign bit overflows. Use `1u << 31`.
- (d) **Undefined**: writing one past the end of an array (AddressSanitizer reports a stack-buffer-overflow).
- (e) **Defined**: conversion from floating point truncates toward zero, giving `3`, provided the value fits.
- (f) **Implementation-defined**: 300 does not fit a `char`; GCC wraps to 44. It compiles silently, but `-Woverflow` warns.

**5.** `size_t` is unsigned, so `i >= 0` is true for every value; decrementing 0 wraps to a huge number instead of going negative, and the loop only stops because of the extra counter. `-Wextra` reports "comparison of unsigned expression in '>= 0' is always true". The idiom that works for any unsigned counter tests before it decrements:

{{include c/practice/p1_signcmp.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o sc p1_signcmp.c 2>&1 | grep -c "always true"; ./sc; rm -f sc}}

`for (size_t i = n; i-- > 0; )` reads "while `i` is positive, decrement it and use the new value", so the body sees `n-1` down to `0`, and `n == 0` runs zero times.

**6.** (a) **Preprocessor**: "No such file or directory" for the header. (b) **Compiler**: "implicit declaration of function" is an error from C99 on (see *Functions, Headers and Multi-File Programs*). (c) **Linker**: "undefined reference to `square`". (d) **Compiler**: initialisation of `int` from `char *` (a diagnostic; many compilers treat it as an error by default now). (e) **Run time**: no build stage sees it; only a sanitizer or a crash reveals it.

</details>

## Key takeaways

1. Operands narrower than `int` are promoted before arithmetic, and signed values mix with unsigned ones by converting to unsigned, so comparisons can silently flip.
2. Unsigned overflow wraps (defined); signed overflow, out-of-range shifts and out-of-bounds accesses are undefined, so check *before* the operation.
3. UBSan and AddressSanitizer turn invisible undefined behaviour into precise reports; build with them while learning.
4. Use `i-- > 0` for countdown loops over unsigned types, and compile with `-Wextra` to catch always-true comparisons.
