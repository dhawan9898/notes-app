---
kind: question
title: Classify the loops
module: Foundations
difficulty: Medium
---

For each loop shape, give the Big-O of the number of times the innermost statement runs, then predict the ratio when `n` doubles from 64 to 128.

- A: `for i in 0..n` (single loop)
- B: `for (i = 1; i < n; i *= 2)` (the counter doubles)
- C: `for i in 0..n { for j in 0..n }`
- D: `for i in 0..n { for j in i..n }` (the inner loop starts at `i`)
- E: `for (i = 1; i < n; i *= 2) { for j in 0..i }`
- F: `for i in 0..n { for (j = 1; j < n; j *= 2) }`
- G: two separate loops of `n` iterations, one after the other

## Explanation

```c
#include <stdio.h>

/* Count how many times the innermost statement runs for n = 64 and n = 128, then read the growth. */
static long a(long n) { long c = 0; for (long i = 0; i < n; i++) c++; return c; }                                   /* A */
static long b(long n) { long c = 0; for (long i = 1; i < n; i *= 2) c++; return c; }                                /* B */
static long c3(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 0; j < n; j++) c++; return c; }     /* C */
static long d(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = i; j < n; j++) c++; return c; }      /* D */
static long e(long n) { long c = 0; for (long i = 1; i < n; i *= 2) for (long j = 0; j < i; j++) c++; return c; }   /* E */
static long f(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 1; j < n; j *= 2) c++; return c; }   /* F */
static long g(long n) { long c = 0; for (long i = 0; i < n; i++) { c++; } for (long j = 0; j < n; j++) { c++; } return c; } /* G */

int main(void) {
    struct { const char *name; long (*fn)(long); } loops[] = {{"A", a}, {"B", b}, {"C", c3}, {"D", d}, {"E", e}, {"F", f}, {"G", g}};
    printf("%-4s %10s %10s %8s\n", "loop", "n = 64", "n = 128", "ratio");
    for (int i = 0; i < 7; i++) {
        long x = loops[i].fn(64), y = loops[i].fn(128);
        printf("%-4s %10ld %10ld %8.2f\n", loops[i].name, x, y, (double)y / (double)x);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p1_loops p1_loops.c && ./p1_loops; rm -f p1_loops
loop     n = 64    n = 128    ratio
A            64        128     2.00
B             6          7     1.17
C          4096      16384     4.00
D          2080       8256     3.97
E            63        127     2.02
F           384        896     2.33
G           128        256     2.00
```

- **A** is **O(n)**: exactly n steps; doubling n doubles the work (ratio 2.00).
- **B** is **O(log n)**: the counter doubles each time, so about log₂ n steps (6, then 7: one extra step when n doubles; ratio 1.17).
- **C** is **O(n²)**: n × n steps; doubling n quadruples the work (4.00).
- **D** is **O(n²)** too: the inner loop runs n, n−1, ..., 1 times, a sum of n(n+1)/2 steps (2080 for n = 64: 64 × 65 / 2). It is half of C, but the constant does not change the class; the ratio approaches 4 (3.97).
- **E** is **O(n)**, a classic surprise: the inner loop runs 1 + 2 + 4 + ... + n/2 times, a geometric series that sums to about n (63 for n = 64). Two nested loops are not automatically quadratic; look at how many times the inner one runs *in total*. Ratio 2.02.
- **F** is **O(n log n)**: an outer loop of n and an inner loop of log n (n × 6 = 384 for n = 64). Ratio 2.33, a little over 2.
- **G** is **O(n)**: loops *in sequence* add (n + n = 2n); only *nested* loops multiply. Ratio 2.00.

The doubling ratio (1, 2, slightly above 2, 4) is the practical test from *Measuring Growth: Counting and Timing in Practice*.
