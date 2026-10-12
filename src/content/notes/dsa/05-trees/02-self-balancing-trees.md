---
title: "Self-Balancing Trees: AVL, Red-Black, Treaps and Skip Lists"
track: dsa
module: Trees
moduleOrder: 5
order: 2
level: advanced
summary: "How rotations keep search trees logarithmic — AVL and red-black trees implemented with invariant checkers, treaps and skip lists as randomized alternatives, with heights and timings measured against a plain BST and a sorted array."
sources:
  - label: "Adelson-Velsky & Landis — An algorithm for the organization of information (Doklady Akademii Nauk SSSR 146, 1962)"
  - label: "Bayer — Symmetric Binary B-Trees, Data Structure and Maintenance Algorithms (Acta Informatica 1, 1972)"
  - label: "Guibas & Sedgewick — A Dichromatic Framework for Balanced Trees (FOCS 1978)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 13 (Red-Black Trees)"
  - label: "Aragon & Seidel — Randomized Search Trees (FOCS 1989)"
  - label: "Pugh — Skip Lists, a Probabilistic Alternative to Balanced Trees (CACM 33(6), 1990)"
  - label: "Sleator & Tarjan — Self-Adjusting Binary Search Trees (J. ACM 32(3), 1985)"
  - label: "Linux kernel documentation — Red-black Trees (rbtree)"
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

```c
#ifndef AVL_H
#define AVL_H
#include <stdlib.h>

typedef struct AVL { int key, h; struct AVL *l, *r; } AVL;   /* h = height of the subtree rooted here (leaf = 1) */

static inline int ht(const AVL *n) { return n ? n->h : 0; }
static inline void upd(AVL *n) { int a = ht(n->l), b = ht(n->r); n->h = 1 + (a > b ? a : b); }
static inline int bal(const AVL *n) { return ht(n->l) - ht(n->r); }

static AVL *rot_right(AVL *y) { AVL *x = y->l; y->l = x->r; x->r = y; upd(y); upd(x); return x; }
static AVL *rot_left(AVL *x)  { AVL *y = x->r; x->r = y->l; y->l = x; upd(x); upd(y); return y; }

/* Restore |balance| <= 1 at n, assuming both subtrees are already AVL and differ by at most 2. */
static AVL *rebalance(AVL *n) {
    upd(n);
    int b = bal(n);
    if (b > 1) {                                   /* left-heavy */
        if (bal(n->l) < 0) n->l = rot_left(n->l);  /* left-right case: first make it left-left */
        return rot_right(n);
    }
    if (b < -1) {                                  /* right-heavy */
        if (bal(n->r) > 0) n->r = rot_right(n->r);
        return rot_left(n);
    }
    return n;
}
static AVL *avl_insert(AVL *n, int key, int *inserted) {
    if (!n) { AVL *x = malloc(sizeof *x); x->key = key; x->h = 1; x->l = x->r = NULL; *inserted = 1; return x; }
    if (key < n->key) n->l = avl_insert(n->l, key, inserted);
    else if (key > n->key) n->r = avl_insert(n->r, key, inserted);
    else return n;
    return rebalance(n);
}
static inline AVL *avl_min(AVL *n) { while (n->l) n = n->l; return n; }
static inline AVL *avl_delete(AVL *n, int key, int *deleted) {
    if (!n) return NULL;
    if (key < n->key) n->l = avl_delete(n->l, key, deleted);
    else if (key > n->key) n->r = avl_delete(n->r, key, deleted);
    else {
        *deleted = 1;
        if (!n->l || !n->r) { AVL *c = n->l ? n->l : n->r; free(n); return c; }
        AVL *s = avl_min(n->r);                    /* two children: copy the successor's key, delete it below */
        n->key = s->key;
        int dummy = 0; n->r = avl_delete(n->r, s->key, &dummy);
    }
    return rebalance(n);
}
static void avl_free(AVL *n) { if (!n) return; avl_free(n->l); avl_free(n->r); free(n); }
static inline const AVL *avl_find(const AVL *n, int key) { while (n && n->key != key) n = key < n->key ? n->l : n->r; return n; }

/* Invariant checker: BST order, stored heights correct, |balance| <= 1. Returns the height or -1 on violation. */
static inline int avl_check(const AVL *n, long lo, long hi) {
    if (!n) return 0;
    if (n->key <= lo || n->key >= hi) return -1;
    int a = avl_check(n->l, lo, n->key), b = avl_check(n->r, n->key, hi);
    if (a < 0 || b < 0 || a - b > 1 || b - a > 1) return -1;
    int h = 1 + (a > b ? a : b);
    return h == n->h ? h : -1;
}
#endif
```

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

