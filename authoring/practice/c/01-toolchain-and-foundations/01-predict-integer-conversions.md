---
kind: question
title: Predict integer conversions
module: Toolchain and Language Foundations
difficulty: Medium
---

On a typical 64-bit Linux system with 32-bit `int` and signed `char`, what do these print? (a) `uint8_t x = 200, y = 100; printf("%d", x + y);` and the same with `(uint8_t)(x + y)`; (b) `printf("%d", (int)(char)200);` (c) `printf("%d", -1 < 1u);` (d) `-7 / 2`, `-7 % 2`, `7 / -2`; (e) `0.1 + 0.2 == 0.3`.

## Explanation

The answers come from running the program below.

```c
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Exercise 3: add two ints without ever overflowing. Return false if the true sum does not fit. */
static bool safe_add(int a, int b, int *out) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return false;
    *out = a + b;
    return true;
}

int main(void) {
    /* Exercise 1: predict, then compare */
    uint8_t x = 200, y = 100;
    printf("x + y = %d (promoted to int), (uint8_t)(x + y) = %d\n", x + y, (uint8_t)(x + y));
    printf("(char)200 as int = %d on this platform (char is %s)\n", (int)(char)200, (char)-1 < 0 ? "signed" : "unsigned");
    printf("-1 < 1u is %d (the -1 converts to unsigned and becomes %u)\n", -1 < 1u, (unsigned)-1);
    printf("-7 / 2 = %d, -7 %% 2 = %d, 7 / -2 = %d\n", -7 / 2, -7 % 2, 7 / -2);
    printf("1.0f/3 == 1.0/3 is %d (float and double round differently)\n", 1.0f / 3 == 1.0 / 3);
    printf("0.1 + 0.2 == 0.3 is %d\n", 0.1 + 0.2 == 0.3);

    /* Exercise 2: sizes and limits */
    printf("sizeof: char %zu short %zu int %zu long %zu long long %zu void* %zu\n",
           sizeof(char), sizeof(short), sizeof(int), sizeof(long), sizeof(long long), sizeof(void *));
    printf("INT_MAX = %d, INT_MIN = %d, UINT_MAX = %u\n", INT_MAX, INT_MIN, UINT_MAX);

    /* Exercise 3: safe_add */
    int r = 0;
    bool ok1 = safe_add(2000000000, 2000000000, &r);
    printf("safe_add(2000000000, 2000000000) -> %s\n", ok1 ? "fits" : "would overflow");
    bool ok2 = safe_add(2000000000, -5, &r);                       /* separate statements: argument order is unspecified */
    printf("safe_add(2000000000, -5) -> %s (%d)\n", ok2 ? "fits" : "would overflow", r);

    /* Exercise 4: unsigned wrap-around IS defined; signed overflow is not */
    unsigned u = UINT_MAX;
    u++;
    printf("UINT_MAX + 1 wraps to %u (defined behaviour)\n", u);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p1 p1_types.c 2>&1 | grep -c warning; ./p1; rm -f p1
1
x + y = 300 (promoted to int), (uint8_t)(x + y) = 44
(char)200 as int = -56 on this platform (char is signed)
-1 < 1u is 0 (the -1 converts to unsigned and becomes 4294967295)
-7 / 2 = -3, -7 % 2 = -1, 7 / -2 = -3
1.0f/3 == 1.0/3 is 0 (float and double round differently)
0.1 + 0.2 == 0.3 is 0
sizeof: char 1 short 2 int 4 long 8 long long 8 void* 8
INT_MAX = 2147483647, INT_MIN = -2147483648, UINT_MAX = 4294967295
safe_add(2000000000, 2000000000) -> would overflow
safe_add(2000000000, -5) -> fits (1999999995)
UINT_MAX + 1 wraps to 0 (defined behaviour)
```

(The warning count line shows that the compiler flags the signed/unsigned comparison.) Explanations:

- (a) `x + y` is `300`: both operands are promoted to `int` before the addition, so nothing wraps. Casting the result to `uint8_t` keeps only the low 8 bits: 300 mod 256 = `44`.
- (b) `(char)200` is `-56` where `char` is signed (the conversion of an out-of-range value to a signed type is implementation-defined; GCC wraps modulo 256).
- (c) `-1 < 1u` is **0** (false): the usual arithmetic conversions turn the `int` into `unsigned`, so `-1` becomes 4294967295.
- (d) C99 and later *truncate toward zero*: `-7 / 2 = -3` and the remainder takes the sign of the dividend, `-7 % 2 = -1`; `7 / -2 = -3`.
- (e) `0.1 + 0.2 == 0.3` is false: none of the three is exactly representable in binary floating point. Compare with a tolerance.
