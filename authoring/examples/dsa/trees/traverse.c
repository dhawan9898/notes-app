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