```c
#ifndef RB_H
#define RB_H
#include <stdlib.h>

/* Red-black tree after CLRS 4th ed. Ch.13, with one shared black sentinel NIL (so rotations and fix-ups never test for NULL). */
typedef struct RB { int key; char red; struct RB *l, *r, *p; } RB;
typedef struct { RB *root; RB nil; } RBT;

static void rb_init(RBT *t) { t->nil = (RB){ 0, 0, &t->nil, &t->nil, &t->nil }; t->root = &t->nil; }

static void rotl(RBT *t, RB *x) {
    RB *y = x->r; x->r = y->l;
    if (y->l != &t->nil) y->l->p = x;
    y->p = x->p;
    if (x->p == &t->nil) t->root = y; else if (x == x->p->l) x->p->l = y; else x->p->r = y;
    y->l = x; x->p = y;
}
static void rotr(RBT *t, RB *x) {
    RB *y = x->l; x->l = y->r;
    if (y->r != &t->nil) y->r->p = x;
    y->p = x->p;
    if (x->p == &t->nil) t->root = y; else if (x == x->p->r) x->p->r = y; else x->p->l = y;
    y->r = x; x->p = y;
}
static int rb_insert(RBT *t, int key) {
    RB *y = &t->nil, *x = t->root;
    while (x != &t->nil) { y = x; if (key == x->key) return 0; x = key < x->key ? x->l : x->r; }
    RB *z = malloc(sizeof *z); *z = (RB){ key, 1, &t->nil, &t->nil, y };       /* new nodes are red */
    if (y == &t->nil) t->root = z; else if (key < y->key) y->l = z; else y->r = z;
    while (z->p->red) {                                  /* red parent => red-red violation */
        RB *g = z->p->p;
        if (z->p == g->l) {
            RB *u = g->r;                                /* uncle */
            if (u->red) { z->p->red = 0; u->red = 0; g->red = 1; z = g; }       /* case 1: recolor, push violation up */
            else {
                if (z == z->p->r) { z = z->p; rotl(t, z); }                     /* case 2: inner child -> rotate to outer */
                z->p->red = 0; g->red = 1; rotr(t, g);                          /* case 3: recolor + rotate grandparent */
            }
        } else {
            RB *u = g->l;
            if (u->red) { z->p->red = 0; u->red = 0; g->red = 1; z = g; }
            else {
                if (z == z->p->l) { z = z->p; rotr(t, z); }
                z->p->red = 0; g->red = 1; rotl(t, g);
            }
        }
    }
    t->root->red = 0;
    return 1;
}
static void transplant(RBT *t, RB *u, RB *v) {
    if (u->p == &t->nil) t->root = v; else if (u == u->p->l) u->p->l = v; else u->p->r = v;
    v->p = u->p;                                         /* deliberately also when v == NIL: the fix-up relies on it */
}
static RB *rb_min(RBT *t, RB *x) { while (x->l != &t->nil) x = x->l; return x; }
static void delete_fixup(RBT *t, RB *x) {
    while (x != t->root && !x->red) {                    /* x carries an extra "doubly black" */
        if (x == x->p->l) {
            RB *w = x->p->r;                             /* sibling */
            if (w->red) { w->red = 0; x->p->red = 1; rotl(t, x->p); w = x->p->r; }              /* case 1: red sibling */
            if (!w->l->red && !w->r->red) { w->red = 1; x = x->p; }                             /* case 2: black sibling, black nephews */
            else {
                if (!w->r->red) { w->l->red = 0; w->red = 1; rotr(t, w); w = x->p->r; }         /* case 3: far nephew black */
                w->red = x->p->red; x->p->red = 0; w->r->red = 0; rotl(t, x->p); x = t->root;   /* case 4: far nephew red */
            }
        } else {
            RB *w = x->p->l;
            if (w->red) { w->red = 0; x->p->red = 1; rotr(t, x->p); w = x->p->l; }
            if (!w->r->red && !w->l->red) { w->red = 1; x = x->p; }
            else {
                if (!w->l->red) { w->r->red = 0; w->red = 1; rotl(t, w); w = x->p->l; }
                w->red = x->p->red; x->p->red = 0; w->l->red = 0; rotr(t, x->p); x = t->root;
            }
        }
    }
    x->red = 0;
}
static inline int rb_delete(RBT *t, int key) {
    RB *z = t->root;
    while (z != &t->nil && z->key != key) z = key < z->key ? z->l : z->r;
    if (z == &t->nil) return 0;
    RB *y = z, *x; char y_was_red = y->red;
    if (z->l == &t->nil) { x = z->r; transplant(t, z, z->r); }
    else if (z->r == &t->nil) { x = z->l; transplant(t, z, z->l); }
    else {
        y = rb_min(t, z->r); y_was_red = y->red; x = y->r;
        if (y->p == z) x->p = y; else { transplant(t, y, y->r); y->r = z->r; y->r->p = y; }
        transplant(t, z, y); y->l = z->l; y->l->p = y; y->red = z->red;
    }
    free(z);
    if (!y_was_red) delete_fixup(t, x);                  /* removing a black node breaks the black-height rule */
    return 1;
}
static void rb_free_rec(RBT *t, RB *n) { if (n == &t->nil) return; rb_free_rec(t, n->l); rb_free_rec(t, n->r); free(n); }
static void rb_free(RBT *t) { rb_free_rec(t, t->root); t->root = &t->nil; }
static inline int rb_find(const RBT *t, int key) { const RB *x = t->root; while (x != &t->nil && x->key != key) x = key < x->key ? x->l : x->r; return x != &t->nil; }

/* Checker for the five red-black properties + BST order + parent pointers. Returns black-height or -1. */
static int rb_check_rec(const RBT *t, const RB *n, long lo, long hi) {
    if (n == &t->nil) return 1;
    if (n->key <= lo || n->key >= hi) return -1;
    if (n->red && (n->l->red || n->r->red)) return -1;                        /* no red node has a red child */
    if ((n->l != &t->nil && n->l->p != n) || (n->r != &t->nil && n->r->p != n)) return -1;
    int a = rb_check_rec(t, n->l, lo, n->key), b = rb_check_rec(t, n->r, n->key, hi);
    if (a < 0 || b < 0 || a != b) return -1;                                  /* equal black count on every path */
    return a + !n->red;
}
static inline int rb_check(const RBT *t) { return !t->root->red && !t->nil.red && rb_check_rec(t, t->root, -(1L << 40), 1L << 40) >= 0; }
static int rb_height(const RBT *t, const RB *n) { if (n == &t->nil) return 0; int a = rb_height(t, n->l), b = rb_height(t, n->r); return 1 + (a > b ? a : b); }
#endif
```

