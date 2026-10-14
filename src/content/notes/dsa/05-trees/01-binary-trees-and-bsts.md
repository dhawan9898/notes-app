---
title: Binary Trees and Binary Search Trees
track: dsa
module: Trees
moduleOrder: 5
order: 1
level: intermediate
summary: "Traversals (recursive, explicit-stack, Morris, level order), BST insert/find/delete with the link-pointer idiom, successor, LCA, serialization, the broken-validator trap, and why shape (height) is everything — measured."
prerequisites:
  - Recursion, Divide and Conquer, and Loop Invariants
  - Linked Lists
  - "Trees and Graphs for Beginners"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 12 (Binary Search Trees) and Ch. 10.3 (Representing rooted trees)"
  - label: "Knuth — The Art of Computer Programming, Vol. 1, §2.3.1 (Traversing Binary Trees) and Vol. 3, §6.2.2 (Binary Tree Searching)"
  - label: "Morris — Traversing Binary Trees Simply and Cheaply (Information Processing Letters 9(5), 1979)"
  - label: "Devroye — A Note on the Height of Binary Search Trees (J. ACM 33(3), 1986)"
  - label: "Culberson — The Effect of Updates in Binary Search Trees (STOC 1985)"
  - label: "Hibbard — Some Combinatorial Properties of Certain Trees With Applications to Searching and Sorting (J. ACM 9(1), 1962)"
---

A **binary tree** is a node with at most two children; a **binary search tree** (BST) adds one rule — every key in the left subtree is smaller than the node's key, every key in the right subtree is larger. That single invariant turns the tree into a dynamic sorted structure: search, insert, delete, predecessor, successor and range queries all take time proportional to the **height** `h`. Whether `h` is `log n` or `n` is the entire story of the next three notes.

```c
#ifndef BST_H
#define BST_H
#include <stdint.h>
#include <stdlib.h>

typedef struct BNode { int key; struct BNode *l, *r; } BNode;

static inline BNode *bst_new(int key) { BNode *n = malloc(sizeof *n); n->key = key; n->l = n->r = NULL; return n; }

/* Iterative insert: returns 1 if inserted, 0 if the key was already present. */
static inline int bst_insert(BNode **root, int key) {
    BNode **link = root;                         /* pointer to the link we may overwrite */
    while (*link) {
        if (key == (*link)->key) return 0;
        link = key < (*link)->key ? &(*link)->l : &(*link)->r;
    }
    *link = bst_new(key);
    return 1;
}
static inline BNode *bst_find(BNode *t, int key) {
    while (t && t->key != key) t = key < t->key ? t->l : t->r;
    return t;
}
/* Delete using the link-pointer idiom: no special case for "node is the root". */
static inline int bst_delete(BNode **root, int key) {
    BNode **link = root;
    while (*link && (*link)->key != key) link = key < (*link)->key ? &(*link)->l : &(*link)->r;
    BNode *n = *link;
    if (!n) return 0;
    if (!n->l)      *link = n->r;                /* case 1 and 2: zero or one child -> splice out */
    else if (!n->r) *link = n->l;
    else {                                       /* case 3: two children -> replace with in-order successor */
        BNode **sl = &n->r;
        while ((*sl)->l) sl = &(*sl)->l;
        BNode *s = *sl;
        *sl = s->r;                              /* detach successor (it has no left child) */
        s->l = n->l; s->r = n->r; *link = s;
    }
    free(n);
    return 1;
}
static inline void bst_free(BNode *t) { if (!t) return; bst_free(t->l); bst_free(t->r); free(t); }
static inline int bst_height(const BNode *t) {          /* recursive: depth == height, fine for balanced trees */
    if (!t) return 0;
    int a = bst_height(t->l), b = bst_height(t->r);
    return 1 + (a > b ? a : b);
}
#endif
```

Two idioms are worth pausing on, because they remove the special cases that make textbook BST code long.

