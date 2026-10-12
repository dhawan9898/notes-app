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
