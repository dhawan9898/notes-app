#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

typedef struct { uint64_t *bits; size_t m; int k; } Bloom;      /* m bits, k hash functions */

static Bloom bloom_new(size_t m, int k) { Bloom b = { calloc((m + 63) / 64, 8), m, k }; return b; }
/* Kirsch-Mitzenmacher double hashing: k indices from just two hashes, h_i = h1 + i*h2 (no loss of asymptotic false-positive rate). */
static inline size_t bloom_idx(const Bloom *b, uint64_t h1, uint64_t h2, int i) { return (size_t)((h1 + (uint64_t)i * h2) % b->m); }
static void bloom_add(Bloom *b, uint64_t key) {
    uint64_t h1 = hash64(key, 1), h2 = hash64(key, 2) | 1;
    for (int i = 0; i < b->k; i++) { size_t j = bloom_idx(b, h1, h2, i); b->bits[j >> 6] |= 1ULL << (j & 63); }
}
static int bloom_maybe(const Bloom *b, uint64_t key) {
    uint64_t h1 = hash64(key, 1), h2 = hash64(key, 2) | 1;
    for (int i = 0; i < b->k; i++) { size_t j = bloom_idx(b, h1, h2, i); if (!(b->bits[j >> 6] >> (j & 63) & 1)) return 0; }
    return 1;
}

int main(void) {
    size_t n = 1000000;                                         /* items inserted */
    printf("Bloom filter, n = %zu inserted keys, false-positive rate measured with 2,000,000 absent keys\n", n);
    printf("%10s %8s %10s %14s %14s %12s\n", "bits/key", "k", "memory", "theory", "measured", "set bits");
    double bpk[] = { 4, 8, 10, 12, 16 };
    for (int c = 0; c < 5; c++) {
        size_t m = (size_t)(bpk[c] * (double)n);
        int kopt = (int)round(bpk[c] * log(2.0));              /* optimal k = (m/n) ln 2 */
        Bloom b = bloom_new(m, kopt);
        for (uint64_t i = 0; i < n; i++) bloom_add(&b, i);
        size_t fp = 0, trials = 2000000;
        for (uint64_t i = 0; i < trials; i++) fp += (size_t)bloom_maybe(&b, (1ULL << 40) + i);
        size_t ones = 0; for (size_t w = 0; w < (m + 63) / 64; w++) ones += (size_t)__builtin_popcountll(b.bits[w]);
        double theory = pow(1.0 - exp(-(double)kopt * (double)n / (double)m), kopt);
        printf("%10.0f %8d %7.2f MB %13.4f%% %13.4f%% %11.1f%%\n", bpk[c], kopt, (double)m / 8e6, theory * 100, 100.0 * (double)fp / (double)trials, 100.0 * (double)ones / (double)m);
        /* no false negatives, ever */
        for (uint64_t i = 0; i < n; i += 997) if (!bloom_maybe(&b, i)) { puts("FALSE NEGATIVE!"); return 1; }
        free(b.bits);
    }
    printf("\nsizing: for target false-positive rate p, bits per key = -ln(p)/(ln 2)^2 and k = -log2(p):\n");
    double ps[] = { 0.1, 0.01, 0.001, 0.0001 };
    for (int i = 0; i < 4; i++) printf("  p = %-7g -> %5.2f bits/key, k = %2.0f   (a hash set of 64-bit keys needs >= 64 bits/key)\n", ps[i], -log(ps[i]) / (log(2.0) * log(2.0)), ceil(-log2(ps[i])));
    return 0;
}