1. **The link pointer.** `BNode **link` points at *the pointer that references the current node* — initially `root`, later some node's `l` or `r` field. Insertion walks `link` down until `*link == NULL` and assigns the new node there; no "is the tree empty" branch, no trailing parent pointer. Deletion likewise rewrites `*link` whether the victim is the root or a deep node.
2. **Deletion has three cases.** A node with no child is unlinked. A node with one child is replaced by that child. A node with two children is replaced by its **in-order successor** (the leftmost node of its right subtree), which by construction has no left child and so is trivially detached first. Replacing with the predecessor is symmetric. (Hibbard's 1962 deletion always uses the successor; always choosing the successor is asymmetric, and Culberson and Eppinger showed that long random insert/delete sequences then degrade the expected depth to about √n — alternate between successor and predecessor, or use a balanced tree.)

## Traversals

The four orders differ in when a node is visited relative to its subtrees: **pre-order** (node, left, right — copies a tree, serializes it), **in-order** (left, node, right — yields BST keys in sorted order), **post-order** (left, right, node — frees a tree, evaluates expression trees) and **level order** (breadth-first by depth, using a queue).

Recursion uses stack depth equal to the tree height: harmless for balanced trees, a crash for a degenerate tree with a million nodes (see the recursion note). Three ways around it:

- **Explicit stack**: same O(h) memory, but on the heap, so the limit is available memory rather than the 8 MB thread stack.
- **Morris traversal**: O(1) extra memory. It uses the fact that the in-order predecessor of a node has a null right pointer; temporarily point it back at the node (a *thread*), descend, and remove the thread when you return. Every edge is traversed at most 3 times, so total time stays O(n). It mutates the tree while running, so it is unsafe with concurrent readers and must complete (or be unwound) to restore the shape.
- **Parent pointers or threaded trees** permanently, at the cost of a word per node.

```c
#include <stdio.h>
#include "bst.h"

/* ---- 1. recursive in-order: uses O(h) call stack ---- */
static void inorder_rec(const BNode *t, void (*visit)(int)) { if (!t) return; inorder_rec(t->l, visit); visit(t->key); inorder_rec(t->r, visit); }

/* ---- 2. iterative with an explicit stack: still O(h) memory, but the heap instead of the call stack ---- */
static void inorder_stack(const BNode *t, void (*visit)(int)) {
    const BNode **st = malloc(100000 * sizeof *st); int sp = 0;
    while (t || sp) {
        while (t) { st[sp++] = t; t = t->l; }
        t = st[--sp]; visit(t->key); t = t->r;
    }
    free(st);
}

/* ---- 3. Morris traversal: O(1) extra memory by temporarily threading the tree ---- */
static void inorder_morris(BNode *t, void (*visit)(int)) {
    while (t) {
        if (!t->l) { visit(t->key); t = t->r; continue; }
        BNode *p = t->l;
        while (p->r && p->r != t) p = p->r;      /* p = in-order predecessor of t */
        if (!p->r) { p->r = t; t = t->l; }       /* first visit: thread predecessor -> t, go left */
        else       { p->r = NULL; visit(t->key); t = t->r; }   /* second visit: remove thread, visit, go right */
    }
}

/* ---- 4. level order with a queue ---- */
static void level_order(const BNode *root, void (*visit)(int)) {
    const BNode *q[1024]; int h = 0, t = 0;
    if (root) q[t++] = root;
    while (h < t) { const BNode *n = q[h++]; visit(n->key); if (n->l) q[t++] = n->l; if (n->r) q[t++] = n->r; }
}

static void pr(int k) { printf("%d ", k); }

int main(void) {
    BNode *root = NULL;
    int keys[] = { 50, 30, 70, 20, 40, 60, 80, 35, 45, 65 };
    for (int i = 0; i < 10; i++) bst_insert(&root, keys[i]);
    printf("in-order (recursive): "); inorder_rec(root, pr); printf("\n");
    printf("in-order (stack)    : "); inorder_stack(root, pr); printf("\n");
    printf("in-order (Morris)   : "); inorder_morris(root, pr); printf("\n");
    printf("level order         : "); level_order(root, pr); printf("\n");
    printf("height = %d\n\n", bst_height(root));

    /* the three deletion cases */
    bst_delete(&root, 20); printf("delete leaf 20       -> "); inorder_rec(root, pr); printf("\n");
    bst_delete(&root, 30); printf("delete 30 (1 child:40)-> "); level_order(root, pr); printf("\n");
    bst_delete(&root, 50); printf("delete root 50 (2 ch.) -> "); level_order(root, pr); printf("  (successor 60 became the root)\n");
    printf("Morris restored the tree (no threads left): "); inorder_morris(root, pr); printf("\n");
    bst_free(root);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined traverse.c -o traverse && ./traverse
in-order (recursive): 20 30 35 40 45 50 60 65 70 80 
in-order (stack)    : 20 30 35 40 45 50 60 65 70 80 
in-order (Morris)   : 20 30 35 40 45 50 60 65 70 80 
level order         : 50 30 70 20 40 60 80 35 45 65 
height = 4

delete leaf 20       -> 30 35 40 45 50 60 65 70 80 
delete 30 (1 child:40)-> 50 40 70 35 45 60 80 65 
delete root 50 (2 ch.) -> 60 40 70 35 45 65 80   (successor 60 became the root)
Morris restored the tree (no threads left): 35 40 45 60 65 70 80 
```

The three in-order implementations agree, level order is the breadth-first layout, and each of the three deletion cases leaves a valid BST: deleting the root with two children promotes its successor (60). The final line shows Morris leaving no stray threads behind — the sanitizers would catch a leak, and a stuck thread would make the later traversal loop forever.

## More operations without parent pointers

Successor and predecessor do not need parent pointers: descend from the root and remember the last node where you turned left (for successor) or right (for predecessor). The lowest common ancestor of `a` and `b` is the first node on the search path whose key lies between them. And a BST can be serialized as a pre-order listing with null markers, which fixes the shape uniquely. (For a BST, the pre-order key list alone is enough to rebuild the tree by re-inserting in that order, but the null markers make the format work for any binary tree.)

The listing also contains the most common wrong answer in tree interviews and real code: **validating a BST by comparing each node only with its children**. The tree where `55` hangs under `30` (whose parent is `50`) passes that check, yet `55 > 50` is in the left subtree. A node's key must lie in an open interval inherited from *all* its ancestors, which is what the second validator passes down. (An equivalent check: an in-order traversal must be strictly increasing.)

```c
#include <stdio.h>
#include <string.h>
#include "bst.h"

/* Successor / predecessor of `key` WITHOUT parent pointers: remember the last node where we went left/right. */
static const BNode *successor(const BNode *t, int key) {
    const BNode *best = NULL;
    while (t) { if (key < t->key) { best = t; t = t->l; } else t = t->r; }
    return best;
}
static const BNode *predecessor(const BNode *t, int key) {
    const BNode *best = NULL;
    while (t) { if (key > t->key) { best = t; t = t->r; } else t = t->l; }
    return best;
}
/* Lowest common ancestor in a BST: the first node whose key lies between a and b. */
static const BNode *lca(const BNode *t, int a, int b) {
    while (t) {
        if (a < t->key && b < t->key) t = t->l;
        else if (a > t->key && b > t->key) t = t->r;
        else return t;
    }
    return NULL;
}
/* Serialize in pre-order with '#' for null; the pre-order + null markers identify the shape uniquely. */
static void serialize(const BNode *t, char **p) {
    if (!t) { *p += sprintf(*p, "# "); return; }
    *p += sprintf(*p, "%d ", t->key); serialize(t->l, p); serialize(t->r, p);
}
static BNode *deserialize(char **p) {
    if (**p == '#') { *p += 2; return NULL; }
    int k, used; sscanf(*p, "%d %n", &k, &used); *p += used;
    BNode *n = bst_new(k); n->l = deserialize(p); n->r = deserialize(p); return n;
}
/* THE classic wrong validator: compares each node only with its children. */
static int valid_wrong(const BNode *t) {
    if (!t) return 1;
    if (t->l && t->l->key >= t->key) return 0;
    if (t->r && t->r->key <= t->key) return 0;
    return valid_wrong(t->l) && valid_wrong(t->r);
}
static int valid_right(const BNode *t, long lo, long hi) {
    if (!t) return 1;
    return t->key > lo && t->key < hi && valid_right(t->l, lo, t->key) && valid_right(t->r, t->key, hi);
}

int main(void) {
    BNode *root = NULL; int keys[] = { 50, 30, 70, 20, 40, 60, 80, 35, 45, 65 };
    for (int i = 0; i < 10; i++) bst_insert(&root, keys[i]);
    const BNode *s = successor(root, 45), *p = predecessor(root, 45);
    printf("successor(45) = %d, predecessor(45) = %d, successor(80) = %s\n", s->key, p->key, successor(root, 80) ? "?" : "none");
    printf("lca(35, 45) = %d, lca(20, 65) = %d, lca(60, 80) = %d\n", lca(root, 35, 45)->key, lca(root, 20, 65)->key, lca(root, 60, 80)->key);

    char buf[256], *w = buf; serialize(root, &w);
    printf("serialized: %s\n", buf);
    char *r = buf; BNode *copy = deserialize(&r);
    char buf2[256]; w = buf2; serialize(copy, &w);
    printf("round trip identical: %s\n", strcmp(buf, buf2) == 0 ? "yes" : "NO");

    /* a tree that passes the local check but is not a BST: 55 sits in the left subtree of 50 */
    BNode *bad = bst_new(50); bad->l = bst_new(30); bad->l->r = bst_new(55); bad->r = bst_new(70);
    printf("\nbroken tree (55 is in the left subtree of 50):\n  local-compare validator says %s\n  range validator says         %s\n",
           valid_wrong(bad) ? "VALID (wrong)" : "invalid", valid_right(bad, -(1L << 40), 1L << 40) ? "valid" : "INVALID (correct)");
    bst_free(root); bst_free(copy); bst_free(bad);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined ops.c -o ops && ./ops
successor(45) = 50, predecessor(45) = 40, successor(80) = none
lca(35, 45) = 40, lca(20, 65) = 50, lca(60, 80) = 70
serialized: 50 30 20 # # 40 35 # # 45 # # 70 60 # 65 # # 80 # # 
round trip identical: yes

broken tree (55 is in the left subtree of 50):
  local-compare validator says VALID (wrong)
  range validator says         INVALID (correct)
```

## Shape is everything

The BST operations cost `O(h)`, and `h` depends on insertion order:

- **Sorted (or reverse sorted) input** produces a linked list: `h = n`.
- **Random insertion order** yields a "random BST" — equivalent to quicksort's recursion tree, with the first key as the pivot. Its **average node depth** is `2 ln n ≈ 1.39 log2 n` and its **height** is about `4.311 ln n` (Devroye), i.e. a small constant factor over the optimum `⌈log2(n+1)⌉`.

The experiment checks both claims against a differential test of 200,000 random insert/delete/find operations (compared with a membership table, the BST property re-verified every 1000 operations, and in-order sortedness checked at the end).

```c
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bst.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

static int pos;
static int *out;
static void walk(const BNode *t) { if (!t) return; walk(t->l); out[pos++] = t->key; walk(t->r); }
static int check_bst(const BNode *t, long lo, long hi) {   /* every key strictly inside (lo, hi) */
    if (!t) return 1;
    if (t->key <= lo || t->key >= hi) return 0;
    return check_bst(t->l, lo, t->key) && check_bst(t->r, t->key, hi);
}

int main(void) {
    /* differential test: random insert/delete/find against a boolean membership table */
    enum { RANGE = 2000, OPS = 200000 };
    static char present[RANGE]; BNode *root = NULL; int count = 0;
    for (int i = 0; i < OPS; i++) {
        int k = (int)(rnd() % RANGE), op = (int)(rnd() % 3);
        if (op == 0)      { int ins = bst_insert(&root, k); assert(ins == !present[k]); if (ins) count++; present[k] = 1; }
        else if (op == 1) { int del = bst_delete(&root, k); assert(del == present[k]);  if (del) count--; present[k] = 0; }
        else              { assert((bst_find(root, k) != NULL) == present[k]); }
        if (i % 1000 == 0) assert(check_bst(root, -1, RANGE));
    }
    out = malloc(RANGE * sizeof *out); pos = 0; walk(root);
    assert(pos == count); for (int i = 1; i < pos; i++) assert(out[i - 1] < out[i]);
    printf("differential test passed: %d ops, final size %d, BST property and in-order sortedness verified\n", OPS, count);
    bst_free(root); free(out);

    /* shape: height vs n for random order, sorted order and shuffled-then-half-deleted */
    printf("\n%8s %14s %14s %14s\n", "n", "random height", "sorted height", "log2(n)+1");
    for (int n = 1000; n <= 20000; n *= 4) {
        int *a = malloc(n * sizeof *a); for (int i = 0; i < n; i++) a[i] = i;
        BNode *sorted = NULL; for (int i = 0; i < n; i++) bst_insert(&sorted, a[i]);
        for (int i = n - 1; i > 0; i--) { int j = (int)(rnd() % (i + 1)), t = a[i]; a[i] = a[j]; a[j] = t; }
        BNode *rand_ = NULL; for (int i = 0; i < n; i++) bst_insert(&rand_, a[i]);
        printf("%8d %14d %14d %14.1f\n", n, bst_height(rand_), bst_height(sorted), log2(n) + 1);
        bst_free(rand_); bst_free(sorted); free(a);
    }

    /* average depth of a node in a random BST ~ 2 ln n = 1.39 log2 n */
    int n = 100000; BNode *t = NULL; for (int i = 0; i < n; i++) bst_insert(&t, (int)rnd());
    long total = 0; long nodes = 0;
    BNode *stack[200]; int depth[200]; int sp = 0; stack[sp] = t; depth[sp++] = 1;
    while (sp) { BNode *x = stack[--sp]; int d = depth[sp]; total += d; nodes++; if (x->l) { stack[sp] = x->l; depth[sp++] = d + 1; } if (x->r) { stack[sp] = x->r; depth[sp++] = d + 1; } }
    printf("\nrandom BST, %ld nodes: average depth %.2f, 2*ln(n) = %.2f, log2(n) = %.2f, height %d\n", nodes, (double)total / (double)nodes, 2 * log((double)nodes), log2((double)nodes), bst_height(t));
    bst_free(t);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined test_bst.c -o test_bst -lm && ./test_bst
differential test passed: 200000 ops, final size 1023, BST property and in-order sortedness verified

       n  random height  sorted height      log2(n)+1
    1000             22           1000           11.0
    4000             27           4000           13.0
   16000             32          16000           15.0

random BST, 99997 nodes: average depth 21.32, 2*ln(n) = 23.03, log2(n) = 16.61, height 39
```

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
