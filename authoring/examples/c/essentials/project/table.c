#include "table.h"
#include <stdlib.h>
#include <string.h>

typedef struct Node { char *word; unsigned count; struct Node *next; } Node;
struct Table { Node **buckets; size_t nbuckets, size; };

static size_t hash(const char *s) {
    size_t h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

Table *table_new(size_t buckets) {
    Table *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->buckets = calloc(buckets, sizeof *t->buckets);
    if (!t->buckets) { free(t); return NULL; }
    t->nbuckets = buckets;
    return t;
}

int table_add(Table *t, const char *word) {
    size_t b = hash(word) % t->nbuckets;
    for (Node *n = t->buckets[b]; n; n = n->next)
        if (strcmp(n->word, word) == 0) { n->count++; return 0; }
    Node *n = malloc(sizeof *n);
    if (!n) return -1;
    size_t len = strlen(word) + 1;
    n->word = malloc(len);
    if (!n->word) { free(n); return -1; }
    memcpy(n->word, word, len);
    n->count = 1; n->next = t->buckets[b]; t->buckets[b] = n;
    t->size++;
    return 0;
}

size_t table_size(const Table *t) { return t->size; }

Entry *table_entries(const Table *t) {
    Entry *out = malloc((t->size ? t->size : 1) * sizeof *out);
    if (!out) return NULL;
    size_t i = 0;
    for (size_t b = 0; b < t->nbuckets; b++)
        for (Node *n = t->buckets[b]; n; n = n->next) { out[i].word = n->word; out[i].count = n->count; i++; }
    return out;
}

void table_free(Table *t) {
    if (!t) return;
    for (size_t b = 0; b < t->nbuckets; b++)
        for (Node *n = t->buckets[b], *next; n; n = next) { next = n->next; free(n->word); free(n); }
    free(t->buckets);
    free(t);
}