Note two details that bite when this is rewritten from memory: `transplant` assigns `v->p = u->p` **even when `v` is the sentinel** (the fix-up reads `x->p` afterwards), and the checker also validates parent pointers, since a rotation that forgets one produces a tree that works for lookups but corrupts later fix-ups.

## Verification

Both implementations are exercised with 100,000 random inserts, deletes and lookups against a membership table, with every invariant re-verified after each operation. Then each is fed 1000 to 1,000,000 keys in sorted order — the worst case for a plain BST — and in random order.

```c
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "avl.h"
#include "rb.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* 1. differential + invariant test: AVL and RB vs a membership table, invariants verified after EVERY operation */
    enum { RANGE = 1500, OPS = 100000 };
    static char present[RANGE]; AVL *avl = NULL; RBT rb; rb_init(&rb);
    for (int i = 0; i < OPS; i++) {
        int k = (int)(rnd() % RANGE), op = (int)(rnd() % 3);
        if (op == 0) {
            int ins = 0; avl = avl_insert(avl, k, &ins); int ins2 = rb_insert(&rb, k);
            assert(ins == !present[k] && ins2 == ins); present[k] = 1;
        } else if (op == 1) {
            int del = 0; avl = avl_delete(avl, k, &del); int del2 = rb_delete(&rb, k);
            assert(del == present[k] && del2 == del); present[k] = 0;
        } else {
            assert((avl_find(avl, k) != NULL) == present[k] && rb_find(&rb, k) == present[k]);
        }
        assert(avl_check(avl, -1, RANGE) >= 0);
        assert(rb_check(&rb));
    }
    printf("random insert/delete/find, %d ops: AVL and red-black invariants held after every operation\n\n", OPS);
    avl_free(avl); rb_free(&rb);

    /* 2. worst-case height: sorted insertion */
    printf("%9s %8s | %12s %12s | %12s %12s | %9s\n", "n", "ceil lg", "AVL sorted", "AVL random", "RB sorted", "RB random", "1.44lg n");
    for (int n = 1000; n <= 1000000; n *= 10) {
        AVL *a1 = NULL, *a2 = NULL; RBT r1, r2; rb_init(&r1); rb_init(&r2); int d;
        for (int i = 0; i < n; i++) { d = 0; a1 = avl_insert(a1, i, &d); rb_insert(&r1, i); }
        for (int i = 0; i < n; i++) { int k = (int)(rnd() % (4u * (unsigned)n)); d = 0; a2 = avl_insert(a2, k, &d); rb_insert(&r2, k); }
        printf("%9d %8d | %12d %12d | %12d %12d | %9.1f\n", n, (int)ceil(log2(n + 1)), a1->h, a2->h, rb_height(&r1, r1.root), rb_height(&r2, r2.root), 1.44 * log2(n));
        avl_free(a1); avl_free(a2); rb_free(&r1); rb_free(&r2);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_balanced.c -o test_balanced -lm && ./test_balanced
random insert/delete/find, 100000 ops: AVL and red-black invariants held after every operation

        n  ceil lg |   AVL sorted   AVL random |    RB sorted    RB random |  1.44lg n
     1000       10 |           10           11 |           17           12 |      14.4
    10000       14 |           14           16 |           24           16 |      19.1
   100000       17 |           17           20 |           31           20 |      23.9
  1000000       20 |           20           24 |           37           24 |      28.7
```

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

