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
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §2.3, Ch. 8 (Sorting in Linear Time)
  - label: Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)
  - label: Knuth — The Art of Computer Programming, Vol. 3, §5.1.1 (Inversions) and §6.2.1 (Searching an Ordered Table)
---

Most sorting and searching bugs are boundary mistakes. Each exercise below has a precise invariant to state before you write any code.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Bounds.** In the sorted array `{1, 3, 3, 3, 5, 8, 8, 13}`, implement `lower_bound(x)` (first index with `a[i] >= x`) and `upper_bound(x)` (first index with `a[i] > x`). Use them to count occurrences of 3, 8, 4, 0 and 20 in O(log n). What does `lower_bound` return for an absent value?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/sorting/ex1-bounds.c}}

</details>

**2. Stability.** Sort the records `ana:2 bob:2 cy:1 dee:2 eli:1` (already in name order) by grade with selection sort and with insertion sort. Which preserves the name order among equal grades, and why does it matter?

**3. Counting sort.** Sort ten ages in the range 0 to 10 without any comparisons. Give the three steps, the cost, and explain why the final pass runs backwards.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/sorting/ex3-counting-sort.c}}

</details>

**4. Inversions.** An **inversion** is a pair `i < j` with `a[i] > a[j]`. Count them in `{8, 4, 2, 1, 7, 3, 9, 5}` in O(n log n) by extending merge sort, and verify against the O(n²) definition.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/sorting/ex4-inversions.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p3_search.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_search p3_search.c && ./p3_search; rm -f p3_search}}

Both functions use the half-open interval `[lo, hi)` and the invariant "everything left of `lo` is less than (or, for `upper_bound`, not greater than) `x`; everything from `hi` on is not". The loop is `while (lo < hi)` with `hi = mid` in one branch and `lo = mid + 1` in the other, so the interval shrinks every time and the loop always terminates. The answer is where `lo` and `hi` meet. `upper_bound − lower_bound` is the number of occurrences: 3 for the value 3, 2 for 8, and 0 for the absent 4. For an absent value, `lower_bound` is the **insertion point** that keeps the array sorted (4 would go at index 4; 20 at index 8 = n, one past the end). These two functions replace most hand-written binary searches ("find first", "find last", "count", "insert position"), and they match C++'s `std::lower_bound` and `std::upper_bound`.

**2.**

{{include dsa/practice/p3_stable.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_stable p3_stable.c && ./p3_stable; rm -f p3_stable}}

A sort is **stable** if records with equal keys keep their original relative order. Insertion sort moves an item only past *strictly larger* ones, so `ana, bob, dee` stay in order. Selection sort swaps the smallest item with the one at position *i*, which can jump an item over equal ones: here the swap that brought `cy` to the front threw `ana` far to the right and finally put `dee` before `bob`. Stability matters when you sort by more than one key in passes ("sort by name, then by grade" should leave names sorted within each grade), for sorting records in user interfaces, and for radix sort, which *requires* stable passes. Merge sort and insertion sort are stable; selection sort, heapsort and typical quicksort are not (and `qsort` makes no guarantee). To make an unstable sort stable, add the original index as a tie-breaker.

**3.**

{{include dsa/practice/p3_counting.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_counting p3_counting.c && ./p3_counting; rm -f p3_counting}}

Step 1 builds a **histogram** (how many of each value); step 2 turns it into **prefix sums**, so `count[v]` becomes the number of items less than or equal to `v`, which is the position just after the last `v` in the output; step 3 places each item at `--count[value]`. Scanning the input **backwards** puts the *last* of equal items in the *last* slot, so equal keys keep their original order: the sort is stable, which is what makes counting sort usable as the building block of radix sort. The cost is O(n + k) time and space, with k the number of possible values: linear, and it beats the O(n log n) comparison-sort lower bound because it **never compares** items, it uses the key as an array index. That only works when k is small (a range of ten million values needs a ten-million-entry table).

**4.**

{{include dsa/practice/p3_inversions.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_inversions p3_inversions.c && ./p3_inversions; rm -f p3_inversions}}

During the merge, when the next item comes from the **right** half, it is smaller than *every* element still waiting in the left half (they are sorted and all larger), so it forms an inversion with each of them: add `mid - i` at once instead of counting one by one. Inversions inside the halves were counted recursively, and the merge counts the pairs that cross the halves, so every pair is counted exactly once. The total of 13 matches the brute-force double loop, which is the right way to test such code (compare a clever algorithm against an obviously correct slow one on small inputs). The cost is O(n log n). Inversions measure **how unsorted** an array is: 0 for sorted input, n(n−1)/2 for reversed input, and insertion sort's running time is exactly n plus the number of inversions, which is why it excels on nearly sorted data.

</details>

## Key takeaways

1. Write binary searches as lower/upper bound on a half-open interval; the difference counts occurrences and an absent key yields its insertion point.
2. Stability keeps equal keys in input order; insertion and merge sort are stable, selection sort and heapsort are not, and an index tie-breaker fixes any sort.
3. Counting sort is linear when the key range is small, and stays stable by filling the output from the back.
4. Counting inversions in merge sort adds the remaining left-half size whenever a right-half item is taken; test clever code against a brute-force version.
