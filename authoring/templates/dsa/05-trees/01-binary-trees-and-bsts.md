---
title: Binary Trees and Binary Search Trees
track: dsa
module: Trees
moduleOrder: 5
order: 1
level: intermediate
summary: "Traversals (recursive, explicit-stack, Morris, level order), BST insert/find/delete with the link-pointer idiom, successor, LCA, serialization, the broken-validator trap, and why shape (height) is everything — measured."
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 12 (Binary Search Trees) and Ch. 10.3 (Representing rooted trees)
  - label: Knuth — The Art of Computer Programming, Vol. 1, §2.3.1 (Traversing Binary Trees) and Vol. 3, §6.2.2 (Binary Tree Searching)
  - label: Morris — Traversing Binary Trees Simply and Cheaply (Information Processing Letters 9(5), 1979)
  - label: Devroye — A Note on the Height of Binary Search Trees (J. ACM 33(3), 1986)
  - label: Culberson — The Effect of Updates in Binary Search Trees (STOC 1985)
  - label: Hibbard — Some Combinatorial Properties of Certain Trees With Applications to Searching and Sorting (J. ACM 9(1), 1962)
---

A **binary tree** is a node with at most two children; a **binary search tree** (BST) adds one rule — every key in the left subtree is smaller than the node's key, every key in the right subtree is larger. That single invariant turns the tree into a dynamic sorted structure: search, insert, delete, predecessor, successor and range queries all take time proportional to the **height** `h`. Whether `h` is `log n` or `n` is the entire story of the next three notes.

{{include dsa/trees/bst.h}}

Two idioms are worth pausing on, because they remove the special cases that make textbook BST code long.