```c
#ifndef RANDOMIZED_H
#define RANDOMIZED_H
#include <stdint.h>
#include <stdlib.h>

static uint64_t rs = 0x9e3779b97f4a7c15ULL;
static inline uint32_t rr(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }

/* ---------------- Treap: BST on key, max-heap on a random priority ---------------- */
typedef struct Treap { int key; uint32_t pri; struct Treap *l, *r; } Treap;

/* split t into (< key) and (>= key) */
static void tsplit(Treap *t, int key, Treap **a, Treap **b) {
    if (!t) { *a = *b = NULL; return; }
    if (t->key < key) { *a = t; tsplit(t->r, key, &t->r, b); }
    else              { *b = t; tsplit(t->l, key, a, &t->l); }
}
/* merge a and b where every key of a < every key of b */
static Treap *tmerge(Treap *a, Treap *b) {
    if (!a) return b;
    if (!b) return a;
    if (a->pri > b->pri) { a->r = tmerge(a->r, b); return a; }
    b->l = tmerge(a, b->l); return b;
}
static int treap_find(const Treap *t, int key) { while (t && t->key != key) t = key < t->key ? t->l : t->r; return t != NULL; }
static Treap *treap_insert(Treap *t, int key, int *ins) {
    if (treap_find(t, key)) { *ins = 0; return t; }
    *ins = 1;
    Treap *n = malloc(sizeof *n); n->key = key; n->pri = rr(); n->l = n->r = NULL;
    Treap *a, *b; tsplit(t, key, &a, &b);
    return tmerge(tmerge(a, n), b);
}
static Treap *treap_delete(Treap *t, int key, int *del) {
    if (!t) { *del = 0; return NULL; }
    if (key < t->key) { t->l = treap_delete(t->l, key, del); return t; }
    if (key > t->key) { t->r = treap_delete(t->r, key, del); return t; }
    *del = 1; Treap *m = tmerge(t->l, t->r); free(t); return m;
}
static int treap_height(const Treap *t) { if (!t) return 0; int a = treap_height(t->l), b = treap_height(t->r); return 1 + (a > b ? a : b); }
static int treap_check(const Treap *t, long lo, long hi) {
    if (!t) return 1;
    if (t->key <= lo || t->key >= hi) return 0;
    if ((t->l && t->l->pri > t->pri) || (t->r && t->r->pri > t->pri)) return 0;     /* heap order */
    return treap_check(t->l, lo, t->key) && treap_check(t->r, t->key, hi);
}
static void treap_free(Treap *t) { if (!t) return; treap_free(t->l); treap_free(t->r); free(t); }

/* ---------------- Skip list (Pugh 1990), p = 1/2, up to 24 levels ---------------- */
#define SL_MAX 24
typedef struct SNode { int key; int lvl; struct SNode *next[]; } SNode;
typedef struct { SNode *head; int lvl; size_t n; } Skip;

static void skip_init(Skip *s) { s->head = calloc(1, sizeof(SNode) + SL_MAX * sizeof(SNode *)); s->head->lvl = SL_MAX; s->lvl = 1; s->n = 0; }
static int rand_level(void) { int l = 1; uint32_t r = rr(); while ((r & 1) && l < SL_MAX) { l++; r >>= 1; } return l; }   /* geometric: P(level >= j) = 2^-(j-1) */
static int skip_find(const Skip *s, int key) {
    const SNode *x = s->head;
    for (int i = s->lvl - 1; i >= 0; i--) while (x->next[i] && x->next[i]->key < key) x = x->next[i];
    x = x->next[0];
    return x && x->key == key;
}
static int skip_insert(Skip *s, int key) {
    SNode *update[SL_MAX], *x = s->head;
    for (int i = s->lvl - 1; i >= 0; i--) { while (x->next[i] && x->next[i]->key < key) x = x->next[i]; update[i] = x; }
    if (x->next[0] && x->next[0]->key == key) return 0;
    int l = rand_level();
    if (l > s->lvl) { for (int i = s->lvl; i < l; i++) update[i] = s->head; s->lvl = l; }
    SNode *n = malloc(sizeof(SNode) + (size_t)l * sizeof(SNode *)); n->key = key; n->lvl = l;
    for (int i = 0; i < l; i++) { n->next[i] = update[i]->next[i]; update[i]->next[i] = n; }
    s->n++; return 1;
}
static int skip_delete(Skip *s, int key) {
    SNode *update[SL_MAX], *x = s->head;
    for (int i = s->lvl - 1; i >= 0; i--) { while (x->next[i] && x->next[i]->key < key) x = x->next[i]; update[i] = x; }
    x = x->next[0];
    if (!x || x->key != key) return 0;
    for (int i = 0; i < s->lvl; i++) if (update[i]->next[i] == x) update[i]->next[i] = x->next[i];
    free(x);
    while (s->lvl > 1 && !s->head->next[s->lvl - 1]) s->lvl--;
    s->n--; return 1;
}
static void skip_free(Skip *s) { SNode *x = s->head->next[0]; while (x) { SNode *n = x->next[0]; free(x); x = n; } free(s->head); }
#endif
```

