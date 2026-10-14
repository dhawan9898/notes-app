---
title: "Searching and Sorting for Beginners"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 1
level: beginner
summary: "Linear search versus binary search with measured step counts, then selection sort and insertion sort written by hand with counted comparisons, why sorted input changes the cost, and how to call the C library's qsort, as the gentle on-ramp to the complexity and sorting notes."
prerequisites:
  - "Thinking in Data Structures"
  - "More Control Flow: switch, Loop Control and Recursion"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 2 (Getting Started: insertion sort) and §2.3"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §1.1 (binary search) and §2.1 (elementary sorts)"
  - label: "Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)"
  - label: "ISO/IEC 9899:2018 §7.22.5.2 (The qsort function)"
---

Almost every program searches for something or puts things in order. This note is the first, gentle look at both. The algorithms are short enough to read in one sitting, and the programs *count* how much work each does, so you see growth with your own eyes rather than taking Big-O on faith. The full treatment comes later in *Binary Search and Its Variants* and *Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort*.

## Searching: look at everything, or halve the problem

**Linear search** checks the items one by one until it finds the target. It needs no preparation and works on any array, but the worst case examines every item.

**Binary search** requires the array to be **sorted**. Look at the middle item: if it is the target you are done; if the target is bigger, the left half cannot contain it, so throw that half away; otherwise throw the right half away. Repeat on what is left. Each step halves the remaining items, so 1,000 items need at most about 10 steps and a million need about 20.

```c
#include <stdio.h>

/* Linear search: look at every item until found. Works on any array. */
static int linear_search(const int *a, int n, int target, int *steps) {
    for (int i = 0; i < n; i++) {
        (*steps)++;
        if (a[i] == target) return i;
    }
    return -1;
}

/* Binary search: needs a SORTED array; each step throws away half of what is left. */
static int binary_search(const int *a, int n, int target, int *steps) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;          /* not (lo + hi) / 2: that can overflow for huge arrays */
        (*steps)++;
        if (a[mid] == target) return mid;
        if (a[mid] < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int main(void) {
    int a[1000];
    for (int i = 0; i < 1000; i++) a[i] = i * 3;          /* sorted: 0, 3, 6, ... 2997 */

    int targets[] = {2997, 1500, 7};                       /* last item, middle item, absent */
    for (int t = 0; t < 3; t++) {
        int s1 = 0, s2 = 0;
        int i1 = linear_search(a, 1000, targets[t], &s1);
        int i2 = binary_search(a, 1000, targets[t], &s2);
        printf("target %4d: linear -> index %4d in %4d steps | binary -> index %4d in %2d steps\n",
               targets[t], i1, s1, i2, s2);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o search search.c && ./search; rm -f search
target 2997: linear -> index  999 in 1000 steps | binary -> index  999 in 10 steps
target 1500: linear -> index  500 in  501 steps | binary -> index  500 in  9 steps
target    7: linear -> index   -1 in 1000 steps | binary -> index   -1 in 10 steps
```

Read the numbers: finding the last item takes the linear search 1,000 steps and binary search 10. Even for an item that is *absent*, binary search needs only 10 steps. The price is that the data must already be sorted, and sorting costs time too. That trade-off, preparing data once so that many later questions are cheap, appears in nearly every data structure.

Two details in the code are worth copying. `mid = lo + (hi - lo) / 2` instead of `(lo + hi) / 2` avoids integer overflow when the array is huge. The loop condition `lo <= hi` and the updates `mid + 1` and `mid - 1` are the part beginners get wrong most often: if either update were `mid`, some inputs would loop forever.

## Sorting by hand: two simple algorithms

Two simple sorts show the main idea, each in about ten lines.

- **Selection sort**: find the smallest remaining item and swap it into the next position. Whatever the input, it scans the entire unsorted part each time.
- **Insertion sort**: keep a sorted prefix at the front; take the next item and slide it left past every larger item until it fits, the way most people sort playing cards in their hand.

