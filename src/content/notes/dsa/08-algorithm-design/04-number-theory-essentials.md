---
title: Number Theory Essentials
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 4
level: advanced
summary: "Euclid's algorithm and Bezout coefficients, modular exponentiation and inverses, overflow-safe 128-bit mulmod, bit-array and segmented sieves, deterministic Miller-Rabin for all 64-bit integers, fast-doubling Fibonacci, CRT, binomials mod p and a toy RSA — each checked, with the two overflow bugs the tests caught."
sources:
  - label: "Knuth — The Art of Computer Programming, Vol. 2, §4.5 (Rational arithmetic, Euclid's algorithm) and §4.5.4 (Factoring into primes; primality tests)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 31 (Number-Theoretic Algorithms)"
  - label: "Miller — Riemann's Hypothesis and Tests for Primality (J. Computer and System Sciences 13, 1976)"
  - label: "Rabin — Probabilistic Algorithm for Testing Primality (J. Number Theory 12, 1980)"
  - label: "Sorenson & Webster — Strong Pseudoprimes to Twelve Prime Bases (Mathematics of Computation 86, 2017)"
  - label: "Rivest, Shamir, Adleman — A Method for Obtaining Digital Signatures and Public-Key Cryptosystems (CACM 21(2), 1978)"
  - label: "Hardy & Wright — An Introduction to the Theory of Numbers, 6th ed."
  - label: "Warren — Hacker's Delight, 2nd ed. (Chapter 10, integer division by constants; Chapter 11, number systems)"
---

Integer arithmetic with a modulus is the substrate of hashing, random number generators, cyclic buffers, error-correcting codes, and all of public-key cryptography. The algorithms are short — most are under ten lines — but each hides a trap: overflow in the intermediate product, a negative remainder, a probabilistic test that the wrong number fools. This note builds the standard toolbox in C, verifies every piece, and records two overflow bugs that the test suite caught in the *first* version of this code.

```c
#ifndef NT_H
#define NT_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef unsigned __int128 u128;

/* ---- gcd and extended Euclid: g = gcd(a, b) = a*x + b*y (Bezout coefficients) ---- */
static inline u64 gcd(u64 a, u64 b) { while (b) { u64 t = a % b; a = b; b = t; } return a; }
static inline i64 ext_gcd(i64 a, i64 b, i64 *x, i64 *y) {
    if (!b) { *x = 1; *y = 0; return a; }
    i64 x1, y1, g = ext_gcd(b, a % b, &x1, &y1); *x = y1; *y = x1 - (a / b) * y1; return g;
}
/* ---- overflow-safe modular multiplication and exponentiation for 64-bit moduli ---- */
static inline u64 mulmod(u64 a, u64 b, u64 m) { return (u64)((u128)a * b % m); }
static inline u64 powmod(u64 b, u64 e, u64 m) { u64 r = 1 % m; b %= m; while (e) { if (e & 1) r = mulmod(r, b, m); b = mulmod(b, b, m); e >>= 1; } return r; }
/* ---- modular inverse via extended Euclid (any modulus coprime to a) or Fermat's little theorem (prime modulus) ---- */
static inline i64 inv_euclid(i64 a, i64 m) {
    i64 x, y; a %= m; if (a < 0) a += m;                         /* a in [0, m): never compute (a % m) + m, which can overflow for m near 2^63 */
    i64 g = ext_gcd(a, m, &x, &y); if (g != 1) return -1;
    x %= m; if (x < 0) x += m;                                   /* same care for the coefficient: x is in (-m, m) */
    return x;
}
static inline u64 inv_fermat(u64 a, u64 p) { return powmod(a, p - 2, p); }

/* ---- Miller-Rabin, deterministic for every 64-bit integer with the first twelve prime bases (Sorenson & Webster 2015: bases 2..37 suffice below 3.3e24) ---- */
static inline int miller_rabin(u64 n) {
    if (n < 2) return 0;
    static const u64 bases[12] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
    for (int i = 0; i < 12; i++) { if (n % bases[i] == 0) return n == bases[i]; }
    u64 d = n - 1; int r = 0; while (!(d & 1)) { d >>= 1; r++; }
    for (int i = 0; i < 12; i++) {
        u64 x = powmod(bases[i], d, n);
        if (x == 1 || x == n - 1) continue;
        int composite = 1; for (int j = 1; j < r; j++) { x = mulmod(x, x, n); if (x == n - 1) { composite = 0; break; } }
        if (composite) return 0;
    }
    return 1;
}
/* ---- fast doubling Fibonacci: F(2k) = F(k) (2F(k+1) - F(k)),  F(2k+1) = F(k)^2 + F(k+1)^2   (mod m), O(log n) ---- */
static inline void fib_pair(u64 n, u64 m, u64 *f, u64 *g) {                     /* returns F(n), F(n+1) */
    if (!n) { *f = 0; *g = 1 % m; return; }
    u64 a, b; fib_pair(n >> 1, m, &a, &b);
    u64 c = mulmod(a, (2 * b % m + m - a) % m, m), d = (mulmod(a, a, m) + mulmod(b, b, m)) % m;
    if (n & 1) { *f = d; *g = (c + d) % m; } else { *f = c; *g = d; }
}
/* ---- Chinese remainder theorem for two coprime moduli: the unique x mod m1*m2 with x = a1 (m1), x = a2 (m2) ---- */
static inline i64 crt2(i64 a1, i64 m1, i64 a2, i64 m2) {
    i64 inv = inv_euclid(m1 % m2, m2); i64 t = (((a2 - a1) % m2 + m2) % m2) * inv % m2; return a1 + m1 * t;
}
#endif
```

