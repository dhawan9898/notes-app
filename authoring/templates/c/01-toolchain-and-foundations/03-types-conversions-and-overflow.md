---
title: Types, Integer Promotion, Conversions and Overflow
track: c
module: Toolchain and Language Foundations
moduleOrder: 1
order: 3
level: intermediate
summary: The C type system as it behaves on real machines — fixed-width and size types, the integer promotion and usual arithmetic conversion rules (with the bugs they cause), IEEE 754 floating point, and safe checked arithmetic.
sources:
  - label: ISO/IEC 9899:2018 (C17) §6.2.5 Types, §6.3 Conversions, §6.5 Expressions, §7.20 Integer types, §7.22 General utilities; Annex F (IEEE 754)
  - label: IEEE 754-2019 — Standard for Floating-Point Arithmetic
  - label: Goldberg — What Every Computer Scientist Should Know About Floating-Point Arithmetic (ACM Computing Surveys, 1991)
    url: https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
  - label: SEI CERT C — INT30-C, INT31-C, INT32-C, INT02-C (integer rules)
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+04.+Integers+%28INT%29
  - label: ISO/IEC 9899:2024 (C23) §7.20 <stdckdint.h> checked integer arithmetic
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf
---

C's arithmetic looks simple and hides more bug-producing rules than any other part of the language. The fixes are mostly mechanical once you know the rules.

## The integer types

| Type | Size (typical LP64 Linux) | Range / notes |
|---|---|---|
| `char` | 8 bits | signed *or* unsigned — **implementation-defined** (signed on x86, unsigned on ARM Linux) |
| `signed char` / `unsigned char` | 8 | `unsigned char` is the type for raw bytes |
| `short` | 16 | at least 16 bits |
| `int` | 32 | at least 16 bits; the "natural" type; arithmetic happens at least at this width |
| `long` | **64** on LP64 (Linux/macOS 64-bit), **32** on LLP64 (Windows) | at least 32 |
| `long long` | 64 | at least 64 |
| `_Bool` / `bool` | 8 | holds 0 or 1 |

The standard guarantees only minimum ranges and `sizeof(char) == 1`; `CHAR_BIT` (usually 8) is in `<limits.h>`, limits `INT_MAX`, `LONG_MIN`, etc.  Signed types may use any representation per C17, but C23 mandates **two's complement**; overflow of signed types remains undefined.

**Use the right type for the job:**

| Need | Use |
|---|---|
| Exact width (protocols, files, hardware registers) | `int8_t … int64_t`, `uint8_t … uint64_t` (`<stdint.h>`) |
| Sizes, counts, array indexes, `sizeof` results | `size_t` (unsigned); `ssize_t` (POSIX, signed) for "bytes or −1"; `ptrdiff_t` for pointer differences |
| Integer large enough to hold a pointer | `uintptr_t` / `intptr_t` |
| Fast type at least N bits | `int_fast32_t` (rarely needed) |
| Largest integer | `intmax_t` / `uintmax_t` |
| Characters/bytes | `char` for text, `unsigned char`/`uint8_t` for bytes |
| Time, file offsets | `time_t`, `off_t` (build with `-D_FILE_OFFSET_BITS=64` on 32-bit) |

Printing: `%d` int, `%u` unsigned, `%ld` long, `%lld` long long, **`%zu` size_t, `%zd` ssize_t/ptrdiff_t**, `%p` pointers (cast to `void *`), `PRId64`/`PRIu32` from `<inttypes.h>` for fixed-width types. A wrong specifier is undefined behavior (`-Wformat=2` warns).

## Conversion rules

### Integer promotions

