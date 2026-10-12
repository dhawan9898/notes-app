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
