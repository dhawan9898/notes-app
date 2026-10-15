---
title: "Thinking in Data Structures"
track: dsa
module: Getting Started
moduleOrder: 0
order: 1
level: beginner
summary: "The beginner's map: what a data structure is, how to count work and read Big-O, why binary search beats linear search by a thousand times, why n-squared algorithms stop working, what recursion is, the call stack, and why memoization turns 2.7 million calls into 59."
prerequisites:
  - "C From Zero: Your First Programs"
  - Pointers for Beginners
sources:
  - label: "Bentley — Programming Pearls, 2nd ed., Columns 1-2 and 4"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 1-3"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., Ch. 1.4 (Analysis of Algorithms) and 1.1"
---

## Data structures are about operations

Imagine a phone book. You could keep the contacts in a drawer in any order, or sorted by name, or in an index card box by first letter. The *data* is the same; what changes is how fast you can **find** someone, **add** someone, or **list** everyone alphabetically. A data structure is that choice: it stores the same information so that some operations are fast, usually at the cost of making others slower or using more memory.

Four things you will keep asking about every structure in this track:

1. **Lookup**: is this item in there? find it.
2. **Insert / delete**: add or remove an item.
3. **Order**: can I get items in sorted order, or the smallest quickly?
4. **Cost**: how much time and memory as the amount of data grows?

| Structure | Good at | Bad at |
|---|---|---|
| Array | reading item number *i*; scanning in order | inserting in the middle |
| Linked list | inserting/removing at a known place | finding the *i*-th item |
| Stack / queue | last-in-first-out / first-in-first-out access | anything else |
| Hash table | lookup by key in about one step | keeping items sorted |
| Sorted array + binary search | very fast lookup | inserting (must shift items) |
| Tree | lookup *and* sorted order *and* inserts | simplicity |
| Heap | quickly getting the smallest | arbitrary lookup |
| Graph | modelling connections | (it is a model, not a container) |

## Counting work

To compare methods we do not time them on one computer; we **count the basic steps** as a function of the input size *n*. The program below counts the comparisons needed to find a number in a sorted list of *n* numbers (worst case: the number is at the end) with two methods, and also the work of comparing every pair of items.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Count the work (comparisons) three ways of finding a value among n numbers need. */
static long linear_steps(const int *a, int n, int key) { long steps = 0; for (int i = 0; i < n; i++) { steps++; if (a[i] == key) break; } return steps; }
static long binary_steps(const int *a, int n, int key) { long steps = 0; int lo = 0, hi = n - 1; while (lo <= hi) { steps++; int mid = lo + (hi - lo) / 2; if (a[mid] == key) break; if (a[mid] < key) lo = mid + 1; else hi = mid - 1; } return steps; }
static long pairs_steps(int n) { long steps = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) steps++; return steps; }   /* compare every pair */