## Greatest common divisor and Bezout's identity

**Euclid's algorithm** computes `gcd(a, b)` by replacing `(a, b)` with `(b, a mod b)` until `b = 0`. It takes `O(log min(a, b))` steps; the worst case is consecutive Fibonacci numbers (Lamé's theorem, 1844 — the first analysis of an algorithm's running time). The **extended** algorithm also returns integers `x, y` with `a·x + b·y = gcd(a, b)` (Bezout's identity), by back-substituting through the recursion. These coefficients are what you need for:

- **Modular inverses.** `a⁻¹ mod m` exists iff `gcd(a, m) = 1`, and it is the `x` in `a·x + m·y = 1`. This works for *any* modulus, prime or not.
- **Fermat's little theorem** gives an alternative when `p` is prime: `a⁻¹ ≡ a^(p−2) (mod p)` — one modular exponentiation.
- **Linear Diophantine equations** `a·x + b·y = c` (solvable iff `gcd(a, b) | c`) and the **Chinese remainder theorem**.

**The Chinese remainder theorem** says that for coprime moduli `m₁`, `m₂` the pair of congruences `x ≡ a₁ (m₁)`, `x ≡ a₂ (m₂)` has exactly one solution modulo `m₁m₂`; the code builds it from one inverse. CRT is how big-number RSA decryption is sped up by about 4× (work modulo each prime separately), and how residue-number-system arithmetic splits a large computation into small independent ones.

## Fast modular exponentiation and the overflow trap

`b^e mod m` by repeated squaring takes `O(log e)` multiplications: square the base each step and multiply it into the result when the corresponding bit of `e` is set. Everything else — Fermat inverses, Diffie–Hellman, RSA, primality tests — builds on it.

The trap is that the intermediate product `a·b` of two residues below a 62-bit modulus is up to 124 bits and overflows 64-bit arithmetic *silently* — `uint64_t` wraps, and the answer is plausible and wrong. GCC and Clang provide `unsigned __int128` on 64-bit targets; a single `(u128)a * b % m` is exact. (Without 128-bit support use `a·b mod m` by shift-and-add, or Montgomery multiplication, which also avoids the slow 128-bit division and is what real big-integer libraries use.)

## Primes: sieves

The **sieve of Eratosthenes** marks multiples of each prime starting at its square, in `O(n log log n)` time. Two standard improvements, both implemented below:

1. **Odd numbers only, one bit each**: half the numbers and one-eighth of the space of a byte array — 62.5 MB for all numbers up to 10⁹.
2. **Segmented sieve**: sieve a window of `[lo, hi)` at a time using the primes up to `√hi`. The working set fits in the L1/L2 cache instead of streaming through all of memory, and it needs only `O(√n + block)` memory, which makes ranges like `[10¹², 10¹²+10⁷]` feasible. (In the measurement below the byte-per-number segmented sieve is only about as fast as the bit-packed plain sieve at 10⁹ — the bit-packing already cut memory traffic by 16× — so its real advantage here is memory, and speed for larger limits.)

For a single number, trial division up to `√n` costs `O(√n)` — hopeless for 64-bit inputs. That calls for a primality *test*.

## Miller–Rabin primality

For an odd `n`, write `n − 1 = 2^r · d` with `d` odd. If `n` is prime then for any base `a` either `a^d ≡ 1` or `a^(2^j·d) ≡ −1 (mod n)` for some `0 ≤ j < r`, because the only square roots of 1 modulo a prime are ±1. A composite `n` fails this test for at least 3/4 of all bases, so testing `k` random bases leaves error probability at most `4^−k`. The older **Fermat test** (`a^(n−1) ≡ 1`) is fooled by **Carmichael numbers** — composite `n` for which *every* coprime base passes; the smallest, 561 = 3·11·17, is shown below passing for all 319 coprime bases. Miller–Rabin has no analogue of Carmichael numbers: its worst case is a quarter of the bases, not all of them.