Before arithmetic or bitwise operation, any operand of type narrower than `int` (`char`, `short`, `_Bool`, bit-fields, enums) is **promoted to `int`** (or `unsigned int` if `int` can't hold all its values). Then the **usual arithmetic conversions** bring both operands to a common type:

1. If either is a floating type, convert to the "larger" floating type (`long double` > `double` > `float`).
2. Otherwise promote both; if the types are the same, done.
3. If both are signed or both unsigned, convert to the one with greater rank.
4. If the unsigned operand has rank ≥ the signed one's, the **signed operand is converted to unsigned**.
5. Otherwise, if the signed type can represent all values of the unsigned type, convert to the signed type; else convert both to the unsigned version of the signed type.

Rule 4 is the notorious one: `-1 < 1u` is false. A program that exercises the traps (`conversions.c`):

{{include c/types/conversions.c}}

{{run c/types :: gcc -O0 -Wall -Wextra -Wsign-compare conversions.c -o conversions 2>&1 | sed 's/^conversions.c://' | cut -c1-150; ./conversions}}

Reading the output:

- `-1 < 1u` → **0**: `-1` is converted to `4294967295u`. GCC warns with `-Wsign-compare` (it is in `-Wextra`). The same trap applies to `if (len < sizeof buf)` with a negative `len`: always check for negative values before comparing with a `size_t`.
- `a + b` with two `uint8_t` is **300**: both were promoted to `int`, no wraparound. Wrapping happens only when assigning back into `uint8_t` (`44`): implicit narrowing silently truncates (`-Wconversion` can flag some cases).
- `~m` on a `uint8_t` yields `0xfffffff0`: the promotion to `int` happens first, so the complement sets all upper bits. Compare with `(uint8_t)~m == 0xf0`. This is a frequent bug in bit-twiddling code and when comparing `~x == 0xF0`.
- Plain `char` holding 200 prints `-56` where `char` is signed. Never use `char` for numeric data or index tables with it without casting to `unsigned char` (`isalpha((unsigned char)c)`).
- `x * y` with two `int`s is computed in `int` and overflows (undefined behavior) *before* it is widened for the `long long` assignment; cast **one operand first**: `(long long)x * y`.
- `(int)0x1FFFFFFFF` is implementation-defined (GCC wraps modulo 2^32: `-1`); converting an out-of-range *floating* value to an integer type is undefined.

### Unsigned and signed arithmetic

- **Unsigned arithmetic wraps modulo 2^N** (well-defined): `UINT_MAX + 1 == 0`, `0u - 1 == UINT_MAX`. Use unsigned types for hash functions, bit manipulation, checksums, and ring-buffer indices.
- **Signed overflow is undefined** (see the UB note): the optimizer assumes it never happens.
- **Division/modulo**: `INT_MIN / -1` and `INT_MIN % -1` are UB (overflow); `/` truncates toward zero since C99, so `-7 / 2 == -3` and `-7 % 2 == -1` (the remainder has the sign of the dividend; use `((a % n) + n) % n` for a mathematical mod).
- **Shifts**: shifting by a negative amount or by ≥ the width of the promoted left operand is UB; right shift of a negative signed value is implementation-defined (arithmetic on GCC); left shift of a negative value, or of a value whose result is not representable, is UB (avoid it). Use unsigned operands: `1u << 31` is fine, `1 << 31` is overflow in old standards. Shifting `uint8_t` first promotes to `int`.
- **`abs(INT_MIN)`** overflows; `-INT_MIN` too. Handle explicitly.

### The `size_t` countdown trap

```c
for (size_t i = n - 1; i >= 0; i--)    /* BUG: i >= 0 is always true for unsigned; infinite loop (or i = SIZE_MAX if n == 0) */
for (size_t i = n; i-- > 0; )          /* correct: tests then decrements; visits n-1 ... 0 */
```

Likewise `n - 1` when `n == 0` wraps to `SIZE_MAX`. Prefer `for (i = 0; i < n; i++)` or the post-decrement idiom above.

### Pointers and integers

Converting between pointers and integers is implementation-defined; `uintptr_t` round-trips a `void *`. Subtracting pointers into the same array gives `ptrdiff_t`. Don't store pointers in `int` (truncates on 64-bit; the classic porting bug).

## Safe arithmetic

Check *before* the operation (since after signed overflow it's too late) or use compiler support:

{{include c/types/safe_arith.c}}

{{run c/types :: gcc -std=c23 -Wall -Wextra safe_arith.c -o safe_arith && ./safe_arith}}

- `__builtin_add_overflow` / `_sub_` / `_mul_` (GCC ≥ 5, Clang) compute in infinite precision and report whether the result fits the destination; the stored result is the wrapped value.
- C23 `<stdckdint.h>` standardizes `ckd_add`, `ckd_sub`, `ckd_mul` with the same semantics.
- For size computations before `malloc`: `calloc(n, size)` performs the overflow check for you; otherwise `if (n != 0 && size > SIZE_MAX / n) fail;`. **Integer overflow in allocation size arithmetic is a classic source of heap overflows** (`malloc(n * sizeof(struct item))`).
- Note the lesson hidden in the example: an earlier draft printed `r` in the same `printf` as the call that sets it. Argument evaluation order is **unspecified**, so `r` was read first. Separate side effects from uses.
- Saturating or wrapping behavior must be explicit; use unsigned types for intended wrap.

## Floating point

`float` (32-bit), `double` (64-bit), `long double` (x87 80-bit on x86) follow **IEEE 754** on virtually all platforms. A value is `(-1)^sign × 1.fraction × 2^(exponent − bias)`:

| Format | Sign | Exponent | Fraction | Decimal digits |
|---|---|---|---|---|
| binary32 (`float`) | 1 | 8 (bias 127) | 23 | ~7 |
| binary64 (`double`) | 1 | 11 (bias 1023) | 52 | ~15–16 |

Bit patterns inspected with `memcpy` (the defined way to view an object's bytes):

{{include c/types/floats.c}}

{{run c/types :: gcc -O0 -Wall -Wextra floats.c -o floats -lm && ./floats}}

Consequences:

- Most decimal fractions are not representable: `0.1f` is stored as `0.100000001`; `0.1 + 0.2 == 0.3` is **false** (`0.30000000000000004`). Never compare floats with `==` after arithmetic; compare with a tolerance appropriate to the magnitude (`fabs(a-b) <= eps * fmax(fabs(a), fabs(b))`), or compare integers (cents instead of dollars).
- **Precision runs out**: above 2^53 a `double` can't represent every integer (`2^53 + 1 == 2^53`); a `float` above 2^24 can't count by ones (`16777216 + 1` stays `16777216`). Accumulating many small values into a large sum loses them; sum in order of increasing magnitude, use **Kahan/pairwise summation**, or a wider type.
- **Special values**: `±infinity` (exponent all ones, fraction zero) from overflow and division by zero (`1.0/0.0` is `inf` in IEEE arithmetic, not UB when `Annex F` applies), **NaN** (exponent all ones, fraction non-zero) from `0.0/0.0`, `sqrt(-1)`; `NaN != NaN`, so test with `isnan(x)`. Comparisons with NaN are false except `!=`. NaN propagates silently: a single NaN poisons a whole computation, and `-ffast-math` makes the compiler assume none exist.
- **Subnormals** (exponent 0) fill the gap near zero but are very slow on some CPUs; flush-to-zero flags exist.
- **Rounding** is to nearest-even by default; `fesetround` changes mode; conversions float↔int truncate toward zero, and out-of-range conversions are UB.
- **Cancellation**: subtracting nearly equal numbers destroys significant digits (`1e16 + 1 - 1e16`); reformulate formulas (e.g. quadratic formula alternate form).
- `-ffast-math` allows reassociation and ignores NaN/Inf/signed zero: faster and **not IEEE-conforming**; results can change by compiler version. Use `-O2` plus targeted flags unless you've verified accuracy.
- x87 `long double` / excess precision on 32-bit x86 (`FLT_EVAL_METHOD`) is the reason the same code gave different results on old builds; SSE2 (x86-64) avoids this.
- Money: use integers (minor units) or decimal libraries, never binary floats.
- Format: `%g` for compact output, `%.17g` round-trips a `double` exactly, `%.9g` for `float`; `printf("%f", float_arg)` is fine because `float` arguments are promoted to `double` in variadic calls (default argument promotions), but `%d` with a double is UB.

### Default argument promotions

In variadic calls (`printf`) and unprototyped functions, `float` → `double` and small integers → `int`. This is why `%c` takes an `int` and `%f` a `double`, and why passing a `size_t` to `%d` is wrong on 64-bit.

## Type-related good practice

1. Prefer `size_t`/`unsigned` for sizes and indices but guard subtraction; use `ptrdiff_t`/`ssize_t` when negative values are meaningful. Keep types consistent across an interface; avoid mixing signed and unsigned in expressions.
2. Compile with `-Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare -Wformat=2` and treat narrowing casts as deliberate: write the cast to document intent.
3. Make sizes explicit at protocol/file boundaries (`uint32_t`) and serialize byte by byte (endianness note).
4. Use `static_assert(sizeof(int) == 4, "...")` for assumptions; use `<limits.h>` constants, not magic numbers.
5. Validate untrusted lengths: reject negatives, check against bounds, check `a + b` and `a * b` for overflow before allocating or indexing.
6. Run tests under `-fsanitize=undefined,integer` (Clang) or `-fsanitize=undefined` (GCC), which reports signed overflow and bad shifts; enable `-ftrapv` in debug builds if you want a trap.

## Key takeaways

- Operands narrower than `int` are promoted; mixed signed/unsigned comparisons convert the signed side to unsigned (`-1 < 1u` is false).
- Unsigned arithmetic wraps; signed overflow is undefined; widen *before* multiplying; guard `size_t` subtraction and countdown loops.
- Use `<stdint.h>` types at boundaries, `size_t`/`%zu` for sizes, and checked arithmetic (`__builtin_*_overflow`, C23 `ckd_*`) around allocations and user input.
- IEEE 754 doubles have 53 bits of precision; don't compare with `==`, expect NaN/Inf, and prefer integers for money and counters.
