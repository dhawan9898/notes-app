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
