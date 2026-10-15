---
kind: code
title: Lower bound and upper bound
module: Searching and Sorting
difficulty: Medium
tags:
  - binary search
  - duplicates
starter: dsa/starters/sorting/ex1-bounds.c
examples:
  - input: a = {1, 3, 3, 3, 5, 8, 8, 13}, x = 3
    output: lower 1, upper 4
  - input: x = 4
    output: lower 4, upper 4 (absent)
  - input: x = 20
    output: lower 8, upper 8
    why: Both return n when every element is smaller.
constraints:
  - '`0 <= n`; `a` is sorted ascending.'
  - O(log n) time.
hints:
  - Search the half-open range `[lo, hi)`; the answer is where the predicate flips from false to true.
  - The two functions differ only in `<` versus `<=` in one comparison.
---

In a sorted array that may contain duplicates, `lower_bound(a, n, x)` is the first index `i` with `a[i] >= x` and `upper_bound(a, n, x)` is the first index with `a[i] > x`. Both return `n` if there is no such index. The count of `x` is their difference.

## Explanation

Both functions use the half-open interval `[lo, hi)` and the invariant "everything left of `lo` is less than (or, for `upper_bound`, not greater than) `x`; everything from `hi` on is not". The loop is `while (lo < hi)` with `hi = mid` in one branch and `lo = mid + 1` in the other, so the interval shrinks every time and the loop always terminates. The answer is where `lo` and `hi` meet. `upper_bound − lower_bound` is the number of occurrences: 3 for the value 3, 2 for 8, and 0 for the absent 4. For an absent value, `lower_bound` is the **insertion point** that keeps the array sorted (4 would go at index 4; 20 at index 8 = n, one past the end). These two functions replace most hand-written binary searches ("find first", "find last", "count", "insert position"), and they match C++'s `std::lower_bound` and `std::upper_bound`.
