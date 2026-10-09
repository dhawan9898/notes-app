#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static unsigned pc_loop(uint64_t x) { unsigned c = 0; while (x) { c += x & 1; x >>= 1; } return c; }
static unsigned pc_kern(uint64_t x) { unsigned c = 0; while (x) { x &= x - 1; c++; } return c; }
static unsigned pc_table(uint64_t x) { static unsigned char t[256]; static int init; if (!init) { for (int i = 0; i < 256; i++) t[i] = (unsigned char)((i & 1) + t[i / 2]); init = 1; } unsigned c = 0; for (int i = 0; i < 8; i++) c += t[(x >> (8 * i)) & 255]; return c; }
static unsigned pc_swar(uint64_t x) { x = x - ((x >> 1) & 0x5555555555555555ULL); x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL); x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL; return (unsigned)((x * 0x0101010101010101ULL) >> 56); }
static unsigned pc_builtin(uint64_t x) { return (unsigned)__builtin_popcountll(x); }
typedef unsigned (*fn)(uint64_t);
int main(void) {
    enum { N = 20000000 };
    uint64_t *v = malloc(N * sizeof *v); uint64_t s = 88172645463325252ULL;
    for (int i = 0; i < N; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; v[i] = s; }
    struct { const char *name; fn f; } impls[] = { {"bit loop", pc_loop}, {"Kernighan", pc_kern}, {"table (bytes)", pc_table}, {"SWAR", pc_swar}, {"__builtin_popcountll", pc_builtin} };
    unsigned long ref = 0;
    for (size_t k = 0; k < 5; k++) {
        double t0 = now(); unsigned long total = 0;
        for (int i = 0; i < N; i++) total += impls[k].f(v[i]);
        double dt = now() - t0;
        if (k == 0) ref = total;
        printf("%-22s %7.1f ms  (%5.2f ns/value) %s\n", impls[k].name, dt * 1e3, dt * 1e9 / N, total == ref ? "" : "MISMATCH");
    }
    free(v); return 0;
}
