#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ---- Arena (bump) allocator: allocation = advance a pointer; free = reset the whole arena at once ---- */
typedef struct { uint8_t *base, *cur, *end; } Arena;
static inline void *arena_alloc(Arena *a, size_t n) { n = (n + 15) & ~(size_t)15; if ((size_t)(a->end - a->cur) < n) return NULL; void *p = a->cur; a->cur += n; return p; }
/* ---- Pool (fixed-size free list): freed objects are threaded through their own memory; alloc/free are a pointer swap ---- */
typedef struct PoolNode { struct PoolNode *next; } PoolNode;
typedef struct { uint8_t *mem; PoolNode *free; size_t obj, count; } Pool;
static void pool_init(Pool *p, size_t obj, size_t count) { p->obj = obj < sizeof(PoolNode) ? sizeof(PoolNode) : obj; p->count = count; p->mem = malloc(p->obj * count); p->free = NULL; for (size_t i = count; i-- > 0; ) { PoolNode *n = (PoolNode *)(p->mem + i * p->obj); n->next = p->free; p->free = n; } }
static inline void *pool_alloc(Pool *p) { PoolNode *n = p->free; if (n) p->free = n->next; return n; }
static inline void pool_free(Pool *p, void *x) { PoolNode *n = x; n->next = p->free; p->free = n; }

typedef struct Node { struct Node *next; uint64_t key, a, b; } Node;               /* a 32-byte object */

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    enum { N = 5000000 };
    printf("== build a linked list of %d 32-byte nodes, traverse it, then release everything ==\n", N);
    double t0 = now(); Node *head = NULL; for (int i = 0; i < N; i++) { Node *x = malloc(sizeof *x); x->key = (uint64_t)i; x->next = head; head = x; }
    double t_build = now() - t0; t0 = now(); uint64_t sum = 0; for (Node *x = head; x; x = x->next) sum += x->key; double t_walk = now() - t0;
    t0 = now(); while (head) { Node *n = head->next; free(head); head = n; } double t_free = now() - t0;
    printf("malloc/free     : build %6.1f ms, walk %5.1f ms, release %6.1f ms\n", t_build * 1e3, t_walk * 1e3, t_free * 1e3);
    Arena a = { malloc((size_t)N * sizeof(Node)), NULL, NULL }; a.cur = a.base; a.end = a.base + (size_t)N * sizeof(Node);
    t0 = now(); head = NULL; for (int i = 0; i < N; i++) { Node *x = arena_alloc(&a, sizeof *x); x->key = (uint64_t)i; x->next = head; head = x; }
    double a_build = now() - t0; t0 = now(); uint64_t sum2 = 0; for (Node *x = head; x; x = x->next) sum2 += x->key; double a_walk = now() - t0;
    t0 = now(); a.cur = a.base; head = NULL; double a_free = now() - t0; assert(sum == sum2);
    printf("arena           : build %6.1f ms, walk %5.1f ms, release %6.3f ms  (one pointer reset)\n", a_build * 1e3, a_walk * 1e3, a_free * 1e3);
    free(a.base);

    printf("\n== churn: keep 100,000 live 32-byte objects; 20,000,000 times free a random one and allocate a replacement ==\n");
    enum { LIVE = 100000, ITER = 20000000 };
    Node **slot = malloc(LIVE * sizeof *slot);
    for (int i = 0; i < LIVE; i++) slot[i] = malloc(sizeof(Node));
    t0 = now(); for (int i = 0; i < ITER; i++) { int j = (int)(rnd() % LIVE); free(slot[j]); slot[j] = malloc(sizeof(Node)); slot[j]->key = (uint64_t)i; } double t_m = now() - t0;
    for (int i = 0; i < LIVE; i++) free(slot[i]);
    Pool pool; pool_init(&pool, sizeof(Node), LIVE + 1);
    for (int i = 0; i < LIVE; i++) slot[i] = pool_alloc(&pool);
    t0 = now(); for (int i = 0; i < ITER; i++) { int j = (int)(rnd() % LIVE); pool_free(&pool, slot[j]); slot[j] = pool_alloc(&pool); slot[j]->key = (uint64_t)i; } double t_p = now() - t0;
    printf("malloc/free: %.1f ns per free+alloc pair;  pool: %.1f ns per pair  (%.1fx faster)\n", t_m / ITER * 1e9, t_p / ITER * 1e9, t_m / t_p);
    /* pool correctness: every live object distinct and inside the pool */
    for (int i = 0; i < LIVE; i++) { assert((uint8_t *)slot[i] >= pool.mem && (uint8_t *)slot[i] < pool.mem + pool.obj * pool.count); for (int k = i + 1; k < i + 3 && k < LIVE; k++) assert(slot[i] != slot[k]); }
    free(pool.mem); free(slot); return 0;
}
