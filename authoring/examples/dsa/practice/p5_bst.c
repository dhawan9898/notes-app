#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct node { int key; struct node *l, *r; } Node;

static Node *mk(int k, Node *l, Node *r) { Node *n = malloc(sizeof *n); if (!n) exit(1); n->key = k; n->l = l; n->r = r; return n; }
static void destroy(Node *n) { if (n) { destroy(n->l); destroy(n->r); free(n); } }

/* Valid BST: EVERY key in the left subtree is smaller and every key in the right is larger, not just the direct children. */
static int valid(const Node *n, long lo, long hi) {
    if (!n) return 1;
    if (n->key <= lo || n->key >= hi) return 0;
    return valid(n->l, lo, n->key) && valid(n->r, n->key, hi);
}

/* k-th smallest by an in-order walk that stops early (k counts down). */
static int kth(const Node *n, int *k, int *out) {
    if (!n) return 0;
    if (kth(n->l, k, out)) return 1;
    if (--*k == 0) { *out = n->key; return 1; }
    return kth(n->r, k, out);
}

/* Height-balanced: for every node, subtree heights differ by at most 1. Returns height, or -1 if unbalanced. */
static int balanced_height(const Node *n) {
    if (!n) return 0;
    int l = balanced_height(n->l), r = balanced_height(n->r);
    if (l < 0 || r < 0 || abs(l - r) > 1) return -1;
    return 1 + (l > r ? l : r);
}

int main(void) {
    Node *good = mk(50, mk(30, mk(20, NULL, NULL), mk(40, NULL, NULL)), mk(70, mk(60, NULL, NULL), mk(80, NULL, NULL)));
    Node *trap = mk(50, mk(30, mk(20, NULL, NULL), mk(55, NULL, NULL)), mk(70, NULL, NULL));   /* 55 is 'right of 30' but left of 50: invalid */
    printf("good tree valid: %d, trap tree valid: %d\n", valid(good, LONG_MIN, LONG_MAX), valid(trap, LONG_MIN, LONG_MAX));
    printf("trap tree checked only against parents would pass: 55 > 30 is %d\n", 55 > 30);
    int k = 3, out = -1;
    kth(good, &k, &out);
    printf("3rd smallest in good tree: %d\n", out);
    printf("good balanced: %s\n", balanced_height(good) >= 0 ? "yes" : "no");
    Node *chain = mk(1, NULL, mk(2, NULL, mk(3, NULL, NULL)));
    printf("chain 1-2-3 balanced: %s\n", balanced_height(chain) >= 0 ? "yes" : "no");
    destroy(good); destroy(trap); destroy(chain);
    return 0;
}
