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
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 2 (Getting Started: insertion sort) and §2.3
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §1.1 (binary search) and §2.1 (elementary sorts)
  - label: Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)
  - label: ISO/IEC 9899:2018 §7.22.5.2 (The qsort function)
---

Almost every program searches for something or puts things in order. This note is the first, gentle look at both. The algorithms are short enough to read in one sitting, and the programs *count* how much work each does, so you see growth with your own eyes rather than taking Big-O on faith. The full treatment comes later in *Binary Search and Its Variants* and *Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort*.

## Searching: look at everything, or halve the problem

**Linear search** checks the items one by one until it finds the target. It needs no preparation and works on any array, but the worst case examines every item.

**Binary search** requires the array to be **sorted**. Look at the middle item: if it is the target you are done; if the target is bigger, the left half cannot contain it, so throw that half away; otherwise throw the right half away. Repeat on what is left. Each step halves the remaining items, so 1,000 items need at most about 10 steps and a million need about 20.

{{include dsa/essentials/search.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -o search search.c && ./search; rm -f search}}

Read the numbers: finding the last item takes the linear search 1,000 steps and binary search 10. Even for an item that is *absent*, binary search needs only 10 steps. The price is that the data must already be sorted, and sorting costs time too. That trade-off, preparing data once so that many later questions are cheap, appears in nearly every data structure.

Two details in the code are worth copying. `mid = lo + (hi - lo) / 2` instead of `(lo + hi) / 2` avoids integer overflow when the array is huge. The loop condition `lo <= hi` and the updates `mid + 1` and `mid - 1` are the part beginners get wrong most often: if either update were `mid`, some inputs would loop forever.

## Sorting by hand: two simple algorithms

Two simple sorts show the main idea, each in about ten lines.

- **Selection sort**: find the smallest remaining item and swap it into the next position. Whatever the input, it scans the entire unsorted part each time.
- **Insertion sort**: keep a sorted prefix at the front; take the next item and slide it left past every larger item until it fits, the way most people sort playing cards in their hand.

{{include dsa/essentials/sort_basic.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -o sort_basic sort_basic.c && ./sort_basic; rm -f sort_basic}}

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
