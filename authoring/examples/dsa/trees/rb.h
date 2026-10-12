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
