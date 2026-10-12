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
