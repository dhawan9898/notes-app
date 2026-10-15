---
title: "Practice: Trees, Heaps and Tries"
track: dsa
module: Trees
moduleOrder: 5
order: 9
level: advanced
summary: "Four exercises on the trees module: validating a binary search tree correctly, finding the k-th smallest key and checking balance, selecting the k largest items with a size-k min-heap, and counting words by prefix with a trie, each with a verified C answer."
prerequisites:
  - "Binary Trees and Binary Search Trees"
  - "Heaps and Priority Queues"
  - "Tries, Radix Trees and Longest-Prefix Match"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 6 (Heapsort), Ch. 12 (Binary Search Trees)
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §2.4 (Priority Queues), §3.2, §5.2 (Tries)
  - label: Knuth — The Art of Computer Programming, Vol. 3, §6.3 (Digital Searching)
---

Tree problems reward getting the *invariant* right. For each exercise, state the invariant in one sentence before writing code, and test the nasty case (the trap tree in exercise 1, the empty prefix, a chain).

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Validate a BST.** Write `valid(root)`. A common wrong answer checks only that each node is greater than its left child and smaller than its right child. Build a tree that passes that check but is *not* a BST, and explain the correct invariant.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/trees/ex1-valid-bst.c}}

</details>

**2. k-th smallest and balance.** Find the k-th smallest key without copying the tree into an array (stop early). Then write a function that reports whether a tree is height-balanced (subtree heights differ by at most 1 at **every** node) in one pass.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/trees/ex2-kth-balanced.c}}

</details>

**3. Top-k with a heap.** From a stream of *n* integers, report the *k* largest using O(k) memory. Which kind of heap (min or max) do you need, and why? State the time complexity.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/trees/ex3-top-k.c}}

</details>

**4. Prefix counting.** Build a trie over the words `car, card, care, cart, cat, dog, do` and answer "how many words start with this prefix?" in time proportional to the prefix length. Why must a trie node also record whether a word *ends* there?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/trees/ex4-trie.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1 and 2.**

{{include dsa/practice/p5_bst.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_bst p5_bst.c && ./p5_bst; rm -f p5_bst}}

*Validation.* The invariant is about **ranges**, not neighbours: every key in a node's left subtree must be smaller than the node *and* larger than everything above that bound. The trap tree has `55` as the right child of `30` (55 > 30, so the parent-only check passes) but it sits in the left subtree of `50`, so it is too large. The correct check passes an open interval `(lo, hi)` down the recursion: going left sets `hi = node->key`, going right sets `lo = node->key`. (Use a wider type than the keys for the initial bounds, as the code does with `long`, or the extreme key `INT_MIN` would be wrongly rejected.) An alternative proof: an in-order traversal of a valid BST is strictly increasing.

*k-th smallest.* An in-order walk visits keys in sorted order, so count down `k` and stop when it reaches zero: O(height + k) time, O(height) stack. Augmenting each node with its subtree size gives O(height) lookup by rank.

*Balance.* The function returns the height, or `-1` as an "unbalanced" signal that propagates up, so each node is visited once (O(n)). Computing heights separately at every node would be O(n²) on a chain. The chain `1-2-3` is correctly reported unbalanced.

**3.**

{{include dsa/practice/p5_heap.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_heap p5_heap.c && ./p5_heap; rm -f p5_heap}}

You need a **min-heap of size k**, which seems backwards but is the point: its root is the *smallest of the current top k*, i.e. the item most likely to be displaced. A new value only matters if it beats that root; then the root is popped and the new value pushed. Each of the *n* items costs at most one pop and one push, O(log k), so the total is **O(n log k)** time and **O(k)** space, and it works on a stream you cannot store. Sorting everything would take O(n log n) time and O(n) memory. (For one-off selection from an array in memory, quickselect is O(n) on average.)

**4.**

{{include dsa/practice/p5_trie.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_trie p5_trie.c && ./p5_trie; rm -f p5_trie}}

Each node counts how many inserted words pass through it (`words_through`), so after walking the prefix in O(length of prefix), that node's counter *is* the answer: 4 words start with `car`, 5 with `ca`. The separate `is_word` flag is needed because a prefix is not necessarily a word: `ca` is on the path of five words but is not itself one (`contains("ca")` is 0), whereas `car` is both a word and a prefix of three others. The cost is memory: this layout uses 26 pointers per node; compressed (radix) tries and hash- or array-mapped children reduce it.

</details>

## Key takeaways

1. A BST's invariant spans whole subtrees (a range check), not just parent-child pairs; an in-order traversal must be strictly increasing.
2. Return a sentinel (such as `-1`) from a recursive height function to check balance in one pass instead of O(n²).
3. Top-k selection uses a min-heap of size k: O(n log k) time and O(k) memory, and it works on streams.
4. Tries answer prefix questions in O(prefix length); store a count per node for prefix totals and a separate end-of-word flag.
