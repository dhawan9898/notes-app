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
  - label: Bentley — Programming Pearls, 2nd ed., Column 8 (Algorithm Design Techniques) and Appendix 3 (Cost Models)
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §1.4 (Analysis of Algorithms: the doubling test)
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

{{include dsa/essentials/growth.c}}

{{run dsa/essentials :: gcc -std=gnu17 -Wall -Wextra -o growth growth.c && ./growth; rm -f growth}}

Read the table column by column:

- **O(n) steps** equals n exactly: 1000, 2000, 4000... doubling n doubles the work.
- **O(n log n)** goes 10,000, 22,000, 48,000, 104,000, 224,000: each doubling multiplies the work by slightly more than 2 (about 2.2 here), because the log n factor grows slowly. At 16,000 items the cost is only 14 times the size of the input.
- **O(n²)** goes 1 million, 4 million, 16 million, 64 million, 256 million: *exactly* 4.0 times more every doubling. At 16,000 items it is 16,000 times the input size.

The practical meaning: a quadratic program that takes 1 second for 10,000 items takes 100 seconds for 100,000 items and almost 3 hours for a million. A linear program does the million in 100 seconds, and an n log n program in about two and a half minutes. That gap, not the speed of the machine, decides whether a program is usable.

## Measuring real time honestly

Counting steps is exact but only works when you can add a counter. For real code, measure time, and follow a few rules so the number means something:

{{include dsa/essentials/timing.c}}

{{run dsa/essentials :: gcc -std=gnu17 -O2 -Wall -Wextra -o timing timing.c && ./timing; rm -f timing}}

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