1. **The link pointer.** `BNode **link` points at *the pointer that references the current node* — initially `root`, later some node's `l` or `r` field. Insertion walks `link` down until `*link == NULL` and assigns the new node there; no "is the tree empty" branch, no trailing parent pointer. Deletion likewise rewrites `*link` whether the victim is the root or a deep node.
2. **Deletion has three cases.** A node with no child is unlinked. A node with one child is replaced by that child. A node with two children is replaced by its **in-order successor** (the leftmost node of its right subtree), which by construction has no left child and so is trivially detached first. Replacing with the predecessor is symmetric. (Hibbard's 1962 deletion always uses the successor; always choosing the successor is asymmetric, and Culberson and Eppinger showed that long random insert/delete sequences then degrade the expected depth to about √n — alternate between successor and predecessor, or use a balanced tree.)

## Traversals

The four orders differ in when a node is visited relative to its subtrees: **pre-order** (node, left, right — copies a tree, serializes it), **in-order** (left, node, right — yields BST keys in sorted order), **post-order** (left, right, node — frees a tree, evaluates expression trees) and **level order** (breadth-first by depth, using a queue).

Recursion uses stack depth equal to the tree height: harmless for balanced trees, a crash for a degenerate tree with a million nodes (see the recursion note). Three ways around it:

- **Explicit stack**: same O(h) memory, but on the heap, so the limit is available memory rather than the 8 MB thread stack.
- **Morris traversal**: O(1) extra memory. It uses the fact that the in-order predecessor of a node has a null right pointer; temporarily point it back at the node (a *thread*), descend, and remove the thread when you return. Every edge is traversed at most 3 times, so total time stays O(n). It mutates the tree while running, so it is unsafe with concurrent readers and must complete (or be unwound) to restore the shape.
- **Parent pointers or threaded trees** permanently, at the cost of a word per node.

{{include dsa/trees/traverse.c}}

{{run dsa/trees :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined traverse.c -o traverse && ./traverse}}

The three in-order implementations agree, level order is the breadth-first layout, and each of the three deletion cases leaves a valid BST: deleting the root with two children promotes its successor (60). The final line shows Morris leaving no stray threads behind — the sanitizers would catch a leak, and a stuck thread would make the later traversal loop forever.

## More operations without parent pointers

Successor and predecessor do not need parent pointers: descend from the root and remember the last node where you turned left (for successor) or right (for predecessor). The lowest common ancestor of `a` and `b` is the first node on the search path whose key lies between them. And a BST can be serialized as a pre-order listing with null markers, which fixes the shape uniquely. (For a BST, the pre-order key list alone is enough to rebuild the tree by re-inserting in that order, but the null markers make the format work for any binary tree.)

The listing also contains the most common wrong answer in tree interviews and real code: **validating a BST by comparing each node only with its children**. The tree where `55` hangs under `30` (whose parent is `50`) passes that check, yet `55 > 50` is in the left subtree. A node's key must lie in an open interval inherited from *all* its ancestors, which is what the second validator passes down. (An equivalent check: an in-order traversal must be strictly increasing.)

{{include dsa/trees/ops.c}}

{{run dsa/trees :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined ops.c -o ops && ./ops}}

## Shape is everything

The BST operations cost `O(h)`, and `h` depends on insertion order:

- **Sorted (or reverse sorted) input** produces a linked list: `h = n`.
- **Random insertion order** yields a "random BST" — equivalent to quicksort's recursion tree, with the first key as the pivot. Its **average node depth** is `2 ln n ≈ 1.39 log2 n` and its **height** is about `4.311 ln n` (Devroye), i.e. a small constant factor over the optimum `⌈log2(n+1)⌉`.

The experiment checks both claims against a differential test of 200,000 random insert/delete/find operations (compared with a membership table, the BST property re-verified every 1000 operations, and in-order sortedness checked at the end).

{{include dsa/trees/test_bst.c}}

{{run dsa/trees :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined test_bst.c -o test_bst -lm && ./test_bst}}

Sorted insertion gives a height equal to `n`; random insertion stays within roughly 2.3× `log2 n` here. The average depth of the 100,000-node random tree sits near the `2 ln n` prediction (the exact expectation is slightly lower, `2(1+1/n)H_n − 3` counting the root at depth 0, which is why the measurement is a little below `2 ln n`), and the height is roughly 2.4 times `log2 n`.

The practical lessons: a plain BST is fine for **randomized, insert-mostly** workloads (symbol tables over hashed keys, for instance), and wrong for anything where the order is controlled by input — timestamps, auto-increment IDs, alphabetical imports, an adversary. Also note that sorted insertion is quadratic overall: n inserts of depth up to n cost `n²/2` comparisons; building the 16,000-node degenerate tree above already needs about 128 million steps.

## BST vs alternatives

| Need | Better choice |
|---|---|
| Exact lookups only | Hash table (O(1) expected) |
| Ordered iteration, range queries, predecessor | Balanced BST, B-tree, skip list |
| Static data | Sorted array + binary search (and cache-friendly layouts) |
| Worst-case guarantees | AVL / red-black (next note) |
| Disk-resident | B-tree / B+ tree |

A pointer-based BST also has poor cache behavior — each step is a dependent load from a node that probably lives in a different cache line — so even a perfectly balanced BST is usually slower than a sorted array for lookups when the data fits in memory (compare the binary-search layouts note).

## Pitfalls

- **Unbalanced inputs.** Never feed a plain BST sorted data; shuffle first or use a self-balancing tree.
- **Recursive code on deep trees.** Height-bounded for balanced trees, but a degenerate tree can overflow the stack; use iteration, an explicit stack or Morris.
- **Local-only validation** (above) and **duplicates**: decide up front whether equal keys are rejected, counted, or stored to one side, and keep every operation consistent with that rule.
- **Integer overflow in comparisons** such as `a - b` for `int` keys; compare with `<` and `>`.
- **Freeing during traversal**: use post-order; in-order frees a node before its right subtree is read.

## Key takeaways

1. A BST's invariant is global (every ancestor constrains a key), so correct validation passes down an interval.
2. Search, insert, delete, successor and LCA cost `O(h)`; the work is entirely in controlling `h`.
3. Deletion is three cases; the indirect-pointer idiom removes the root special case.
4. Random insertion gives height ~`4.3 ln n` and average depth `2 ln n`; sorted insertion gives a list.
5. Morris traversal trades mutation for O(1) memory; explicit stacks move the O(h) usage off the call stack.
6. Pointer-chasing trees lose to arrays on cache behavior, so use trees when you need ordered *dynamic* updates.
