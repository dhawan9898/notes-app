---
title: "Practice: Search Bounds, Stability and Counting"
track: dsa
module: Searching and Sorting
moduleOrder: 3
order: 4
level: advanced
summary: "Four exercises on the searching and sorting module: lower and upper bound binary searches that count occurrences, demonstrating that selection sort is unstable while insertion sort is stable, a stable counting sort without comparisons, and counting inversions with merge sort, each with a verified C answer."
prerequisites:
  - "Binary Search and Its Variants"
  - "Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort"
  - "Non-Comparison Sorts and Selection"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §2.3, Ch. 8 (Sorting in Linear Time)"
  - label: "Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)"
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §5.1.1 (Inversions) and §6.2.1 (Searching an Ordered Table)"
---

Most sorting and searching bugs are boundary mistakes. Each exercise below has a precise invariant to state before you write any code.

## Exercises

**1. Bounds.** In the sorted array `{1, 3, 3, 3, 5, 8, 8, 13}`, implement `lower_bound(x)` (first index with `a[i] >= x`) and `upper_bound(x)` (first index with `a[i] > x`). Use them to count occurrences of 3, 8, 4, 0 and 20 in O(log n). What does `lower_bound` return for an absent value?

**2. Stability.** Sort the records `ana:2 bob:2 cy:1 dee:2 eli:1` (already in name order) by grade with selection sort and with insertion sort. Which preserves the name order among equal grades, and why does it matter?

**3. Counting sort.** Sort ten ages in the range 0 to 10 without any comparisons. Give the three steps, the cost, and explain why the final pass runs backwards.

**4. Inversions.** An **inversion** is a pair `i < j` with `a[i] > a[j]`. Count them in `{8, 4, 2, 1, 7, 3, 9, 5}` in O(n log n) by extending merge sort, and verify against the O(n²) definition.

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>

/* First index i with a[i] >= x (n if none): the "lower bound". Invariant: everything left of lo is < x, everything from hi on is >= x. */
static int lower_bound(const int *a, int n, int x) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1; else hi = mid;
    }
    return lo;
}
static int upper_bound(const int *a, int n, int x) {           /* first index with a[i] > x */
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] <= x) lo = mid + 1; else hi = mid;
    }
    return lo;
}

