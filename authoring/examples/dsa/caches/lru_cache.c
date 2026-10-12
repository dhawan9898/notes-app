#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "policies.h"
#include "../../c/perf/timing.h"

/* A production-shaped LRU cache: hash table (separate chaining) for O(1) lookup + intrusive doubly linked list for O(1) recency updates.
 * Entries are allocated once, up front, from a fixed pool (no malloc/free on the hot path) and recycled through a free list. */
typedef struct Entry { uint64_t key, value; struct Entry *hnext; struct Entry *prev, *next; } Entry;
typedef struct { Entry **bucket; size_t nbuckets; Entry *pool, *free_list, *head, *tail; size_t size, cap; unsigned long long hits, misses, evictions; } LRU;

static inline size_t hash_key(uint64_t k) { k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL; k ^= k >> 33; return (size_t)k; }
static LRU *lru_new(size_t cap) {
    LRU *c = calloc(1, sizeof *c); c->cap = cap; c->nbuckets = 1; while (c->nbuckets < cap * 2) c->nbuckets <<= 1;      /* power of two, load factor <= 0.5 */
    c->bucket = calloc(c->nbuckets, sizeof(Entry *)); c->pool = calloc(cap, sizeof(Entry));
    for (size_t i = 0; i < cap; i++) { c->pool[i].hnext = c->free_list; c->free_list = &c->pool[i]; }
    return c;
}
static void lru_free(LRU *c) { free(c->bucket); free(c->pool); free(c); }
static void list_unlink(LRU *c, Entry *e) { if (e->prev) e->prev->next = e->next; else c->head = e->next; if (e->next) e->next->prev = e->prev; else c->tail = e->prev; }
static void list_push_front(LRU *c, Entry *e) { e->prev = NULL; e->next = c->head; if (c->head) c->head->prev = e; c->head = e; if (!c->tail) c->tail = e; }
static Entry *lookup(LRU *c, uint64_t key) { for (Entry *e = c->bucket[hash_key(key) & (c->nbuckets - 1)]; e; e = e->hnext) if (e->key == key) return e; return NULL; }
static int lru_get(LRU *c, uint64_t key, uint64_t *value) {
    Entry *e = lookup(c, key);
    if (!e) { c->misses++; return 0; }
    c->hits++; if (c->head != e) { list_unlink(c, e); list_push_front(c, e); }                          /* touch: move to the front */
    *value = e->value; return 1;
}
static void lru_put(LRU *c, uint64_t key, uint64_t value) {
    Entry *e = lookup(c, key);
    if (e) { e->value = value; if (c->head != e) { list_unlink(c, e); list_push_front(c, e); } return; }
    if (c->size == c->cap) {                                                                             /* evict the tail (least recently used) */
        Entry *v = c->tail; list_unlink(c, v);
        Entry **pp = &c->bucket[hash_key(v->key) & (c->nbuckets - 1)]; while (*pp != v) pp = &(*pp)->hnext; *pp = v->hnext;
        v->hnext = c->free_list; c->free_list = v; c->size--; c->evictions++;
    }
    e = c->free_list; c->free_list = e->hnext; e->key = key; e->value = value;
    Entry **b = &c->bucket[hash_key(key) & (c->nbuckets - 1)]; e->hnext = *b; *b = e; list_push_front(c, e); c->size++;
}
static void lru_check(const LRU *c) {                                                                    /* invariants: list and hash table hold exactly the same entries */
    size_t n = 0; for (const Entry *e = c->head; e; e = e->next) { n++; assert(e->prev ? e->prev->next == e : c->head == e); assert(lookup((LRU *)c, e->key) == e); }
    assert(n == c->size && n <= c->cap); size_t m = 0; for (size_t b = 0; b < c->nbuckets; b++) for (const Entry *e = c->bucket[b]; e; e = e->hnext) m++; assert(m == n);
}

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

int main(void) {
    /* differential test: hit count must equal the array-based LRU simulator on the same trace */
    int NK = 5000; size_t n = 300000; int *tr = malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) { uint64_t r = rnd(); tr[i] = (r & 3) ? (int)(rnd() % 200) : (int)(rnd() % (uint64_t)NK); }          /* hot set of 200 + a cold tail */
    for (int cap = 10; cap <= 1000; cap *= 10) {
        LRU *c = lru_new((size_t)cap); uint64_t v;
        for (size_t i = 0; i < n; i++) { if (!lru_get(c, (uint64_t)tr[i], &v)) lru_put(c, (uint64_t)tr[i], (uint64_t)tr[i] * 7); else assert(v == (uint64_t)tr[i] * 7); if (i % 50000 == 0) lru_check(c); }
        lru_check(c); ll ref = sim_lru(tr, n, NK, cap, NULL); assert((ll)c->hits == ref);
        printf("capacity %4d: %llu hits, %llu evictions; array simulator reports %lld hits (identical)\n", cap, c->hits, c->evictions, ref); lru_free(c);
    }
    free(tr);

    /* throughput */
    size_t N = 20000000; int NK2 = 1000000; uint64_t *keys = malloc(N * sizeof *keys); for (size_t i = 0; i < N; i++) { uint64_t r = rnd(); keys[i] = (r & 3) ? rnd() % 50000 : rnd() % (uint64_t)NK2; }
    LRU *c = lru_new(100000); uint64_t v; double t0 = now();
    for (size_t i = 0; i < N; i++) if (!lru_get(c, keys[i], &v)) lru_put(c, keys[i], keys[i]);
    double dt = now() - t0;
    printf("\n%.0f million requests/s single-threaded, capacity 100,000 (hit ratio %.1f%%, %llu evictions); ~%zu bytes per entry (%zu-byte entry + %.1f buckets)\n",
           (double)N / dt / 1e6, 100.0 * (double)c->hits / (double)N, c->evictions, sizeof(Entry) + (size_t)(8 * c->nbuckets / c->cap), sizeof(Entry), (double)c->nbuckets / (double)c->cap);
    lru_free(c); free(keys); return 0;
}
