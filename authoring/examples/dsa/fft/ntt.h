// Number-theoretic transform over Z_p with p = 998244353 = 119 * 2^23 + 1 (primitive root 3): exact polynomial multiplication.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MOD 998244353ULL
#define G 3ULL

static uint64_t pw(uint64_t b, uint64_t e) { uint64_t r = 1; b %= MOD; while (e) { if (e & 1) r = r * b % MOD; b = b * b % MOD; e >>= 1; } return r; }

// In-place iterative transform of length n (a power of two, n <= 2^23). invert=1 computes the inverse transform.
static void ntt(uint64_t *a, size_t n, int invert) {
    for (size_t i = 1, j = 0; i < n; i++) {                   // bit-reversal permutation
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { uint64_t t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    for (size_t len = 2; len <= n; len <<= 1) {               // butterflies, doubling the block length each round
        uint64_t w = pw(G, (MOD - 1) / len);                  // a primitive len-th root of unity modulo p
        if (invert) w = pw(w, MOD - 2);
        for (size_t i = 0; i < n; i += len) {
            uint64_t wn = 1;
            for (size_t k = 0; k < len / 2; k++) {
                uint64_t u = a[i + k], v = a[i + k + len / 2] * wn % MOD;
                a[i + k] = u + v >= MOD ? u + v - MOD : u + v;
                a[i + k + len / 2] = u >= v ? u - v : u + MOD - v;
                wn = wn * w % MOD;
            }
        }
    }
    if (invert) { uint64_t ninv = pw(n, MOD - 2); for (size_t i = 0; i < n; i++) a[i] = a[i] * ninv % MOD; }
}

// c = a * b (coefficients mod p). Result has na + nb - 1 coefficients.
static uint64_t *poly_mul(const uint64_t *a, size_t na, const uint64_t *b, size_t nb, size_t *nc) {
    size_t n = 1; while (n < na + nb - 1) n <<= 1;
    uint64_t *fa = calloc(n, sizeof *fa), *fb = calloc(n, sizeof *fb);
    memcpy(fa, a, na * sizeof *fa); memcpy(fb, b, nb * sizeof *fb);
    ntt(fa, n, 0); ntt(fb, n, 0);
    for (size_t i = 0; i < n; i++) fa[i] = fa[i] * fb[i] % MOD;      // pointwise product = convolution in the other domain
    ntt(fa, n, 1);
    free(fb); *nc = na + nb - 1;
    return fa;
}
