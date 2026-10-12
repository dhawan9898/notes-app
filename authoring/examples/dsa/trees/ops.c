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
