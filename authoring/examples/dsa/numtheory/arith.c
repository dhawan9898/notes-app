#include <assert.h>
#include <stdio.h>
#include "nt.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

static u64 gcd_naive(u64 a, u64 b) { u64 g = 1; for (u64 d = 1; d <= a && d <= b; d++) if (a % d == 0 && b % d == 0) g = d; return g; }
static u64 powmod_naive(u64 b, u64 e, u64 m) { u64 r = 1 % m; for (u64 i = 0; i < e; i++) r = r * (b % m) % m; return r; }          /* only valid for m < 2^32 */
static u64 fib_iter(u64 n, u64 m) { u64 a = 0, b = 1 % m; for (u64 i = 0; i < n; i++) { u64 c = (a + b) % m; a = b; b = c; } return a; }

/* binomial coefficients modulo a prime via factorial tables and Fermat inverses */
#define P 1000000007ULL
static u64 fact[200001], ifact[200001];
static void init_fact(int n) { fact[0] = 1; for (int i = 1; i <= n; i++) fact[i] = fact[i - 1] * (u64)i % P; ifact[n] = inv_fermat(fact[n], P); for (int i = n; i > 0; i--) ifact[i - 1] = ifact[i] * (u64)i % P; }
static u64 ncr(int n, int k) { return k < 0 || k > n ? 0 : fact[n] * ifact[k] % P * ifact[n - k] % P; }

int main(void) {
    for (int t = 0; t < 3000; t++) { u64 a = rnd() % 2000, b = rnd() % 2000; if (a && b) assert(gcd(a, b) == gcd_naive(a, b)); }
    for (int t = 0; t < 3000; t++) {
        i64 a = (i64)(rnd() % 100000) - 50000, b = (i64)(rnd() % 100000) + 1; i64 x, y, g = ext_gcd(a < 0 ? -a : a, b, &x, &y);
        assert(g == (i64)gcd((u64)(a < 0 ? -a : a), (u64)b)); assert((a < 0 ? -a : a) * x + b * y == g);
    }
    for (int t = 0; t < 3000; t++) { u64 b = rnd() % 1000, e = rnd() % 300, m = 1 + rnd() % 100000; assert(powmod(b, e, m) == powmod_naive(b, e, m)); }
    for (int t = 0; t < 3000; t++) { i64 m = 2 + (i64)(rnd() % 100000), a = 1 + (i64)(rnd() % 100000); i64 inv = inv_euclid(a, m); if (gcd((u64)a, (u64)m) == 1) assert(inv >= 0 && (a % m) * inv % m == 1); else assert(inv == -1); }
    for (u64 p = 2; p < 3000; p++) if (miller_rabin(p)) for (u64 a = 1; a < p; a += 1 + p / 50) assert(mulmod(a, inv_fermat(a, p), p) == 1);
    for (int t = 0; t < 2000; t++) { u64 n = rnd() % 3000, m = 2 + rnd() % 1000000007ULL; u64 f, g; fib_pair(n, m, &f, &g); assert(f == fib_iter(n, m)); }
    for (int t = 0; t < 2000; t++) {
        i64 m1 = 2 + (i64)(rnd() % 5000), m2 = 2 + (i64)(rnd() % 5000); if (gcd((u64)m1, (u64)m2) != 1) continue;
        i64 a1 = (i64)(rnd() % (u64)m1), a2 = (i64)(rnd() % (u64)m2), x = crt2(a1, m1, a2, m2); assert(x >= 0 && x < m1 * m2 && x % m1 == a1 && x % m2 == a2);
    }
    init_fact(200000); u64 pascal[40][40] = {{0}}; for (int n = 0; n < 40; n++) { pascal[n][0] = 1; for (int k = 1; k <= n; k++) pascal[n][k] = pascal[n - 1][k - 1] + (k < n ? pascal[n - 1][k] : 0); }
    for (int n = 0; n < 40; n++) for (int k = 0; k <= n; k++) assert(ncr(n, k) == pascal[n][k] % P);
    printf("randomized checks passed: gcd, extended Euclid (Bezout identity), powmod, modular inverses (Euclid and Fermat), fast-doubling Fibonacci, CRT, C(n,k) mod p vs Pascal's triangle\n\n");

    /* overflow: why mulmod needs 128-bit intermediates */
    u64 m = (1ULL << 62) + 135, a = m - 3, b = m - 5;                                 /* any two residues of a ~62-bit modulus */
    u64 wrong = a * b % m, right = mulmod(a, b, m);                                    /* (m-3)(m-5) = 15 mod m  */
    printf("modulus ~2^62: (m-3)*(m-5) mod m  with 64-bit arithmetic = %llu,  with 128-bit intermediate = %llu  (correct answer is 15)\n", (unsigned long long)wrong, (unsigned long long)right);

    /* Fermat's test is fooled by Carmichael numbers; Miller-Rabin is not */
    int fooled = 0; for (u64 a2 = 2; a2 < 561; a2++) if (gcd(a2, 561) == 1 && powmod(a2, 560, 561) == 1) fooled++;
    printf("561 = 3*11*17 is composite, yet a^560 = 1 (mod 561) for all %d bases a coprime to it; Miller-Rabin says prime? %d\n", fooled, miller_rabin(561));

    /* fast Fibonacci */
    u64 f, g; fib_pair(90, ~0ULL >> 1, &f, &g); printf("F(90) = %llu (fits in 64 bits),  F(10^18) mod 10^9+7 = ", (unsigned long long)f); fib_pair(1000000000000000000ULL, P, &f, &g); printf("%llu  (computed with ~60 squarings)\n", (unsigned long long)f);

    /* toy RSA, to connect with the cryptography notes: INSECURE, 62-bit modulus */
    u64 p = 0, q = 0, st = 3000000019ULL; while (!miller_rabin(st)) st += 2; p = st; st += 1000; if (!(st & 1)) st++; while (!miller_rabin(st)) st += 2; q = st;
    u64 n = p * q, phi = (p - 1) * (q - 1), e = 65537; i64 d = inv_euclid((i64)e, (i64)phi); assert(d > 0);
    u64 msg = 1234567890123456789ULL % n, ct = powmod(msg, e, n), back = powmod(ct, (u64)d, n); assert(back == msg);
    printf("toy RSA (insecure!): p = %llu, q = %llu, n = %llu (%d bits), e = 65537, d = %lld;  m = %llu -> c = %llu -> m = %llu\n",
           (unsigned long long)p, (unsigned long long)q, (unsigned long long)n, 64 - __builtin_clzll(n), (long long)d, (unsigned long long)msg, (unsigned long long)ct, (unsigned long long)back);
    return 0;
}