## Measured costs

Both randomized structures are checked against the same reference table (treap order and heap property verified every 100 operations), then all five structures plus a sorted array are timed on the same shuffled keys.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "avl.h"
#include "rb.h"
#include "bst.h"
#include "randomized.h"
#include "../../c/perf/timing.h"

static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

static void shuffle(int *a, int n) { for (int i = n - 1; i > 0; i--) { int j = (int)(rr() % (uint32_t)(i + 1)), t = a[i]; a[i] = a[j]; a[j] = t; } }

int main(void) {
    /* correctness of treap and skip list under random operations */
    { enum { R = 1500, OPS = 100000 }; static char present[R]; Treap *t = NULL; Skip sk; skip_init(&sk);
      for (int i = 0; i < OPS; i++) {
          int k = (int)(rr() % R), op = (int)(rr() % 3), f;
          if (op == 0) { t = treap_insert(t, k, &f); assert(f == !present[k]); assert(skip_insert(&sk, k) == f); present[k] = 1; }
          else if (op == 1) { t = treap_delete(t, k, &f); assert(f == present[k]); assert(skip_delete(&sk, k) == f); present[k] = 0; }
          else { assert(treap_find(t, k) == present[k] && skip_find(&sk, k) == present[k]); }
          if (i % 100 == 0) assert(treap_check(t, -1, R));
      }
      printf("treap and skip list: %d random ops matched a reference table (treap heap/BST order checked every 100 ops)\n", OPS);
      treap_free(t); skip_free(&sk); }

    printf("\nbuild n random keys, then n random lookups (ns per operation, single run, shuffled distinct keys)\n");
    printf("%9s | %-10s %9s %9s %7s | memory/node\n", "n", "structure", "insert", "lookup", "height");
    for (int n = 10000; n <= 1000000; n *= 10) {
        int *keys = malloc((size_t)n * sizeof *keys), *probe = malloc((size_t)n * sizeof *probe);
        for (int i = 0; i < n; i++) keys[i] = i * 3;
        shuffle(keys, n);
        memcpy(probe, keys, (size_t)n * sizeof *probe); shuffle(probe, n);
        volatile long sink = 0; double t0;

        BNode *b = NULL; t0 = now(); for (int i = 0; i < n; i++) bst_insert(&b, keys[i]); double ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += bst_find(b, probe[i]) != NULL; double tl = now() - t0;
        printf("%9d | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", n, "plain BST", ti / n * 1e9, tl / n * 1e9, bst_height(b)); bst_free(b);

        AVL *a = NULL; int d; t0 = now(); for (int i = 0; i < n; i++) a = avl_insert(a, keys[i], &d); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += avl_find(a, probe[i]) != NULL; tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", "", "AVL", ti / n * 1e9, tl / n * 1e9, a->h); avl_free(a);

        RBT r; rb_init(&r); t0 = now(); for (int i = 0; i < n; i++) rb_insert(&r, keys[i]); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += rb_find(&r, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 32 B (malloc 48)\n", "", "red-black", ti / n * 1e9, tl / n * 1e9, rb_height(&r, r.root)); rb_free(&r);

        Treap *tr = NULL; t0 = now(); for (int i = 0; i < n; i++) tr = treap_insert(tr, keys[i], &d); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += treap_find(tr, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", "", "treap", ti / n * 1e9, tl / n * 1e9, treap_height(tr)); treap_free(tr);

        Skip sk; skip_init(&sk); t0 = now(); for (int i = 0; i < n; i++) skip_insert(&sk, keys[i]); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += skip_find(&sk, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 8+8*2 B avg (levels: %d)\n", "", "skip list", ti / n * 1e9, tl / n * 1e9, sk.lvl, sk.lvl); skip_free(&sk);

        qsort(keys, (size_t)n, sizeof *keys, cmp_int);
        t0 = now(); for (int i = 0; i < n; i++) sink += bsearch(&probe[i], keys, (size_t)n, sizeof *keys, cmp_int) != NULL; tl = now() - t0;
        printf("%9s | %-10s %9s %9.0f %7s | 4 B\n\n", "", "sorted arr", "(static)", tl / n * 1e9, "-");
        free(keys); free(probe); (void)sink;
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_trees.c -o bench_trees && ./bench_trees
treap and skip list: 100000 random ops matched a reference table (treap heap/BST order checked every 100 ops)

build n random keys, then n random lookups (ns per operation, single run, shuffled distinct keys)
        n | structure     insert    lookup  height | memory/node
    10000 | plain BST        186       109      32 | 24 B (malloc 32)
          | AVL              229        75      16 | 24 B (malloc 32)
          | red-black        186        70      16 | 32 B (malloc 48)
          | treap            494       120      33 | 24 B (malloc 32)
          | skip list        289       288      14 | 8+8*2 B avg (levels: 14)
          | sorted arr  (static)       142       - | 4 B

   100000 | plain BST        318       231      43 | 24 B (malloc 32)
          | AVL              428       138      20 | 24 B (malloc 32)
          | red-black        315       140      20 | 32 B (malloc 48)
          | treap            724       297      42 | 24 B (malloc 32)
          | skip list        484       509      16 | 8+8*2 B avg (levels: 16)
          | sorted arr  (static)       196       - | 4 B

  1000000 | plain BST        831       883      50 | 24 B (malloc 32)
          | AVL             1086       469      24 | 24 B (malloc 32)
          | red-black        752       459      25 | 32 B (malloc 48)
          | treap           1021      1888      50 | 24 B (malloc 32)
          | skip list       1809      1876      20 | 8+8*2 B avg (levels: 20)
          | sorted arr  (static)       141       - | 4 B
```

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
