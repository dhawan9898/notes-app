#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "nt.h"
#include "../../c/perf/timing.h"

/* ---- Sieve of Eratosthenes, bit-per-ODD-number: 16x less memory than a byte per number ---- */
static size_t sieve_count(u64 limit) {
    if (limit < 2) return 0;
    size_t nbits = (size_t)(limit - 1) / 2 + 1;                      /* odd numbers 1, 3, 5, ..., bit i represents 2i+1 */
    uint8_t *composite = calloc((nbits + 7) / 8, 1);
    for (u64 i = 1; (2 * i + 1) * (2 * i + 1) <= limit; i++) if (!(composite[i >> 3] >> (i & 7) & 1))
        for (u64 j = ((2 * i + 1) * (2 * i + 1) - 1) / 2; j < nbits; j += 2 * i + 1) composite[j >> 3] |= (uint8_t)(1 << (j & 7));
    size_t count = limit >= 2; for (size_t i = 1; i < nbits; i++) count += !(composite[i >> 3] >> (i & 7) & 1);
    free(composite); return count;
}
/* ---- segmented sieve: sieve [lo, hi) in cache-sized blocks using the primes up to sqrt(hi) ---- */
static size_t segmented_count(u64 limit, size_t seg_bytes) {
    u64 root = (u64)sqrt((double)limit) + 1; uint8_t *small = calloc(root + 1, 1); size_t np = 0; u64 *primes = malloc((root + 1) * sizeof *primes);
    for (u64 i = 2; i <= root; i++) { if (!small[i]) { primes[np++] = i; for (u64 j = i * i; j <= root; j += i) small[j] = 1; } }
    size_t count = 0; uint8_t *blk = malloc(seg_bytes);
    for (u64 lo = 2; lo <= limit; lo += seg_bytes) {
        u64 hi = lo + seg_bytes - 1 < limit ? lo + seg_bytes - 1 : limit; size_t len = (size_t)(hi - lo + 1); memset(blk, 0, len);
        for (size_t k = 0; k < np && primes[k] * primes[k] <= hi; k++) {
            u64 p = primes[k], start = (lo + p - 1) / p * p; if (start < p * p) start = p * p;
            for (u64 j = start; j <= hi; j += p) blk[j - lo] = 1;
        }
        for (size_t i = 0; i < len; i++) count += !blk[i];
    }
    free(small); free(primes); free(blk); return count;
}

int main(void) {
    /* 1. known prime-counting values pi(x) */
    struct { u64 x; size_t pi; } known[] = { { 100, 25 }, { 10000, 1229 }, { 1000000, 78498 }, { 10000000, 664579 }, { 100000000, 5761455 } };
    printf("%12s %12s %12s | %s\n", "x", "pi(x) known", "bit sieve", "segmented (32 KB blocks)");
    for (int i = 0; i < 5; i++) { size_t a = sieve_count(known[i].x), b = segmented_count(known[i].x, 32768); assert(a == known[i].pi && b == known[i].pi); printf("%12llu %12zu %12zu | %zu\n", (unsigned long long)known[i].x, known[i].pi, a, b); }

    /* 2. Miller-Rabin vs the sieve on EVERY integer below 2,000,000, then known strong pseudoprimes */
    size_t lim = 2000000; uint8_t *c = calloc(lim + 1, 1); c[0] = c[1] = 1; for (u64 i = 2; i * i <= lim; i++) if (!c[i]) for (u64 j = i * i; j <= lim; j += i) c[j] = 1;
    for (u64 n = 0; n <= lim; n++) assert(miller_rabin(n) == !c[n]);
    printf("\nMiller-Rabin (12 fixed bases) agrees with the sieve on every integer 0..%zu\n", lim);
    printf("561 (Carmichael): %d;  3215031751 (strong pseudoprime to bases 2,3,5,7): %d;  2^61-1 (Mersenne prime): %d;  2^64-59 (largest 64-bit prime): %d;  2^64-1: %d\n",
           miller_rabin(561), miller_rabin(3215031751ULL), miller_rabin((1ULL << 61) - 1), miller_rabin(0xFFFFFFFFFFFFFFC5ULL), miller_rabin(0xFFFFFFFFFFFFFFFFULL));
    free(c);

    /* 3. timing */
    double t0 = now(); size_t p1 = sieve_count(1000000000ULL); double t1 = now() - t0;
    t0 = now(); size_t p2 = segmented_count(1000000000ULL, 32768); double t2 = now() - t0; assert(p1 == p2);
    printf("\nprimes below 10^9: %zu (known: 50847534).  plain bit sieve (62.5 MB): %.2f s;  segmented, 32 KB working set: %.2f s\n", p1, t1, t2);
    t0 = now(); u64 x = 1000000000000000000ULL; size_t found = 0, tested = 0; for (; found < 1000; x++, tested++) found += miller_rabin(x);
    double t3 = now() - t0; printf("Miller-Rabin: the first 1000 primes above 10^18 found by testing %zu numbers in %.1f ms (%.0f ns per test; density about 1/ln(10^18) = 1/41.4)\n", tested, t3 * 1e3, t3 / (double)tested * 1e9);
    return 0;
}
