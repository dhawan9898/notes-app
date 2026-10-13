// Open-addressing hash map, linear probing, power-of-two capacity, backward-shift deletion (no tombstones).
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct { uint64_t key, val; bool full; } slot_t;
typedef struct { slot_t *s; size_t cap, n; unsigned long probes; } map_t;   // probes: statistics only

static inline uint64_t mix64(uint64_t x) {          // SplitMix64 finalizer: good avalanche in the LOW bits
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}

static bool map_init(map_t *m, size_t cap) {         // cap: power of two
    m->s = calloc(cap, sizeof *m->s); m->cap = cap; m->n = 0; m->probes = 0;
    return m->s != NULL;
}
static void map_free(map_t *m) { free(m->s); m->s = NULL; }

static bool map_get(map_t *m, uint64_t k, uint64_t *out) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask;; i = (i + 1) & mask) {
        m->probes++;
        if (!m->s[i].full) return false;             // an empty slot ends the probe chain
        if (m->s[i].key == k) { if (out) *out = m->s[i].val; return true; }
    }
}

static bool map_grow(map_t *m);

static bool map_put_nogrow(map_t *m, uint64_t k, uint64_t v) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask;; i = (i + 1) & mask) {
        if (!m->s[i].full) { m->s[i] = (slot_t){k, v, true}; m->n++; return true; }
        if (m->s[i].key == k) { m->s[i].val = v; return true; }
    }
}

static bool map_put(map_t *m, uint64_t k, uint64_t v) {
    if ((m->n + 1) * 4 > m->cap * 3 && !map_grow(m)) return false;    // keep load factor <= 0.75
    return map_put_nogrow(m, k, v);
}

static bool map_grow(map_t *m) {
    map_t n;
    if (!map_init(&n, m->cap * 2)) return false;
    for (size_t i = 0; i < m->cap; i++) if (m->s[i].full) map_put_nogrow(&n, m->s[i].key, m->s[i].val);
    free(m->s); n.probes = m->probes; *m = n; return true;
}

// Backward-shift deletion: after emptying slot i, pull later members of the same probe run back
// into the hole whenever that does not move them in front of their home slot.
static bool map_del(map_t *m, uint64_t k) {
    size_t mask = m->cap - 1, i = mix64(k) & mask;
    while (m->s[i].full && m->s[i].key != k) i = (i + 1) & mask;
    if (!m->s[i].full) return false;
    size_t hole = i;
    for (size_t j = (i + 1) & mask; m->s[j].full; j = (j + 1) & mask) {
        size_t home = mix64(m->s[j].key) & mask;
        // can slot j move into the hole? only if its home is NOT cyclically in (hole, j]
        bool in_between = hole <= j ? (hole < home && home <= j) : (hole < home || home <= j);
        if (!in_between) { m->s[hole] = m->s[j]; hole = j; }
    }
    m->s[hole].full = false; m->n--;
    return true;
}
