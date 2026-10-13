// Cuckoo hashing: two tables, two hash functions; every key lives in one of exactly two slots, so a lookup is at most 2 probes.
// Insert kicks the resident out of its slot into its alternate slot, possibly repeatedly; a chain that is too long means a
// cycle, and the whole table is rebuilt with fresh hash seeds. Two-table cuckoo works up to about 50% load.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct { uint64_t *t[2]; bool *used[2]; size_t half; uint64_t seed[2]; size_t kicks; } cuckoo_t;

static uint64_t mix64(uint64_t x) { x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; x ^= x >> 31; return x; }
static size_t slot(const cuckoo_t *c, int w, uint64_t k) { return mix64(k ^ c->seed[w]) % c->half; }

static uint64_t rng = 0x9e3779b97f4a7c15ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void reset(cuckoo_t *c) {                         // empty tables, fresh seeds
    for (int w = 0; w < 2; w++) { free(c->t[w]); free(c->used[w]); c->t[w] = calloc(c->half, 8); c->used[w] = calloc(c->half, 1); c->seed[w] = rnd(); }
}
static bool lookup(const cuckoo_t *c, uint64_t k) {
    for (int w = 0; w < 2; w++) { size_t i = slot(c, w, k); if (c->used[w][i] && c->t[w][i] == k) return true; }
    return false;
}
static bool insert(cuckoo_t *c, uint64_t k) {            // false: kick chain too long (cycle); a key is left homeless
    int w = 0;
    for (int kick = 0; kick < 64; kick++) {
        size_t i = slot(c, w, k);
        if (!c->used[w][i]) { c->used[w][i] = true; c->t[w][i] = k; return true; }
        uint64_t evicted = c->t[w][i]; c->t[w][i] = k; k = evicted; w ^= 1; c->kicks++;
    }
    return false;
}

int main(void) {
    printf("%9s %9s %6s %10s %14s\n", "slots", "keys", "load", "rebuilds", "kicks/insert");
    for (int pct = 20; pct <= 48; pct += 7) {
        cuckoo_t c = { .half = 1 << 16 };
        size_t want = (size_t)(pct / 100.0 * 2 * c.half), rebuilds = 0, inserts = 0;
        uint64_t *keys = malloc(want * 8);
        for (size_t i = 0; i < want; i++) keys[i] = i * 11400714819323198485ULL + 1;
        reset(&c);
        for (size_t i = 0; i < want; i++) {
            inserts++;
            if (!insert(&c, keys[i])) { rebuilds++; reset(&c); i = (size_t)-1; }   // restart with new seeds
        }
        bool all = true;
        for (size_t i = 0; i < want; i++) if (!lookup(&c, keys[i])) all = false;
        printf("%9zu %9zu %5d%% %10zu %14.2f %s\n", 2 * c.half, want, pct, rebuilds, (double)c.kicks / inserts, all ? "" : "LOOKUP FAILED");
        free(keys);
    }
    return 0;
}
