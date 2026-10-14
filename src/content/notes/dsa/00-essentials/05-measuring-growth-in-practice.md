---
title: "Measuring Growth: Counting and Timing in Practice"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 5
level: beginner
summary: "A hands-on bridge to the complexity notes: the doubling experiment that reveals whether code is linear, n log n or quadratic by counting steps, how to read the ratios, and how to time real code honestly, with compiled programs."
prerequisites:
  - "Thinking in Data Structures"
  - "Searching and Sorting for Beginners"
sources:
  - label: "Bentley — Programming Pearls, 2nd ed., Column 8 (Algorithm Design Techniques) and Appendix 3 (Cost Models)"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §1.4 (Analysis of Algorithms: the doubling test)"
  - label: "man 2 clock_gettime — CLOCK_MONOTONIC"
    url: https://man7.org/linux/man-pages/man2/clock_gettime.2.html
---

*Complexity Analysis and Amortized Analysis*, the next DSA note, is full of proofs and notation. Before reading it, learn the experimental habit that makes the notation meaningful: **do not guess how an algorithm grows; measure it.** The method, the **doubling experiment**, is simple enough that you can run it on any code you write.

## The doubling experiment

Run the code with input size *n*, then 2*n*, 4*n*, 8*n*, and look at how the cost grows each time the input doubles:

| Cost multiplies by... | When n doubles | The code is about... |
|---|---|---|
| 1 | | constant, O(1) |
| 2 | | linear, O(n) |
| a bit over 2 | | O(n log n) |
| 4 | | quadratic, O(n²) |
| 8 | | cubic, O(n³) |

The first program counts basic steps for three loop shapes, so the result does not depend on how fast your computer is:

```c
#include <stdio.h>

/* Three loops with different growth. Each counts its basic steps, so the experiment does not depend on machine speed. */
static long linear(long n)    { long steps = 0; for (long i = 0; i < n; i++) steps++; return steps; }
static long nlogn(long n)     { long steps = 0; for (long i = 0; i < n; i++) for (long k = 1; k < n; k *= 2) steps++; return steps; }
static long quadratic(long n) { long steps = 0; for (long i = 0; i < n; i++) for (long j = 0; j < n; j++) steps++; return steps; }

int main(void) {
    printf("%8s %14s %14s %16s %10s\n", "n", "O(n) steps", "O(n log n)", "O(n^2) steps", "n^2 ratio");
    long prev = 0;
    for (long n = 1000; n <= 16000; n *= 2) {
        long q = quadratic(n);
        char ratio[16] = "-";
        if (prev) snprintf(ratio, sizeof ratio, "%.1fx", (double)q / (double)prev);
        printf("%8ld %14ld %14ld %16ld %10s\n", n, linear(n), nlogn(n), q, ratio);
        prev = q;
    }

    return 0;
}
```

```text
$ gcc -std=gnu17 -Wall -Wextra -o growth growth.c && ./growth; rm -f growth
       n     O(n) steps     O(n log n)     O(n^2) steps  n^2 ratio
    1000           1000          10000          1000000          -
    2000           2000          22000          4000000       4.0x
    4000           4000          48000         16000000       4.0x
    8000           8000         104000         64000000       4.0x
   16000          16000         224000        256000000       4.0x
```

Read the table column by column:

- **O(n) steps** equals n exactly: 1000, 2000, 4000... doubling n doubles the work.
- **O(n log n)** goes 10,000, 22,000, 48,000, 104,000, 224,000: each doubling multiplies the work by slightly more than 2 (about 2.2 here), because the log n factor grows slowly. At 16,000 items the cost is only 14 times the size of the input.
- **O(n²)** goes 1 million, 4 million, 16 million, 64 million, 256 million: *exactly* 4.0 times more every doubling. At 16,000 items it is 16,000 times the input size.

The practical meaning: a quadratic program that takes 1 second for 10,000 items takes 100 seconds for 100,000 items and almost 3 hours for a million. A linear program does the million in 100 seconds, and an n log n program in about two and a half minutes. That gap, not the speed of the machine, decides whether a program is usable.