For a *bounded* range the test becomes **deterministic** with a fixed base set: the first twelve primes (2…37) are sufficient for every `n < 3.3·10²⁴` (Sorenson & Webster), hence for all 64-bit integers. The code uses exactly those bases. It is the right primality test for any integer that fits in a machine word: about 0.5 µs per number here.

## Fast Fibonacci and linear recurrences

`F(n)` for huge `n` by the **fast-doubling identities** `F(2k) = F(k)(2F(k+1) − F(k))` and `F(2k+1) = F(k)² + F(k+1)²` needs `O(log n)` steps — about 60 for `n = 10¹⁸`. The general method is **matrix exponentiation**: any linear recurrence of order `k` is a `k×k` matrix power, solved in `O(k³ log n)`.

## Binomial coefficients modulo a prime

Counting problems ask for `C(n, k) mod p` for many `(n, k)`. Precompute factorials and inverse factorials modulo `p` once (`O(n)`, with a single Fermat inverse and a downward sweep for the rest), then each query is two multiplications. The code verifies the table against Pascal's triangle.

## Verification

```c
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
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined arith.c -o arith && ./arith
randomized checks passed: gcd, extended Euclid (Bezout identity), powmod, modular inverses (Euclid and Fermat), fast-doubling Fibonacci, CRT, C(n,k) mod p vs Pascal's triangle

modulus ~2^62: (m-3)*(m-5) mod m  with 64-bit arithmetic = 16890,  with 128-bit intermediate = 15  (correct answer is 15)
561 = 3*11*17 is composite, yet a^560 = 1 (mod 561) for all 319 bases a coprime to it; Miller-Rabin says prime? 0
F(90) = 2880067194370816120 (fits in 64 bits),  F(10^18) mod 10^9+7 = 209783453  (computed with ~60 squarings)
toy RSA (insecure!): p = 3000000019, q = 3000001031, n = 9000003150000019589 (63 bits), e = 65537, d = 3224851211232623333;  m = 1234567890123456789 -> c = 915840995674856281 -> m = 1234567890123456789
```

The checks run thousands of random cases against naive implementations: `gcd` against trial division, Bezout's identity for the extended algorithm (including negative inputs), `powmod` against repeated multiplication, inverses by both methods, fast-doubling Fibonacci against the iterative definition, CRT against the congruences, and `C(n,k) mod p` against Pascal's triangle. The rest of the output:

- **Overflow demonstration.** For a modulus around 2⁶² the product `(m−3)·(m−5)` is `15` modulo `m`. With plain 64-bit arithmetic the answer is wrong; with the 128-bit intermediate it is `15`.
- **Fermat fooled, Miller–Rabin not.** 561 passes the Fermat test for all 319 coprime bases yet Miller–Rabin correctly reports composite.
- **Known values.** `F(90) = 2 880 067 194 370 816 120` fits in 64 bits, and `F(10¹⁸) mod (10⁹+7) = 209 783 453` takes about sixty squarings.
- **A toy RSA round trip.** Two 32-bit primes found with Miller–Rabin give a 63-bit modulus; `e = 65537`, `d = e⁻¹ mod φ(n)`, and encrypting then decrypting recovers the message. This is **not secure** — a 63-bit modulus is factored instantly — and it uses no padding; real RSA uses ≥ 2048-bit moduli, OAEP/PSS padding and constant-time big-integer arithmetic. It is here to show where the pieces (primality testing, extended Euclid, modular exponentiation) fit; see the *IPsec and IKEv2* and *SSH, PAM, sudo and auditd* notes for how deployed protocols use Diffie–Hellman, RSA and elliptic-curve variants.

### Two bugs the tests caught

1. **`ext_gcd` was right but `inv_euclid` overflowed.** The first version computed `((x % m) + m) % m` to make the coefficient non-negative. For `m` near 2⁶³ the intermediate `x % m + m` exceeds `2⁶³` — **signed overflow, undefined behavior**, which UBSan flagged at once during the RSA round trip (and the result was negative, tripping the `d > 0` assertion). The corrected form is `x %= m; if (x < 0) x += m;`, which never exceeds `m`. The same pattern, `(a % m + m) % m`, is the universal "make it non-negative" idiom and is only safe when `2m` fits in the type.
2. **The simple modular multiplication overflowed** for 62-bit moduli (shown above), which is why `mulmod` exists.

In both cases a test that exercises values *near the type's limits* found what small tests could not. Number-theory code should always be tested at the boundary of its integer type.

## Sieve and primality measurements

