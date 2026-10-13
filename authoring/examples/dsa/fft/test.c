// Verify the NTT multiplication against the schoolbook O(n*m) product, and check the transform's own properties.
#include "ntt.h"
#include <stdio.h>

static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

int main(void) {
    long checks = 0;
    for (int trial = 0; trial < 3000; trial++) {
        size_t na = 1 + rnd() % 70, nb = 1 + rnd() % 70;
        uint64_t *a = malloc(na * 8), *b = malloc(nb * 8), *want = calloc(na + nb - 1, 8);
        for (size_t i = 0; i < na; i++) a[i] = rnd() % MOD;
        for (size_t i = 0; i < nb; i++) b[i] = rnd() % MOD;
        for (size_t i = 0; i < na; i++) for (size_t j = 0; j < nb; j++) want[i + j] = (want[i + j] + a[i] * b[j]) % MOD;
        size_t nc; uint64_t *got = poly_mul(a, na, b, nb, &nc);
        if (nc != na + nb - 1 || memcmp(got, want, nc * 8)) { printf("MISMATCH at trial %d (na=%zu nb=%zu)\n", trial, na, nb); return 1; }
        checks += nc; free(a); free(b); free(want); free(got);
    }
    // transform then inverse transform returns the input
    uint64_t v[1024], w[1024]; for (int i = 0; i < 1024; i++) w[i] = v[i] = rnd() % MOD;
    ntt(v, 1024, 0); ntt(v, 1024, 1);
    printf("%ld product coefficients matched schoolbook multiplication; inverse(NTT(x)) == x: %s\n", checks, memcmp(v, w, sizeof v) ? "NO" : "yes");
    printf("p - 1 = %llu = 119 * 2^23, so lengths up to 2^23 = %d are supported; 3 has order p-1: 3^((p-1)/2) = p-1 (%s), 3^((p-1)/7) != 1 (%s)\n",
           (unsigned long long)(MOD - 1), 1 << 23, pw(3, (MOD - 1) / 2) == MOD - 1 ? "ok" : "BAD", pw(3, (MOD - 1) / 7) != 1 ? "ok" : "BAD");
    return 0;
}
