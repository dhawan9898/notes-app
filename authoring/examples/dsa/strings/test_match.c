#include <assert.h>
#include <stdio.h>
#include "match.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    long checks = 0;
    for (int alpha = 1; alpha <= 4; alpha++) for (int round = 0; round < 20000; round++) {
        size_t n = rnd() % 60, m = 1 + rnd() % 8; char t[64], p[16];
        for (size_t i = 0; i < n; i++) t[i] = (char)('a' + rnd() % (unsigned)alpha);
        for (size_t i = 0; i < m; i++) p[i] = (char)('a' + rnd() % (unsigned)alpha);
        size_t want = naive_count(t, n, p, m), fh;
        assert(kmp_count(t, n, p, m) == want); assert(z_count(t, n, p, m) == want); assert(rk_count(t, n, p, m, &fh) == want);
        assert(horspool_count(t, n, p, m) == want); assert(memmem_count(t, n, p, m) == want); checks++;
    }
    printf("%ld random (text, pattern) pairs over alphabets of size 1-4: KMP, Z, Rabin-Karp, Horspool and memmem agree with the naive scan (overlaps included)\n", checks);
    return 0;
}