```c
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
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined sieve.c -o sieve_asan -lm && ./sieve_asan | head -9 && gcc -std=gnu11 -O2 -Wall -Wextra sieve.c -o sieve -lm && ./sieve | tail -3
           x  pi(x) known    bit sieve | segmented (32 KB blocks)
         100           25           25 | 25
       10000         1229         1229 | 1229
     1000000        78498        78498 | 78498
    10000000       664579       664579 | 664579
   100000000      5761455      5761455 | 5761455

Miller-Rabin (12 fixed bases) agrees with the sieve on every integer 0..2000000
561 (Carmichael): 0;  3215031751 (strong pseudoprime to bases 2,3,5,7): 0;  2^61-1 (Mersenne prime): 1;  2^64-59 (largest 64-bit prime): 1;  2^64-1: 0

primes below 10^9: 50847534 (known: 50847534).  plain bit sieve (62.5 MB): 3.36 s;  segmented, 32 KB working set: 1.69 s
Miller-Rabin: the first 1000 primes above 10^18 found by testing 40814 numbers in 18.8 ms (460 ns per test; density about 1/ln(10^18) = 1/41.4)
```

The first block is the sanitizer build: both sieves reproduce the known prime-counting values π(x) (25, 1229, 78 498, 664 579, 5 761 455 for x = 10², 10⁴, 10⁶, 10⁷, 10⁸), and Miller–Rabin agrees with the sieve on **every** integer from 0 to two million; the classic traps (561, the strong pseudoprime 3 215 031 751, 2⁶⁴−1) are classified correctly, and the Mersenne prime 2⁶¹−1 and the largest 64-bit prime 2⁶⁴−59 are accepted. The last lines are from the optimized build: counting primes below 10⁹ (π = 50 847 534; a few seconds for either sieve) and testing numbers just above 10¹⁸, where a prime occurs once in about `ln(10¹⁸) ≈ 41` numbers — exactly the density the run finds.

## Beyond this note

- **Factoring**: trial division, Pollard's rho (`O(n^¼)`, practical to ~10¹⁸), the quadratic sieve and the number field sieve (the algorithm behind factoring records). The difficulty of factoring large semiprimes is what RSA's security rests on.
- **Euler's totient** `φ(n)` and the **Möbius function**, computable for all `n ≤ N` in one sieve pass; used in counting problems and Euler's theorem `a^φ(m) ≡ 1`.
- **Discrete logarithm**: baby-step giant-step in `O(√m)`; the hard problem underlying Diffie–Hellman.
- **Montgomery multiplication** and Barrett reduction for fast modular multiplication, and **constant-time** implementations for cryptography (branches and table lookups on secret data leak through timing).
- **Number-theoretic transform (NTT)**: an FFT in modular arithmetic, for exact polynomial and big-integer multiplication in `O(n log n)`.

## Pitfalls

- **Overflow in `a*b % m`** for `m > 2³²` (use `unsigned __int128`, Montgomery, or `fmod`-style tricks); **overflow in `(a % m + m) % m`** for `m > 2⁶²`.
- **C's `%` is the remainder, not the mathematical modulus**: `-7 % 5 == -2`. Normalise with `r < 0 ? r + m : r`.
- **Signed shifts and sizes**: use `1ULL << k` for masks of 32 or more bits.
- **Using the Fermat test** (or a few random Miller–Rabin rounds) where an adversary picks the number: Carmichael numbers and crafted pseudoprimes exist; use the deterministic base set (or at least a strong random test) for untrusted input.
- **Modulus not prime**: Fermat inverses and Fermat's little theorem require a prime modulus; use extended Euclid otherwise.
- **Rolling your own cryptography**: the toy RSA above has no padding and a tiny modulus; never ship it. Use a vetted library (OpenSSL, libsodium) with constant-time implementations.
- **Zero and one**: `gcd(0, 0)`, `powmod` with modulus 1 (everything is 0), and `miller_rabin(1)` are the edge cases that unit tests should cover.

## Key takeaways

1. Euclid's algorithm is `O(log n)`; its extended form gives modular inverses for any coprime modulus and underlies CRT.
2. Modular exponentiation takes `O(log e)` multiplications — but multiplication of 62-bit residues needs a 128-bit intermediate.
3. The Fermat test is fooled by Carmichael numbers; Miller–Rabin with the first twelve prime bases is a *deterministic* primality test for all 64-bit integers at ~0.5 µs per test.
4. Segmented, bit-packed sieves enumerate primes to 10⁹ in seconds with a cache-sized working set.
5. Precomputed factorial and inverse-factorial tables turn binomials modulo a prime into two multiplications.
6. Test number-theoretic code near the limits of the integer type: both bugs in this note appeared only for values close to 2⁶³.
