---
title: Types, Integer Promotion, Conversions and Overflow
track: c
module: Toolchain and Language Foundations
moduleOrder: 1
order: 3
level: intermediate
summary: The C type system as it behaves on real machines — fixed-width and size types, the integer promotion and usual arithmetic conversion rules (with the bugs they cause), IEEE 754 floating point, and safe checked arithmetic.
prerequisites:
  - Binary, Hexadecimal and Bits
  - C Standards, the Abstract Machine and Undefined Behavior
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.2.5 Types, §6.3 Conversions, §6.5 Expressions, §7.20 Integer types, §7.22 General utilities; Annex F (IEEE 754)"
  - label: "IEEE 754-2019 — Standard for Floating-Point Arithmetic"
  - label: "Goldberg — What Every Computer Scientist Should Know About Floating-Point Arithmetic (ACM Computing Surveys, 1991)"
    url: https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
  - label: "SEI CERT C — INT30-C, INT31-C, INT32-C, INT02-C (integer rules)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+04.+Integers+%28INT%29
  - label: "ISO/IEC 9899:2024 (C23) §7.20 <stdckdint.h> checked integer arithmetic"
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

```c
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* 1. usual arithmetic conversions: signed is converted to unsigned */
    int i = -1;
    unsigned int u = 1;
    printf("-1 < 1u            -> %d   (i is converted to %u)\n", i < u, (unsigned)i);

    /* 2. integer promotion: small types become int before arithmetic */
    uint8_t a = 200, b = 100;
    printf("a + b (uint8_t)    -> %d   (computed in int, not wrapped)\n", a + b);
    uint8_t sum = a + b;
    printf("stored in uint8_t  -> %d   (implicit narrowing)\n", sum);

    /* 3. promotion surprises with unary operators */
    uint8_t m = 0x0F;
    printf("~m                 -> 0x%x (promoted to int)\n", ~m);
    printf("(uint8_t)~m        -> 0x%x\n", (uint8_t)~m);

    /* 4. plain char signedness */
    char c = (char)200;
    printf("(char)200          -> %d   (char is %s here)\n", c, (char)-1 < 0 ? "signed" : "unsigned");

    /* 5. size_t is unsigned: the countdown loop trap */
    size_t n = 3, steps = 0;
    for (size_t k = n - 1; k < n; k--) steps++;   /* correct idiom: stops when k wraps to SIZE_MAX */
    printf("countdown iterations = %zu (using the k < n wrap test)\n", steps);

    /* 6. a signed length compared with a size_t capacity */
    long len = -5;
    size_t cap = 10;
    printf("len(-5) < cap(10)  -> %d   (len is converted to a huge unsigned value)\n", len < cap);
    printf("len < 0 || (size_t)len < cap -> %d   (check the sign first)\n", len < 0 || (size_t)len < cap);

    /* 7. widening multiplication happens too late */
    int x = 100000, y = 100000;
    long long wrong = x * y;                 /* overflow in int arithmetic (UB!) */
    (void)wrong;
    long long right = (long long)x * y;
    printf("(long long)x * y   -> %lld\n", right);

    /* 8. abs and INT_MIN */
    printf("-INT_MIN as unsigned is %u; INT_MIN = %d\n", (unsigned)INT_MIN, INT_MIN);

    /* 9. narrowing conversions */
    long big = 0x1FFFFFFFFL;
    int narrowed = (int)big;                 /* implementation-defined (GCC: modulo 2^32) */
    printf("(int)0x1FFFFFFFF   -> %d\n", narrowed);
    return 0;
}
```

```text
$ gcc -O0 -Wall -Wextra -Wsign-compare conversions.c -o conversions 2>&1 | sed 's/^conversions.c://' | cut -c1-150; ./conversions
 In function ‘main’:
11:69: warning: comparison of integer expressions of different signedness: ‘int’ and ‘unsigned int’ [-Wsign-compare]
   11 |     printf("-1 < 1u            -> %d   (i is converted to %u)\n", i < u, (unsigned)i);
      |                                                                     ^
36:92: warning: comparison of integer expressions of different signedness: ‘long int’ and ‘size_t’ {aka ‘long unsigned int’} [-Wsign-compa
   36 |     printf("len(-5) < cap(10)  -> %d   (len is converted to a huge unsigned value)\n", len < cap);
      |                                                                                            ^
-1 < 1u            -> 0   (i is converted to 4294967295)
a + b (uint8_t)    -> 300   (computed in int, not wrapped)
stored in uint8_t  -> 44   (implicit narrowing)
~m                 -> 0xfffffff0 (promoted to int)
(uint8_t)~m        -> 0xf0
(char)200          -> -56   (char is signed here)
countdown iterations = 3 (using the k < n wrap test)
len(-5) < cap(10)  -> 0   (len is converted to a huge unsigned value)
len < 0 || (size_t)len < cap -> 1   (check the sign first)
(long long)x * y   -> 10000000000
-INT_MIN as unsigned is 2147483648; INT_MIN = -2147483648
(int)0x1FFFFFFFF   -> -1
```

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