int main(void) {
    printf("%10s | %14s %14s %16s\n", "n", "linear search", "binary search", "all pairs (n^2)");
    for (int n = 10; n <= 10000000; n *= 10) {
        int *a = malloc((size_t)n * sizeof *a); for (int i = 0; i < n; i++) a[i] = 2 * i;       /* sorted */
        long lin = linear_steps(a, n, 2 * (n - 1)), bin = binary_steps(a, n, 2 * (n - 1));        /* worst case for linear: the key is last */
        if (n <= 10000) printf("%10d | %14ld %14ld %16ld\n", n, lin, bin, pairs_steps(n));
        else printf("%10d | %14ld %14ld %16s\n", n, lin, bin, "(too slow to run)");
        free(a);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -O2 -Wall -Wextra -o growth growth.c && ./growth
         n |  linear search  binary search  all pairs (n^2)
        10 |             10              4               45
       100 |            100              7             4950
      1000 |           1000             10           499500
     10000 |          10000             14         49995000
    100000 |         100000             17 (too slow to run)
   1000000 |        1000000             20 (too slow to run)
  10000000 |       10000000             24 (too slow to run)
```

- **Linear search** looks at items one by one: for *n* items it takes up to *n* steps. 10 million items, 10 million steps.
- **Binary search** (only for *sorted* data) looks at the middle item and throws away half the list each step: **24 steps** for 10 million items. Doubling the data adds *one* step.
- **All pairs** (compare every item with every other) takes about *n × n / 2* steps: 45 for 10 items, but about 5 *billion* for 100,000, so we did not even run it.

## Big-O: describing growth

**Big-O notation** is shorthand for how the work grows with *n*, ignoring constant factors and small terms:

| Notation | Name | If n goes 10× bigger the work goes… | Example |
|---|---|---|---|
| O(1) | constant | stays the same | reading `a[i]` |
| O(log n) | logarithmic | +a few steps | binary search |
| O(n) | linear | 10× | scanning a list |
| O(n log n) | linearithmic | a bit more than 10× | good sorting |
| O(n²) | quadratic | 100× | comparing all pairs; simple sorts |
| O(2ⁿ) | exponential | impossible for n > ~40 | trying every subset |

Read it as "roughly proportional to". Big-O hides constants (a simple O(n) loop can lose to a clever O(n log n) one for small n, and memory access patterns matter a lot in practice; the later notes measure this), but it tells you which approach **stops working** as data grows. An O(n²) method that is fine for 1,000 items takes a million times longer for a million items.

How to read code: one loop over *n* items is O(n); a loop inside a loop is O(n²); halving the problem each step is O(log n); recursion that splits in two and combines in O(n) is O(n log n). *Complexity Analysis and Amortized Analysis* makes all of this precise.

## Recursion: solving a problem with a smaller copy of itself

A **recursive** function calls itself on a smaller input until it reaches a **base case** it can answer directly. Two rules: (1) there must be a base case, (2) every call must move toward it. `n!` is the textbook example: `n! = n × (n−1)!` with `1! = 1`.

```c
#include <stdio.h>

static unsigned long long factorial(int n) { return n <= 1 ? 1 : (unsigned long long)n * factorial(n - 1); }   /* n! = n * (n-1)!, base case 1! = 1 */

static long calls;
static long fib(int n) { calls++; return n < 2 ? n : fib(n - 1) + fib(n - 2); }                                /* simple but repeats work */

static long memo[100], memo_calls;
static long fib_memo(int n) { memo_calls++; if (n < 2) return n; if (memo[n]) return memo[n]; return memo[n] = fib_memo(n - 1) + fib_memo(n - 2); }

static int moves, verbose;
static void hanoi(int n, char from, char to, char via) {                                                       /* move n disks from `from` to `to` */
    if (n == 0) return;
    hanoi(n - 1, from, via, to);                                                                                /* 1. clear the way */
    moves++; if (verbose) printf("  move disk %d from %c to %c\n", n, from, to);                                  /* 2. move the biggest */
    hanoi(n - 1, via, to, from);                                                                                /* 3. put the others back on top */
}

static void countdown(int n) {                                                                                  /* watch the call stack grow and unwind */
    printf("%*senter %d\n", (3 - n) * 2, "", n);
    if (n > 0) countdown(n - 1);
    printf("%*sleave %d\n", (3 - n) * 2, "", n);
}

int main(void) {
    printf("5! = %llu, 10! = %llu, 20! = %llu\n", factorial(5), factorial(10), factorial(20));
    calls = 0; long f = fib(30); printf("fib(30) = %ld using %ld calls;  ", f, calls);
    long fm = fib_memo(30); printf("with memoization fib(30) = %ld using %ld calls\n", fm, memo_calls);
    printf("Towers of Hanoi with 3 disks:\n"); moves = 0; verbose = 1; hanoi(3, 'A', 'C', 'B');
    printf("%d moves (2^3 - 1); with 20 disks: ", moves); moves = 0; verbose = 0; hanoi(20, 'A', 'C', 'B'); printf("%d moves (2^20 - 1)\n", moves);
    printf("call stack for countdown(3):\n"); countdown(3);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o recursion recursion.c && ./recursion
5! = 120, 10! = 3628800, 20! = 2432902008176640000
fib(30) = 832040 using 2692537 calls;  with memoization fib(30) = 832040 using 59 calls
Towers of Hanoi with 3 disks:
  move disk 1 from A to C
  move disk 2 from A to B
  move disk 1 from C to B
  move disk 3 from A to C
  move disk 1 from B to A
  move disk 2 from B to C
  move disk 1 from A to C
7 moves (2^3 - 1); with 20 disks: 1048575 moves (2^20 - 1)
call stack for countdown(3):
enter 3
  enter 2
    enter 1
      enter 0
      leave 0
    leave 1
  leave 2
leave 3
```

- **`factorial`**: base case `n <= 1`, otherwise multiply `n` by the factorial of a smaller number.
- **The call stack.** The last lines show what happens inside the computer: each call *enters*, calls the next, and only when the deepest call returns does each earlier call *leave*. Each active call occupies **stack memory**, so very deep recursion (hundreds of thousands of levels) crashes with a stack overflow (see *Recursion, Divide and Conquer, and Loop Invariants*).
- **Towers of Hanoi** shows recursion's power: to move *n* disks, move *n−1* out of the way, move the biggest, then move the *n−1* back. Three disks need 7 moves; 20 disks need 1,048,575 (2ⁿ − 1): exponential growth, no matter how clever the code.
- **Fibonacci** shows its danger. `fib(n) = fib(n−1) + fib(n−2)` is correct but **recomputes the same values over and over**: `fib(30)` made 2.7 million calls. Remembering answers already computed (**memoization**) reduces it to **59 calls**. Avoiding repeated work like this is the idea of **dynamic programming** (see the note of that name).

## A recipe for solving a problem

1. **Understand it**: write down 2 or 3 small examples with the correct answers, including an empty or one-element input.
2. **Start with the obvious (brute-force) solution**, however slow. It becomes your test oracle.
3. **Ask what is repeated or wasted**: repeated lookups suggest a hash table or sorting; repeated recomputation suggests memoization; "smallest so far" suggests a heap; "search in sorted data" suggests binary search.
4. **Improve**, then **test the improved version against brute force** on many small random inputs.
5. **Check edge cases**: empty input, one element, duplicates, negative numbers, the largest input size.
6. **State the cost** in Big-O for time and memory.

## Pitfalls

- Choosing a structure by habit instead of by the operations needed.
- Optimising code before checking whether the algorithm is the bottleneck.
- Assuming binary search works on unsorted data.
- Recursion without a base case, or whose input does not shrink (infinite recursion = stack overflow).
- Ignoring constant factors and memory behavior entirely, or only looking at them.
- Off-by-one errors in loops and ranges (the most common bug in this whole track).

## Key takeaways

1. A data structure is a trade-off: it makes some operations fast and others slow.
2. Count steps, not seconds; Big-O says how work grows with the input size.
3. Binary search is O(log n); comparing all pairs is O(n²), which stops working quickly.
4. Recursion = base case + smaller problem; every active call uses stack memory.
5. Memoization removes repeated work (fib(30): 2,692,537 calls → 59).
6. Brute force first, then improve, then test the fast version against the slow one.

**Next:** the practice problems for this module, then *Complexity Analysis and Amortized Analysis*.
