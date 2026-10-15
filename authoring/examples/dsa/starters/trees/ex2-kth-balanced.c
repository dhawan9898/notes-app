// Exercise 2: the k-th smallest key without copying the tree, and a height-balance check.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int key; struct Node *l, *r; } Node;

/* ===== Your code: edit below this line ===== */
/* Store the k-th smallest key (k = 1 is the smallest) in *out and return 1, or return 0 if the tree has fewer than k keys. */
int kth_smallest(const Node *root, int k, int *out) {
    (void)root; (void)k; (void)out;
    return 0;
}

/* Return 1 if, at every node, the heights of the two subtrees differ by at most 1. */
int is_balanced(const Node *root) {
    (void)root;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

static Node pool[32];
static int used;
static Node *mk(int key, Node *l, Node *r) { Node *n = &pool[used++]; n->key = key; n->l = l; n->r = r; return n; }

int main(void) {
    Node *good = mk(50, mk(30, mk(20, NULL, NULL), mk(40, NULL, NULL)), mk(70, mk(60, NULL, NULL), mk(80, NULL, NULL)));
    int v = -1;
    check_int("kth_smallest(good, 3) found", kth_smallest(good, 3, &v), 1);
    check_int("  key", v, 40);
    v = -1; kth_smallest(good, 1, &v);
    check_int("kth_smallest(good, 1)", v, 20);
    v = -1; kth_smallest(good, 7, &v);
    check_int("kth_smallest(good, 7)", v, 80);
    check_int("kth_smallest(good, 8) (only 7 keys)", kth_smallest(good, 8, &v), 0);
    check_int("is_balanced(good)", is_balanced(good), 1);
    check_int("is_balanced(chain 1-2-3)", is_balanced(mk(1, NULL, mk(2, NULL, mk(3, NULL, NULL)))), 0);
    check_int("is_balanced(empty)", is_balanced(NULL), 1);
    /* root heights match (2 and 2) but each child is a chain: balance must hold at every node */
    Node *deep = mk(10, mk(5, mk(4, mk(3, NULL, NULL), NULL), NULL), mk(15, NULL, mk(16, NULL, mk(17, NULL, NULL))));
    check_int("is_balanced(equal heights at the root only)", is_balanced(deep), 0);
    return summary();
}