```c
#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool add_checked(int a, int b, int *out) { return !__builtin_add_overflow(a, b, out); }

/* portable pre-check for size computations: a * b fits in size_t? */
static bool mul_size(size_t a, size_t b, size_t *out) {
    if (b != 0 && a > SIZE_MAX / b) return false;
    *out = a * b;
    return true;
}

int main(void) {
    int r;
    bool ok = add_checked(INT_MAX, 1, &r);
    printf("INT_MAX + 1 via builtin: %s\n", ok ? "ok" : "overflow detected");
    ok = add_checked(100, 23, &r);                       /* evaluate first: printf argument order is unspecified */
    printf("100 + 23 via builtin:    %s (r=%d)\n", ok ? "ok" : "overflow", r);
    int c;
    bool of = ckd_mul(&c, 100000, 100000);              /* C23 <stdckdint.h> */
    printf("ckd_mul(100000,100000): overflow=%d wrapped result=%d\n", of, c);
    size_t bytes;
    ok = mul_size(SIZE_MAX / 2, 4, &bytes);
    printf("mul_size(SIZE_MAX/2, 4): %s\n", ok ? "ok" : "overflow detected");
    ok = mul_size(1000, 24, &bytes);
    printf("mul_size(1000, 24):      %s (%zu)\n", ok ? "ok" : "overflow", bytes);
    return 0;
}
```

```text
$ gcc -std=c23 -Wall -Wextra safe_arith.c -o safe_arith && ./safe_arith
INT_MAX + 1 via builtin: overflow detected
100 + 23 via builtin:    ok (r=123)
ckd_mul(100000,100000): overflow=1 wrapped result=1410065408
mul_size(SIZE_MAX/2, 4): overflow detected
mul_size(1000, 24):      ok (24000)
```

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

```c
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void bits(const char *label, float f) {
    uint32_t u; memcpy(&u, &f, sizeof u);             /* memcpy is the defined way to inspect the bits */
    printf("%-10s %-12.9g sign=%u exp=%3u (unbiased %4d) frac=0x%06x\n", label, f, u >> 31, (u >> 23) & 0xFF, (int)((u >> 23) & 0xFF) - 127, u & 0x7FFFFF);
}

int main(void) {
    bits("1.0", 1.0f); bits("0.1", 0.1f); bits("-2.5", -2.5f); bits("inf", INFINITY); bits("nan", NAN); bits("denorm", 1e-40f);
    printf("\n0.1 + 0.2 == 0.3 -> %d  (0.1+0.2 = %.17g)\n", 0.1 + 0.2 == 0.3, 0.1 + 0.2);
    printf("NAN == NAN -> %d, NAN != NAN -> %d, isnan -> %d\n", NAN == NAN, NAN != NAN, isnan(NAN));
    double big = 9007199254740992.0;                    /* 2^53 */
    printf("2^53 + 1 == 2^53 -> %d  (doubles hold 53 bits of integer precision)\n", big + 1 == big);
    printf("FLT_EPSILON=%g DBL_EPSILON=%g\n", FLT_EPSILON, DBL_EPSILON);
    float f = 16777216.0f;                              /* 2^24 */
    printf("float 2^24 + 1 = %.1f\n", f + 1.0f);
    double a = 1e308 * 10;
    printf("1e308 * 10 = %g, -1/0.0 = %g, 0.0/0.0 = %g\n", a, -1 / 0.0, 0.0 / 0.0);
    double sum = 0; for (int i = 0; i < 10; i++) sum += 0.1;
    printf("sum of ten 0.1 = %.17g, fabs(sum-1) < 1e-9 -> %d\n", sum, fabs(sum - 1.0) < 1e-9);
    return 0;
}
```

```text
$ gcc -O0 -Wall -Wextra floats.c -o floats -lm && ./floats
1.0        1            sign=0 exp=127 (unbiased    0) frac=0x000000
0.1        0.100000001  sign=0 exp=123 (unbiased   -4) frac=0x4ccccd
-2.5       -2.5         sign=1 exp=128 (unbiased    1) frac=0x200000
inf        inf          sign=0 exp=255 (unbiased  128) frac=0x000000
nan        nan          sign=0 exp=255 (unbiased  128) frac=0x400000
denorm     9.9999461e-41 sign=0 exp=  0 (unbiased -127) frac=0x0116c2

0.1 + 0.2 == 0.3 -> 0  (0.1+0.2 = 0.30000000000000004)
NAN == NAN -> 0, NAN != NAN -> 1, isnan -> 1
2^53 + 1 == 2^53 -> 1  (doubles hold 53 bits of integer precision)
FLT_EPSILON=1.19209e-07 DBL_EPSILON=2.22045e-16
float 2^24 + 1 = 16777216.0
1e308 * 10 = inf, -1/0.0 = -inf, 0.0/0.0 = -nan
sum of ten 0.1 = 0.99999999999999989, fabs(sum-1) < 1e-9 -> 1
```

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
