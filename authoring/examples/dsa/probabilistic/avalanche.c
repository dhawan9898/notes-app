#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

typedef uint64_t (*hfn)(uint64_t);
static uint64_t h_identity(uint64_t x) { return x; }
static uint64_t h_modmul(uint64_t x) { return x * 0x9e3779b97f4a7c15ULL; }                 /* Fibonacci hashing: one multiply */
static uint64_t h_fnv(uint64_t x) { return fnv1a(&x, sizeof x); }
static uint64_t h_mix(uint64_t x) { return mix64(x); }

int main(void) {
    struct { const char *name; hfn f; } fs[] = { {"identity", h_identity}, {"x * golden ratio", h_modmul}, {"FNV-1a (8 bytes)", h_fnv}, {"mix64 (splitmix)", h_mix} };
    printf("avalanche test: flip ONE input bit, count how many of the 64 output bits change (ideal: 32 on average)\n");
    printf("%-18s %12s %12s %20s\n", "hash", "avg flipped", "worst bit", "min/max over input bits");
    uint64_t r = 88172645463325252ULL;
    for (int k = 0; k < 4; k++) {
        double total = 0; long trials = 0; double bit_avg[64] = {0};
        for (int t = 0; t < 20000; t++) {
            r ^= r << 13; r ^= r >> 7; r ^= r << 17;
            uint64_t x = r, hx = fs[k].f(x);
            for (int b = 0; b < 64; b++) { int d = __builtin_popcountll(hx ^ fs[k].f(x ^ (1ULL << b))); total += d; trials++; bit_avg[b] += d; }
        }
        double mn = 64, mx = 0;
        for (int b = 0; b < 64; b++) { bit_avg[b] /= 20000; if (bit_avg[b] < mn) mn = bit_avg[b]; if (bit_avg[b] > mx) mx = bit_avg[b]; }
        printf("%-18s %12.2f %12.2f %12.1f / %.1f\n", fs[k].name, total / trials, mn, mn, mx);
    }
    /* low bits matter: tables index with (hash & mask).  Keys that are multiples of 1024 collide in the low 10 bits for weak hashes. */
    printf("\nbucket spread for 100000 keys that are multiples of 1024, into 1024 buckets using the LOW 10 bits of the hash:\n");
    for (int k = 0; k < 4; k++) {
        static int count[1024]; for (int i = 0; i < 1024; i++) count[i] = 0;
        for (uint64_t i = 0; i < 100000; i++) count[fs[k].f(i * 1024) & 1023]++;
        int used = 0, mx = 0; for (int i = 0; i < 1024; i++) { if (count[i]) used++; if (count[i] > mx) mx = count[i]; }
        printf("  %-18s buckets used: %4d / 1024, fullest bucket: %6d (ideal ~98)\n", fs[k].name, used, mx);
    }
    return 0;
}
