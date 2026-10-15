// Exercise 1: is a binary tree a valid binary search tree?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int key; struct Node *l, *r; } Node;

/* ===== Your code: edit below this line ===== */
/* Every key in the left subtree must be smaller than the node's key, every key in the right subtree larger.
   Hint: pass down the range of keys allowed at each node. */
int is_bst(const Node *root) {
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
    Node *trap = mk(50, mk(30, mk(20, NULL, NULL), mk(55, NULL, NULL)), mk(70, NULL, NULL));   /* 55 > 30, but it is left of 50 */
    Node *flipped = mk(50, mk(70, NULL, NULL), mk(30, NULL, NULL));
    check_int("is_bst(good tree)", is_bst(good), 1);
    check_int("is_bst(55 under 30, left of 50)", is_bst(trap), 0);
    check_int("is_bst(children swapped)", is_bst(flipped), 0);
    check_int("is_bst(empty tree)", is_bst(NULL), 1);
    check_int("is_bst(single node)", is_bst(mk(1, NULL, NULL)), 1);
    return summary();
}
