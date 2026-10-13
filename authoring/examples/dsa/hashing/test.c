// Randomized differential test of oamap.h against a trivial array oracle, with heavy deletion churn.
#include "oamap.h"
#include <stdio.h>
#include <string.h>

#define KEYS 512            // small key space => many collisions, re-insertions and deletions
static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

int main(void) {
    bool present[KEYS] = {0}; uint64_t value[KEYS] = {0};
    map_t m; map_init(&m, 8);
    size_t ops = 2000000, count = 0;
    for (size_t t = 0; t < ops; t++) {
        uint64_t k = rnd() % KEYS * 977 + 3;     // spread keys; index back via k
        size_t idx = (k - 3) / 977;
        switch (rnd() % 3) {
        case 0: { uint64_t v = rnd(); map_put(&m, k, v); if (!present[idx]) count++; present[idx] = true; value[idx] = v; break; }
        case 1: { bool r = map_del(&m, k); if (r != present[idx]) { printf("delete mismatch\n"); return 1; } if (present[idx]) count--; present[idx] = false; break; }
        default: { uint64_t v = 0; bool r = map_get(&m, k, &v);
                   if (r != present[idx] || (r && v != value[idx])) { printf("get mismatch\n"); return 1; } }
        }
        if (m.n != count) { printf("size mismatch\n"); return 1; }
    }
    // final audit: every present key findable, no extras
    for (size_t i = 0; i < KEYS; i++) {
        uint64_t v; bool r = map_get(&m, i * 977 + 3, &v);
        if (r != present[i] || (r && v != value[i])) { printf("final mismatch\n"); return 1; }
    }
    printf("%zu random operations match the oracle; final size %zu, capacity %zu\n", ops, m.n, m.cap);
    map_free(&m);
    return 0;
}