```c
#include <stdio.h>
#include <stdlib.h>

static void print(const char *label, const int *a, int n) {
    printf("%-10s", label);
    for (int i = 0; i < n; i++) printf(" %d", a[i]);
    printf("\n");
}

/* Selection sort: repeatedly pick the smallest remaining item and put it next. */
static long selection_sort(int *a, int n) {
    long compares = 0;
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++) { compares++; if (a[j] < a[min]) min = j; }
        int tmp = a[i]; a[i] = a[min]; a[min] = tmp;
    }
    return compares;
}

/* Insertion sort: grow a sorted prefix by sliding each new item left into place. */
static long insertion_sort(int *a, int n) {
    long compares = 0;
    for (int i = 1; i < n; i++) {
        int key = a[i], j = i - 1;
        while (j >= 0) { compares++; if (a[j] <= key) break; a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
    }
    return compares;
}

static int cmp_int(const void *x, const void *y) {      /* the comparator qsort needs */
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);                            /* never "a - b": that can overflow */
}

int main(void) {
    int small[] = {5, 2, 9, 1, 7, 3};
    int copy1[6], copy2[6];
    for (int i = 0; i < 6; i++) copy1[i] = copy2[i] = small[i];
    print("input", small, 6);
    selection_sort(copy1, 6); print("selection", copy1, 6);
    insertion_sort(copy2, 6); print("insertion", copy2, 6);

    enum { N = 2000 };
    static int big1[N], big2[N], big3[N], sorted[N];
    unsigned seed = 12345;                               /* fixed seed: same numbers every run */
    for (int i = 0; i < N; i++) { seed = seed * 1103515245u + 12345u; big1[i] = big2[i] = big3[i] = (int)((seed >> 8) % 100000); }
    for (int i = 0; i < N; i++) sorted[i] = i;           /* already sorted input */

    printf("\ncomparisons for %d items:\n", N);
    printf("  selection sort, random input : %ld\n", selection_sort(big1, N));
    printf("  insertion sort, random input : %ld\n", insertion_sort(big2, N));
    printf("  insertion sort, sorted input : %ld\n", insertion_sort(sorted, N));
    qsort(big3, N, sizeof big3[0], cmp_int);
    int ok = 1; for (int i = 1; i < N; i++) if (big3[i - 1] > big3[i]) ok = 0;
    printf("  qsort from the C library     : sorted correctly = %s\n", ok ? "yes" : "no");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o sort_basic sort_basic.c && ./sort_basic; rm -f sort_basic
input      5 2 9 1 7 3
selection  1 2 3 5 7 9
insertion  1 2 3 5 7 9

comparisons for 2000 items:
  selection sort, random input : 1999000
  insertion sort, random input : 1009561
  insertion sort, sorted input : 1999
  qsort from the C library     : sorted correctly = yes
```

What the counts show, for 2,000 items:

- **Selection sort** always makes 1,999,000 comparisons: about n²/2. It does not care whether the input is sorted.
- **Insertion sort** makes about half that on random input, and only 1,999 (just n − 1) on already-sorted input, because each new item is immediately in place. When data is *nearly* sorted, insertion sort is excellent, which is why real library sorts use it for small ranges.
- Both are **O(n²)** in general: doubling the input quadruples the work. At 2,000 items that is instant; at 2,000,000 items it would be a million times slower, which is why the better algorithms in the next notes (O(n log n)) exist.

## In real programs, call the library

You should rarely write your own sort for production code. The C library provides `qsort`, which needs a **comparator**: a function receiving pointers to two items and returning negative, zero or positive.

- Write the comparator as `(a > b) - (a < b)`. The tempting `a - b` is wrong because it can overflow for large values.
- `qsort` is not guaranteed **stable** (equal items may change relative order); the later sorting notes explain why that matters when sorting records by one field.
- Searching a sorted array has a library partner, `bsearch`, with the same comparator style.

## Choosing between them

| Situation | Reasonable choice |
|---|---|
| Few items, searched once | linear search; sorting is not worth it |
| Many searches over data that rarely changes | sort once, then binary search |
| Small or nearly sorted array | insertion sort |
| Large array | `qsort` (or a better algorithm from later notes) |
| Data changes constantly and must stay searchable | a different structure (hash table, tree), see later notes |

## Pitfalls

- **Binary search on unsorted data** returns wrong answers silently.
- **Off-by-one in the loop bounds** (`<` versus `<=`, `mid` versus `mid ± 1`): test with 0, 1, 2 items, the first item, the last item and an absent item.
- **Comparator that subtracts** (`return a - b;`) overflows.
- **Counting only the happy case.** Measure the worst case and the sorted/reversed inputs too.
- **Sorting inside a loop** that also changes the array, turning an O(n log n) step into O(n² log n).

## Key takeaways

1. Linear search is simple and general; binary search is far faster (about log₂ n steps) but needs sorted data.
2. Selection sort and insertion sort are O(n²); insertion sort is very fast on nearly sorted input.
3. Measure with counters before trusting an intuition; growth in n, not the speed of one run, is what matters.
4. Use `qsort` and `bsearch` with a safe comparator in real code, and move on to O(n log n) sorts for large inputs.