int main(void) {
    int a[] = {1, 3, 3, 3, 5, 8, 8, 13};
    int n = (int)(sizeof a / sizeof a[0]);
    int queries[] = {3, 8, 4, 0, 20};
    for (int i = 0; i < 5; i++) {
        int x = queries[i], lo = lower_bound(a, n, x), hi = upper_bound(a, n, x);
        printf("x = %2d: lower_bound %d, upper_bound %d, occurrences %d%s\n", x, lo, hi, hi - lo, hi == lo ? "  (absent: lower_bound is where it would be inserted)" : "");
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_search p3_search.c && ./p3_search; rm -f p3_search
x =  3: lower_bound 1, upper_bound 4, occurrences 3
x =  8: lower_bound 5, upper_bound 7, occurrences 2
x =  4: lower_bound 4, upper_bound 4, occurrences 0  (absent: lower_bound is where it would be inserted)
x =  0: lower_bound 0, upper_bound 0, occurrences 0  (absent: lower_bound is where it would be inserted)
x = 20: lower_bound 8, upper_bound 8, occurrences 0  (absent: lower_bound is where it would be inserted)
```

Both functions use the half-open interval `[lo, hi)` and the invariant "everything left of `lo` is less than (or, for `upper_bound`, not greater than) `x`; everything from `hi` on is not". The loop is `while (lo < hi)` with `hi = mid` in one branch and `lo = mid + 1` in the other, so the interval shrinks every time and the loop always terminates. The answer is where `lo` and `hi` meet. `upper_bound − lower_bound` is the number of occurrences: 3 for the value 3, 2 for 8, and 0 for the absent 4. For an absent value, `lower_bound` is the **insertion point** that keeps the array sorted (4 would go at index 4; 20 at index 8 = n, one past the end). These two functions replace most hand-written binary searches ("find first", "find last", "count", "insert position"), and they match C++'s `std::lower_bound` and `std::upper_bound`.

**2.**

```c
#include <stdio.h>
#include <string.h>

typedef struct { const char *name; int grade; } Student;

static void print(const char *label, const Student *s, int n) {
    printf("%-26s", label);
    for (int i = 0; i < n; i++) printf(" %s:%d", s[i].name, s[i].grade);
    printf("\n");
}

static void selection_by_grade(Student *s, int n) {            /* swaps over long distances: NOT stable */
    for (int i = 0; i < n - 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++) if (s[j].grade < s[m].grade) m = j;
        Student t = s[i]; s[i] = s[m]; s[m] = t;
    }
}
static void insertion_by_grade(Student *s, int n) {            /* moves items only past strictly larger ones: stable */
    for (int i = 1; i < n; i++) {
        Student key = s[i]; int j = i - 1;
        while (j >= 0 && s[j].grade > key.grade) { s[j + 1] = s[j]; j--; }
        s[j + 1] = key;
    }
}

int main(void) {
    const Student start[] = {{"ana", 2}, {"bob", 2}, {"cy", 1}, {"dee", 2}, {"eli", 1}};    /* already in name order */
    Student a[5], b[5];
    memcpy(a, start, sizeof start); memcpy(b, start, sizeof start);
    print("input (by name):", start, 5);
    selection_by_grade(a, 5); print("selection sort by grade:", a, 5);
    insertion_by_grade(b, 5); print("insertion sort by grade:", b, 5);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_stable p3_stable.c && ./p3_stable; rm -f p3_stable
input (by name):           ana:2 bob:2 cy:1 dee:2 eli:1
selection sort by grade:   cy:1 eli:1 ana:2 dee:2 bob:2
insertion sort by grade:   cy:1 eli:1 ana:2 bob:2 dee:2
```

A sort is **stable** if records with equal keys keep their original relative order. Insertion sort moves an item only past *strictly larger* ones, so `ana, bob, dee` stay in order. Selection sort swaps the smallest item with the one at position *i*, which can jump an item over equal ones: here the swap that brought `cy` to the front threw `ana` far to the right and finally put `dee` before `bob`. Stability matters when you sort by more than one key in passes ("sort by name, then by grade" should leave names sorted within each grade), for sorting records in user interfaces, and for radix sort, which *requires* stable passes. Merge sort and insertion sort are stable; selection sort, heapsort and typical quicksort are not (and `qsort` makes no guarantee). To make an unstable sort stable, add the original index as a tie-breaker.

**3.**

```c
#include <stdio.h>

#define MAXV 10                                                 /* values are known to be 0..10 */

int main(void) {
    int ages[] = {7, 3, 10, 3, 0, 7, 7, 1, 10, 3};
    int n = (int)(sizeof ages / sizeof ages[0]);
    int count[MAXV + 1] = {0}, out[10];
    for (int i = 0; i < n; i++) count[ages[i]]++;                /* 1. histogram */
    for (int v = 1; v <= MAXV; v++) count[v] += count[v - 1];   /* 2. prefix sums: count[v] = number of items <= v */
    for (int i = n - 1; i >= 0; i--) out[--count[ages[i]]] = ages[i];   /* 3. place from the back: keeps equal keys in order (stable) */
    printf("sorted:");
    for (int i = 0; i < n; i++) printf(" %d", out[i]);
    printf("\ncost: O(n + k) = %d + %d steps, no comparisons at all\n", n, MAXV + 1);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_counting p3_counting.c && ./p3_counting; rm -f p3_counting
sorted: 0 1 3 3 3 7 7 7 10 10
cost: O(n + k) = 10 + 11 steps, no comparisons at all
```

Step 1 builds a **histogram** (how many of each value); step 2 turns it into **prefix sums**, so `count[v]` becomes the number of items less than or equal to `v`, which is the position just after the last `v` in the output; step 3 places each item at `--count[value]`. Scanning the input **backwards** puts the *last* of equal items in the *last* slot, so equal keys keep their original order: the sort is stable, which is what makes counting sort usable as the building block of radix sort. The cost is O(n + k) time and space, with k the number of possible values: linear, and it beats the O(n log n) comparison-sort lower bound because it **never compares** items, it uses the key as an array index. That only works when k is small (a range of ten million values needs a ten-million-entry table).

**4.**

```c
#include <stdio.h>
#include <string.h>

/* Merge sort that also counts inversions: pairs (i < j) with a[i] > a[j]. An inversion is found whenever an element
   from the right half is placed before elements still waiting in the left half. */
static long sort_count(int *a, int *tmp, int lo, int hi) {
    if (hi - lo < 2) return 0;
    int mid = lo + (hi - lo) / 2;
    long inv = sort_count(a, tmp, lo, mid) + sort_count(a, tmp, mid, hi);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { tmp[k++] = a[j++]; inv += mid - i; }              /* a[j] is smaller than ALL of a[i..mid) */
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (size_t)(hi - lo) * sizeof *a);
    return inv;
}

static long brute(const int *a, int n) { long c = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (a[i] > a[j]) c++; return c; }

int main(void) {
    int a[] = {8, 4, 2, 1, 7, 3, 9, 5}, copy[8], tmp[8];
    memcpy(copy, a, sizeof a);
    long fast = sort_count(a, tmp, 0, 8);
    printf("inversions by merge sort: %ld, by brute force: %ld\n", fast, brute(copy, 8));
    printf("sorted:"); for (int i = 0; i < 8; i++) printf(" %d", a[i]); printf("\n");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_inversions p3_inversions.c && ./p3_inversions; rm -f p3_inversions
inversions by merge sort: 13, by brute force: 13
sorted: 1 2 3 4 5 7 8 9
```

During the merge, when the next item comes from the **right** half, it is smaller than *every* element still waiting in the left half (they are sorted and all larger), so it forms an inversion with each of them: add `mid - i` at once instead of counting one by one. Inversions inside the halves were counted recursively, and the merge counts the pairs that cross the halves, so every pair is counted exactly once. The total of 13 matches the brute-force double loop, which is the right way to test such code (compare a clever algorithm against an obviously correct slow one on small inputs). The cost is O(n log n). Inversions measure **how unsorted** an array is: 0 for sorted input, n(n−1)/2 for reversed input, and insertion sort's running time is exactly n plus the number of inversions, which is why it excels on nearly sorted data.

</details>

## Key takeaways

1. Write binary searches as lower/upper bound on a half-open interval; the difference counts occurrences and an absent key yields its insertion point.
2. Stability keeps equal keys in input order; insertion and merge sort are stable, selection sort and heapsort are not, and an index tie-breaker fixes any sort.
3. Counting sort is linear when the key range is small, and stays stable by filling the output from the back.
4. Counting inversions in merge sort adds the remaining left-half size whenever a right-half item is taken; test clever code against a brute-force version.
