---
title: "Self-Balancing Trees: AVL, Red-Black, Treaps and Skip Lists"
track: dsa
module: Trees
moduleOrder: 5
order: 2
level: advanced
summary: "How rotations keep search trees logarithmic — AVL and red-black trees implemented with invariant checkers, treaps and skip lists as randomized alternatives, with heights and timings measured against a plain BST and a sorted array."
sources:
  - label: Adelson-Velsky & Landis — An algorithm for the organization of information (Doklady Akademii Nauk SSSR 146, 1962)
  - label: Bayer — Symmetric Binary B-Trees, Data Structure and Maintenance Algorithms (Acta Informatica 1, 1972)
  - label: Guibas & Sedgewick — A Dichromatic Framework for Balanced Trees (FOCS 1978)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 13 (Red-Black Trees)
  - label: Aragon & Seidel — Randomized Search Trees (FOCS 1989)
  - label: Pugh — Skip Lists, a Probabilistic Alternative to Balanced Trees (CACM 33(6), 1990)
  - label: Sleator & Tarjan — Self-Adjusting Binary Search Trees (J. ACM 32(3), 1985)
  - label: Linux kernel documentation — Red-black Trees (rbtree)
    url: https://www.kernel.org/doc/html/latest/core-api/rbtree.html
---

The previous note ended with a problem: a plain binary search tree is only as good as the order its keys arrive in. A **self-balancing** tree repairs its own shape after each update, so that height — and hence every operation — stays `O(log n)` whatever the input. Two techniques cover almost all designs:

- **Deterministic invariants**, restored locally with *rotations*: AVL and red-black trees.
- **Randomization**, which makes bad shapes exponentially unlikely: treaps and skip lists.

## Rotations: the only primitive

A rotation changes which node is the local root while preserving the in-order sequence (and therefore the BST property). It touches three pointers and takes O(1):

```text
        y                         x
       / \     rotate right      / \
      x   C   ─────────────►    A   y
     / \      ◄─────────────       / \
    A   B     rotate left         B   C
```

All three subtrees `A < x < B < y < C` keep their relative order. Rotating moves one level of height from one side to the other, which is how a tree "leans" back toward balance.

## AVL trees

Adelson-Velsky and Landis (1962) gave the first self-balancing tree. **Invariant:** for every node, the heights of its two subtrees differ by at most 1. A tree satisfying it has height at most `1.44·log2(n+2)`; the worst case is a Fibonacci tree, because the minimum number of nodes `N(h)` in an AVL tree of height `h` satisfies `N(h) = N(h−1) + N(h−2) + 1`.

After an insert or delete, walk back up the path recomputing heights. At the first node whose balance becomes ±2 apply one of four cases:

| Imbalance | Child's balance | Fix |
|---|---|---|
| left-left | left child leans left (or even) | one right rotation |
| left-right | left child leans right | rotate the child left, then rotate right |
| right-right | right child leans right (or even) | one left rotation |
| right-left | right child leans left | rotate the child right, then rotate left |

Insertion needs at most one such fix (one single or double rotation), but deletion may need a rotation at *every* level on the way up, `O(log n)` rotations in the worst case.

{{include dsa/trees/avl.h}}

The recursive version rebuilds heights on the way back up, so `rebalance` is called unconditionally at every ancestor. The checker at the bottom verifies all three properties — BST order, stored heights equal real heights, and balance within ±1 — and the tests run it after **every** operation.

## Red-black trees

A red-black tree trades strictness for cheaper updates. Each node is red or black, and:

1. The root is black.
2. Every `NIL` leaf is black.
3. A red node has only black children (no two reds in a row).
4. Every path from a node down to a leaf has the same number of black nodes (the *black-height*).

Rule 4 plus rule 3 give height at most `2·log2(n+1)`: the shortest root-to-leaf path is all black (length `bh`), and the longest alternates red and black, at most twice as long. This bound is weaker than AVL's `1.44`, so red-black trees are somewhat taller on average — but **insertion needs at most 2 rotations and deletion at most 3**, with the remaining work being recolouring that can run up the tree in amortized O(1). That bounded structural change is why most library maps choose them: C++ `std::map`/`std::set` (in all mainstream implementations), Java's `TreeMap`, and the Linux kernel's `rbtree`.