## Measuring real time honestly

Counting steps is exact but only works when you can add a counter. For real code, measure time, and follow a few rules so the number means something:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* How to time code honestly: repeat, use a monotonic clock, keep the best of several runs, and use the result so the
   compiler cannot delete the work. */
static double seconds(struct timespec a, struct timespec b) { return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9; }

static long sum_by_loop(const int *a, long n) { long s = 0; for (long i = 0; i < n; i++) s += a[i]; return s; }

static double best_time(const int *a, long n, long *result) {
    double best = 1e9;
    for (int run = 0; run < 7; run++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        *result = sum_by_loop(a, n);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double s = seconds(t0, t1);
        if (s < best) best = s;                              /* the minimum is the least disturbed by other activity */
    }
    return best;
}

int main(void) {
    long n = 20 * 1000 * 1000;
    int *a = malloc((size_t)n * sizeof *a);
    if (!a) return 1;
    for (long i = 0; i < n; i++) a[i] = (int)(i % 7);

    long r1 = 0, r2 = 0;
    double t1 = best_time(a, n / 2, &r1), t2 = best_time(a, n, &r2);
    printf("sum of %ld ints = %ld, sum of %ld ints = %ld\n", n / 2, r1, n, r2);
    printf("best time for n/2: %.1f ms, for n: %.1f ms, ratio %.1f (a linear loop should give about 2)\n", t1 * 1e3, t2 * 1e3, t2 / t1);
    free(a);
    return 0;
}
```

```text
$ gcc -std=gnu17 -O2 -Wall -Wextra -o timing timing.c && ./timing; rm -f timing
sum of 10000000 ints = 29999994, sum of 20000000 ints = 59999997
best time for n/2: 11.0 ms, for n: 22.4 ms, ratio 2.0 (a linear loop should give about 2)
```

The absolute times depend on your machine (they are sample output from one run), but the **ratio** is stable: doubling the number of integers roughly doubles the time, which is the signature of a linear loop. The rules illustrated by the code:

1. **Use a monotonic clock** (`CLOCK_MONOTONIC`), not wall-clock time, which can jump when the system clock is adjusted.
2. **Repeat and keep the minimum.** Other programs, interrupts and cold caches only ever make a run slower, so the fastest of several runs is the least disturbed.
3. **Use the result.** The program stores the sum; if the result were never used, an optimising compiler could delete the whole loop and you would time nothing. Always check that the answer is correct as well.
4. **Time the optimised build you will actually ship** (`-O2`), and compare variants under the same flags.
5. **Compare ratios across sizes** instead of absolute seconds, and use sizes large enough to take milliseconds; too-small inputs measure timer noise.
6. **Test more than one input shape**: sorted, reversed, random and all-equal inputs can behave differently (see *Searching and Sorting for Beginners*, where insertion sort changes by a factor of 1,000 between sorted and random input).

## What the experiment cannot tell you

- **Constants matter** for small *n*: an O(n²) algorithm with tiny constants can beat an O(n log n) one for 20 items. That is why library sorts switch to insertion sort for small ranges.
- **Memory behaviour** (cache misses) can make two algorithms of the same complexity differ by 10x.
- **Experiments sample inputs; proofs cover all of them.** A test might miss the worst case, which is why later notes prove bounds as well as measure.

## Pitfalls

- **Timing a single run** and trusting it.
- **Timing with the optimiser off** (`-O0`) and drawing conclusions about production speed.
- **A benchmark whose result is unused**, which the compiler removes.
- **Including setup in the timing** (allocating and filling the array) when you meant to time only the algorithm.
- **Believing a ratio from tiny sizes**; run bigger sizes until the pattern is stable.

## Key takeaways

1. Double the input and watch the cost: ×2 is linear, a bit more than ×2 is n log n, ×4 is quadratic.
2. Counting steps is machine-independent and exact; timing needs a monotonic clock, repetition and the minimum of several runs.
3. Make sure the compiler cannot delete the work, and benchmark the optimised build on several input shapes.
4. Growth rate decides usability at scale; constants and caches decide the rest, so measure as well as prove.
