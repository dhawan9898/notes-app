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