Insertion colours the new node red (which never breaks rule 4) and repairs a possible red-red violation by looking at the **uncle**:

| Case | Uncle | Action |
|---|---|---|
| 1 | red | recolour parent and uncle black, grandparent red; continue from the grandparent |
| 2 | black, node is an "inner" child | rotate at the parent so it becomes the outer case |
| 3 | black, node is an "outer" child | recolour parent black / grandparent red, rotate at the grandparent — done |

Deletion is the notorious part. Removing a red node is free; removing a black node leaves a path one black short, modelled as an "extra black" on the replacement node `x`, pushed upward by four cases based on the **sibling** `w`: a red sibling is rotated into a black-sibling case; a black sibling with black children simply recolours red and moves the problem up; otherwise one or two rotations discharge it. The code follows CLRS and uses a single shared black sentinel `NIL`, so no case needs a null check.

{{include dsa/trees/rb.h}}

Note two details that bite when this is rewritten from memory: `transplant` assigns `v->p = u->p` **even when `v` is the sentinel** (the fix-up reads `x->p` afterwards), and the checker also validates parent pointers, since a rotation that forgets one produces a tree that works for lookups but corrupts later fix-ups.

## Verification

Both implementations are exercised with 100,000 random inserts, deletes and lookups against a membership table, with every invariant re-verified after each operation. Then each is fed 1000 to 1,000,000 keys in sorted order — the worst case for a plain BST — and in random order.

{{include dsa/trees/test_balanced.c}}

{{run dsa/trees :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_balanced.c -o test_balanced -lm && ./test_balanced}}

Reading the table:

- **AVL on sorted input is perfectly packed**: its height equals `⌈log2(n+1)⌉` exactly, the minimum possible for n nodes (the measured heights equal it for these sizes). Random insertion costs 1–4 extra levels, still far below the `1.44 log2 n` bound.
- **Red-black trees are looser**: on sorted input they reach about 1.85 × `log2 n`, which is the price of cheaper rebalancing. On random input they end up almost as flat as AVL.
- A plain BST would have heights of 1000, 10,000, 100,000 and 1,000,000 on the sorted rows.

## Randomized alternatives

### Treaps

A **treap** (Seidel & Aragon) stores a random priority with each key, and is simultaneously a BST on keys and a max-heap on priorities. Because the shape of a heap-ordered BST is uniquely determined by the keys and priorities, a treap with random priorities has *exactly the shape of a random BST built in priority order*: expected depth `2 ln n`, with no rotations written explicitly if you use **split** and **merge**:

- `split(t, k)` cuts a treap into keys `< k` and `≥ k`, recursing down one path.
- `merge(a, b)` joins two treaps (all keys of `a` below those of `b`) by comparing root priorities.
- insert = split, merge the new node in; delete = merge the node's children.

The code is a fraction of an AVL tree's, and split/merge give union, intersection and range deletion almost for free. The expected, not worst-case, bounds are the cost — and priorities must be independent of keys and from a decent generator.

### Skip lists

Pugh's skip list keeps a sorted linked list at level 1 and, above it, sparser "express" lists, each element promoted to the next level with probability ½. Search starts at the top level and drops down whenever the next key would overshoot — about `log2(n)/p` ≈ `2 log2 n` expected steps for `p = ½`. There are no rotations: insertion splices the node into the list at each of its levels, and the structure is easy to make lock-free or lock-sharded, which is why concurrent in-memory ordered stores favor it — Redis sorted sets, the LevelDB/RocksDB memtable, and Java's `ConcurrentSkipListMap`.

{{include dsa/trees/randomized.h}}

## Measured costs

Both randomized structures are checked against the same reference table (treap order and heap property verified every 100 operations), then all five structures plus a sorted array are timed on the same shuffled keys.

{{include dsa/trees/bench_trees.c}}

{{run dsa/trees :: gcc -std=gnu11 -O2 -Wall -Wextra bench_trees.c -o bench_trees && ./bench_trees}}

What the numbers say (absolute values depend on the machine; the ratios are what to trust):

1. **Balance pays for lookups.** At 100,000 keys and above, AVL and red-black lookups are faster than the plain BST and treap (about 1.7× at 100,000 keys in a typical run; noisier at a million) because those two trees are about twice as tall; the height column shows why.
2. **AVL vs red-black is close and workload-dependent.** AVL is shallower (slightly better lookups in theory), red-black restructures less (cheaper inserts, and the red-black insert column is lower than AVL's at every size here). At a million keys, run-to-run noise from cache and memory latency is larger than the difference between them; do not read more into one run.
3. **A sorted array beats every tree on large static data**: at a million keys it is several times faster than either balanced tree, because binary search touches contiguous memory instead of chasing pointers to unrelated heap locations. (At 10,000 keys the array row is *slower* than AVL: the whole structure is in L1 cache, and `bsearch`'s comparator callback costs more than the tree's pointer hops — a reminder to measure at your real size.) A tree is for data that changes.
4. **Skip lists search slower** than trees here: the expected comparison count is higher (`≈2 log2 n`) and every step follows a pointer. Their appeal is simplicity and concurrency, not single-thread speed.
5. **Treaps are as tall as a plain BST** (height ≈ 2.5 log2 n), so their lookups look like it; the difference is that this is true *for every input order*.

## Other designs worth knowing

- **Splay trees** (Sleator & Tarjan): no balance information; each access rotates the node to the root. Amortized O(log n), excellent for skewed access (recently used keys are cheap) but every *read* mutates the tree — unsafe for concurrent readers.
- **Weight-balanced and scapegoat trees**: rebalance by subtree sizes; scapegoat trees store nothing extra and rebuild a subtree when it grows too deep (amortized O(log n)).
- **Left-leaning red-black (Sedgewick)**: simpler code, but empirically slower than classic red-black in some benchmarks and not used by major libraries.
- **B-trees** (next-but-two note): the same ideas with nodes wide enough to fill a cache line or disk page; the practical choice for databases and, increasingly, in-memory indexes. The Linux kernel moved the virtual-memory-area tree from an rbtree to a B-tree variant (the *maple tree*) in 6.1 for exactly this reason.

### In the Linux kernel

The kernel's `rbtree` is intrusive: you embed a `struct rb_node` in your own struct and supply your own search and insert loops with `rb_link_node()` and `rb_insert_color()`. The node colour is stored in the low bit of the parent pointer, so the node is only three words. It underlies the deadline/CFS-style scheduler runqueues (keyed by virtual runtime or deadline), epoll's interest list, and many other ordered indexes. See the *Linked lists* note for the same `container_of` pattern.

## Choosing

| If you need | Use |
|---|---|
| A general ordered map in a library | Red-black tree (or a B-tree variant for cache efficiency) |
| Lookup-heavy ordered map, rarely modified | AVL, or sorted array rebuilt in bulk |
| Smallest code that is still O(log n) expected | Treap |
| Concurrent ordered map | Skip list |
| Skewed access with temporal locality | Splay tree |
| Disk or large in-memory index | B-tree / B+ tree |

## Pitfalls

- **Writing it from memory.** Deletion cases are easy to get subtly wrong; always include an invariant checker and a randomized differential test like the ones above.
- **Parent pointers and sentinels**: forgetting to update `p` during rotations or when transplanting `NIL`.
- **Recursion depth** is only O(log n) here, but the deepest recursion inside a *buggy* implementation can still blow the stack — checkers catch this early.
- **Hash keys with weak randomness for treaps**: correlated priorities rebuild the pathological shapes.
- **Duplicate keys**: ordered maps usually reject them; multimaps need a tie-breaker (for example an insertion counter) so deletes remove the right node.
- **Iterator invalidation**: tree rotations preserve node addresses, which is why `std::map` iterators survive inserts and erases of other elements — unlike vectors.

## Key takeaways

1. A rotation is an O(1) in-order-preserving restructure; every deterministic balanced tree is a policy for when to rotate.
2. AVL (height ≤ 1.44 log2 n) is the tightest; red-black (≤ 2 log2 n) rotates less on update, with at most 2 rotations per insert and 3 per delete.
3. Treaps and skip lists use randomness for expected O(log n) with much less code, and skip lists suit concurrent use.
4. Measure shape: heights from sorted input — about log2 n for AVL, ~1.85× that for red-black, n for a plain BST — show why balancing matters.
5. A sorted array still wins lookups on static data because of cache behavior.
6. Always test balanced trees with an invariant checker and a randomized differential test.
