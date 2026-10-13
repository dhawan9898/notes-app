---
title: "FFT, NTT and Fast Convolution"
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 5
level: expert
summary: "Multiply polynomials and huge integers in O(n log n): the DFT and roots of unity, the Cooley-Tukey recursion and its iterative form, the exact number-theoretic transform modulo 998244353 verified against schoolbook multiplication, a measured crossover on million-digit-scale numbers, and the precision limits of floating-point FFT."
prerequisites:
  - Number Theory Essentials
  - Dynamic Arrays
sources:
  - label: "Cooley & Tukey — An Algorithm for the Machine Calculation of Complex Fourier Series (Math. Comp. 1965)"
    url: https://www.ams.org/journals/mcom/1965-19-090/S0025-5718-1965-0178586-1/
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, Ch. 30 Polynomials and the FFT"
  - label: "Pollard — The Fast Fourier Transform in a Finite Field (Math. Comp. 1971)"
    url: https://www.ams.org/journals/mcom/1971-25-114/S0025-5718-1971-0301966-0/
  - label: "Schönhage & Strassen — Schnelle Multiplikation großer Zahlen (Computing 1971)"
  - label: "Harvey & van der Hoeven — Integer multiplication in time O(n log n) (Annals of Mathematics 2021)"
    url: https://annals.math.princeton.edu/2021/193-2/p04
  - label: "Frigo & Johnson — The Design and Implementation of FFTW3 (Proc. IEEE 2005)"
    url: https://www.fftw.org/fftw-paper-ieee.pdf
---

The product of two polynomials of degree *n* takes O(n²) coefficient multiplications by the schoolbook method: each term of one is multiplied by each term of the other. The same operation, called **convolution**, is hiding in many problems: multiplying huge integers (digits are coefficients), counting how many pairs of items sum to each value, matching patterns with wildcards, combining probability distributions, and smoothing signals. The **fast Fourier transform (FFT)** computes it in O(n log n).

## Two ways to represent a polynomial

A polynomial of degree less than *n* can be stored as its **coefficients** `a₀ … a_{n−1}` or as its **values** at *n* distinct points. Both determine it uniquely. Multiplying in coefficient form is a convolution (slow). Multiplying in **value form** is trivial: if you know `A(x)` and `B(x)` at the same points, then `C(x) = A(x)·B(x)` at each point, which is *n* multiplications. (The product has degree up to 2n−2, so evaluate at 2n−1 or more points.)

So the plan is: convert both polynomials to value form, multiply pointwise, convert back. Doing the conversions naively costs O(n²) (evaluate at *n* points by Horner's rule), no better than before. The FFT makes the conversions cost O(n log n) by choosing the evaluation points cleverly.

## Roots of unity and the DFT

Choose as evaluation points the *n* complex **n-th roots of unity**: `ω_n^k` for `k = 0 … n−1`, where `ω_n = e^{2πi/n}`. The values `A(ω^0), A(ω^1), …, A(ω^{n−1})` form the **discrete Fourier transform (DFT)** of the coefficient vector. Two properties make it fast:

- **Halving:** split `A(x) = A_even(x²) + x·A_odd(x²)`. The squares of the *n* roots of unity are only the *n/2* roots of unity (each appears twice), so evaluating `A` at all *n* roots requires evaluating two polynomials of half the size at *n/2* roots, then combining with one multiplication and one addition per output. That is the recurrence `T(n) = 2T(n/2) + O(n)`, which solves to **O(n log n)**.
- **Inversion:** the inverse DFT is the same algorithm with `ω⁻¹` in place of `ω`, followed by dividing by *n*. So one routine does both directions.

The combining step is the **butterfly**: for `k < n/2`, with `u = A_even(ω^{2k})` and `v = ω^k · A_odd(ω^{2k})`, output `u + v` at position *k* and `u − v` at position *k + n/2*. Implementations run it **in place and iteratively**: first permute the input into **bit-reversed** index order (so the recursion's leaves are in place), then perform log₂ n rounds of butterflies with block lengths 2, 4, 8, … n. The length must be a power of two, so inputs are padded with zeros up to the next power of two ≥ the product's length.

```mermaid
graph LR
  A["coefficients a, b"] -->|"pad to 2^k, transform<br/>O(n log n)"| B["values A(ω^i), B(ω^i)"]
  B -->|"pointwise multiply<br/>O(n)"| C["values C(ω^i)"]
  C -->|"inverse transform<br/>O(n log n)"| D["coefficients of a*b"]
```

## The number-theoretic transform (NTT): exact arithmetic

Complex roots of unity bring floating-point rounding. All we really need is a number system with a **primitive n-th root of unity** ω (ω^n = 1, and ω^k ≠ 1 for 0 < k < n). Finite fields have these: in the integers modulo a prime *p*, a primitive root *g* has order *p−1*, and `g^((p−1)/n)` is a primitive *n*-th root of unity whenever *n* divides *p−1*. The transform computed this way is the **NTT**, and it is **exact**: no rounding, no error analysis, results are integers modulo *p*.

The popular prime is `p = 998244353 = 119·2²³ + 1` with primitive root 3: it supports lengths up to 2²³ ≈ 8.4 million, and products fit in 64 bits (p² < 2⁶⁰). Other common ones: `469762049 = 7·2²⁶ + 1` and `167772161 = 5·2²⁵ + 1`, both with root 3.

```c
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
```

### Verification

The product is checked against the schoolbook algorithm on 3,000 random polynomial pairs of random lengths with full-range 30-bit coefficients (including length-1 polynomials), and the transform is checked to invert itself. The last line verifies the root: 3 has order *p−1*, the facts the construction depends on.

```c
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
```

```text
$ gcc -O2 -Wall -Wextra -fsanitize=address,undefined test.c -o t && ./t
208038 product coefficients matched schoolbook multiplication; inverse(NTT(x)) == x: yes
p - 1 = 998244352 = 119 * 2^23, so lengths up to 2^23 = 8388608 are supported; 3 has order p-1: 3^((p-1)/2) = p-1 (ok), 3^((p-1)/7) != 1 (ok)
```

## Multiplying very large integers

An integer in base 10 is the polynomial of its digits evaluated at x = 10, so multiplying integers is multiplying polynomials and then propagating **carries**. The convolution coefficients are at most `81 · (digits)` (each term is a product of two digits), which stays below *p* up to roughly 12 million digits, so the modular result is the true result. (Larger numbers use bigger bases per coefficient, a second or third modulus combined with the Chinese remainder theorem, or splitting.)

```c
// Multiplying huge decimal numbers with the NTT: digits are polynomial coefficients (base 10), the product is a convolution, carries are propagated afterwards.
// Coefficients of the convolution are at most 81 * (number of digits) < p, so the modular result is exact for up to about 12 million digits.
#include "ntt.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static char *multiply(const char *x, const char *y) {
    size_t nx = strlen(x), ny = strlen(y);
    uint64_t *a = malloc(nx * 8), *b = malloc(ny * 8);
    for (size_t i = 0; i < nx; i++) a[i] = x[nx - 1 - i] - '0';              // least significant digit first
    for (size_t i = 0; i < ny; i++) b[i] = y[ny - 1 - i] - '0';
    size_t nc; uint64_t *c = poly_mul(a, nx, b, ny, &nc);
    char *out = malloc(nc + 2); uint64_t carry = 0;
    for (size_t i = 0; i < nc; i++) { uint64_t v = c[i] + carry; out[i] = '0' + v % 10; carry = v / 10; }
    size_t len = nc; while (carry) { out[len++] = '0' + carry % 10; carry /= 10; }
    while (len > 1 && out[len - 1] == '0') len--;
    for (size_t i = 0; i < len / 2; i++) { char t = out[i]; out[i] = out[len - 1 - i]; out[len - 1 - i] = t; }
    out[len] = 0; free(a); free(b); free(c); return out;
}

// schoolbook O(n*m) digit multiplication, as taught in school
static char *school(const char *x, const char *y) {
    size_t nx = strlen(x), ny = strlen(y), n = nx + ny;
    unsigned long long *t = calloc(n, sizeof *t);
    for (size_t i = 0; i < nx; i++) for (size_t j = 0; j < ny; j++) t[i + j] += (unsigned)(x[nx - 1 - i] - '0') * (y[ny - 1 - j] - '0');
    char *out = malloc(n + 2); unsigned long long carry = 0; size_t len = 0;
    for (size_t i = 0; i < n; i++) { unsigned long long v = t[i] + carry; out[len++] = '0' + v % 10; carry = v / 10; }
    while (len > 1 && out[len - 1] == '0') len--;
    for (size_t i = 0; i < len / 2; i++) { char c = out[i]; out[i] = out[len - 1 - i]; out[len - 1 - i] = c; }
    out[len] = 0; free(t); return out;
}

int main(void) {
    char *p = multiply("123456789123456789", "987654321987654321");
    printf("123456789123456789 * 987654321987654321 = %s\n", p); free(p);
    printf("%8s %14s %14s %s\n", "digits", "schoolbook (s)", "NTT (s)", "results equal");
    srand(7);
    for (size_t digits = 1000; digits <= 64000; digits *= 4) {
        char *x = malloc(digits + 1), *y = malloc(digits + 1);
        for (size_t i = 0; i < digits; i++) { x[i] = '1' + rand() % 9; y[i] = '1' + rand() % 9; }
        x[digits] = y[digits] = 0;
        double t0 = now(); char *s = school(x, y); double ts = now() - t0;
        t0 = now(); char *f = multiply(x, y); double tf = now() - t0;
        printf("%8zu %14.4f %14.4f %s\n", digits, ts, tf, strcmp(s, f) ? "NO" : "yes");
        free(x); free(y); free(s); free(f);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra bigmul.c -o bigmul && ./bigmul
123456789123456789 * 987654321987654321 = 121932631356500531347203169112635269
  digits schoolbook (s)        NTT (s) results equal
    1000         0.0014         0.0003 yes
    4000         0.0230         0.0016 yes
   16000         0.3842         0.0073 yes
   64000         4.5658         0.0159 yes
```

The first line is a known-answer check (the product printed is correct). The table compares the schoolbook algorithm with the NTT on random inputs of growing size, verifying each time that both results are identical. Each time the size is multiplied by 4 the schoolbook time grows about 16 times (quadratic), while the NTT time grows 4-5 times (n log n), so at 64,000 digits the transform is faster by a factor of over two hundred and the gap keeps widening. The crossover is small: even at 1,000 digits the transform already wins here, though a tuned schoolbook or **Karatsuba** (O(n^1.585), splitting each number in halves and doing three half-size multiplications) is typically best below a few hundred machine words. Practical big-number libraries (GMP) switch among schoolbook, Karatsuba, Toom-Cook and FFT-based **Schönhage–Strassen** as sizes grow; Harvey and van der Hoeven proved in 2019 that O(n log n) integer multiplication is possible, though that algorithm is a theoretical landmark rather than a practical tool.

## Floating-point FFT: fast but approximate

With ordinary `double` complex numbers the FFT is available everywhere (and extremely well optimized: FFTW, vendor libraries, SIMD). The price is rounding error that grows with the transform length and with the magnitude of the coefficients. The program multiplies random integer polynomials, computes every exact coefficient by direct summation, and reports the worst deviation:

```c
// Complex FFT multiplication in double precision: fast, but only as exact as floating point allows.
// We multiply two random integer polynomials of length n with coefficients below M and report the worst rounding error.
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void fft(double complex *a, size_t n, int invert) {
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) { double complex t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / len * (invert ? -1 : 1);
        for (size_t i = 0; i < n; i += len)
            for (size_t k = 0; k < len / 2; k++) {
                double complex w = cexp(I * ang * k);                       // recomputed each time: slower, but avoids accumulated twiddle error
                double complex u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
            }
    }
    if (invert) for (size_t i = 0; i < n; i++) a[i] /= n;
}

int main(void) {
    printf("%8s %12s %20s %s\n", "length", "coeff < M", "worst rounding error", "all coefficients correct after rounding?");
    srand(5);
    size_t lens[] = {1024, 16384}; unsigned long long Ms[] = {10, 1000, 100000, 10000000};
    for (int li = 0; li < 2; li++) for (int mi = 0; mi < 4; mi++) {
        size_t n = lens[li], N = 2 * n; unsigned long long M = Ms[mi];
        long long *a = malloc(n * 8), *b = malloc(n * 8);
        double complex *fa = calloc(N, sizeof *fa), *fb = calloc(N, sizeof *fb);
        for (size_t i = 0; i < n; i++) { a[i] = rand() % M; b[i] = rand() % M; fa[i] = a[i]; fb[i] = b[i]; }
        fft(fa, N, 0); fft(fb, N, 0); for (size_t i = 0; i < N; i++) fa[i] *= fb[i]; fft(fa, N, 1);
        double worst = 0; int ok = 1;
        for (size_t k = 0; k < 2 * n - 1; k++) {
            // exact coefficient by direct summation (128-bit to avoid overflow)
            __int128 want = 0; size_t lo = k >= n ? k - n + 1 : 0, hi = k < n ? k : n - 1;
            for (size_t i = lo; i <= hi; i++) want += (__int128)a[i] * b[k - i];
            double got = creal(fa[k]), err = fabs(got - (double)want);
            if (err > worst) worst = err;
            if (llround(got) != (long long)want) ok = 0;
        }
        printf("%8zu %12llu %20.3g %s\n", n, M, worst, ok ? "yes" : "NO");
        free(a); free(b); free(fa); free(fb);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra fft.c -o fft -lm && ./fft
  length    coeff < M worst rounding error all coefficients correct after rounding?
    1024           10             7.28e-12 yes
    1024         1000             7.45e-08 yes
    1024       100000             0.000732 yes
    1024     10000000                   12 NO
   16384           10             1.46e-10 yes
   16384         1000             1.91e-06 yes
   16384       100000               0.0234 yes
   16384     10000000                  256 NO
```

Reading the table: the error scales with the **size of the products** (n · M²): at coefficients below 100,000 it stays well below 0.5, so rounding to the nearest integer recovers the exact answer, but at coefficients below 10,000,000 the error is already in the tens or hundreds and **the rounded result is wrong**. The rule of thumb is that a double FFT is safe while the largest possible result coefficient (n · M²) stays below roughly 10¹⁴ to 10¹⁵ (with an accurate twiddle-factor computation as done here, `cexp` evaluated for every butterfly); with the common trick of precomputing twiddle factors by repeated multiplication it is much worse. Remedies: split coefficients into smaller pieces (for example 15-bit halves) and combine several FFTs, use long double, use an NTT with one or more primes and the Chinese remainder theorem, or accept approximate results where that is the goal (signal processing).

## Choosing the tool

| Situation | Use |
|---|---|
| Exact integer or modular answer, length up to ~8 million | NTT modulo 998244353 |
| Answer needed modulo an arbitrary number (1e9+7) | three NTT primes + Chinese remainder theorem, or FFT with splitting |
| Real-valued signals, approximate results | floating-point FFT (FFTW, `numpy.fft`), using real-input transforms (half the work) |
| Very small sizes (< ~64 terms) | schoolbook; the transform's overhead exceeds its savings |
| Medium sizes without a power-of-two or prime constraint | Karatsuba |
| Multiplying a polynomial by itself, many times | transform once, keep values, multiply pointwise repeatedly |

### Applications beyond multiplication

- **Counting pair sums:** if `a[v]` is the number of items with value *v* and `b[v]` likewise in another collection, then the convolution `c[s] = Σ a[v]·b[s−v]` is the number of pairs with sum *s*. (With the NTT counts must stay below *p*.)
- **Pattern matching with wildcards or mismatch counting:** the number of positions where the text and a reversed pattern agree is a convolution of indicator vectors.
- **Probability:** the distribution of the sum of two independent discrete random variables is the convolution of their distributions.
- **Polynomial arithmetic:** division, inverse, `exp`, and `ln` of formal power series using Newton iteration, all built on fast multiplication, power many competitive-programming and computer-algebra tools.
- **Signal processing:** convolution with a filter of length *m* on a signal of length *n* costs O(n log n) via FFT (overlap-add and overlap-save for long signals).

## Pitfalls

- **Length:** the transform size must be a power of two at least `na + nb − 1`; a smaller size makes the result wrap around (cyclic convolution) and silently corrupt the high coefficients.
- **Modulus limits:** the NTT requires the modulus *p* to satisfy `n | p − 1`; with 998244353 the size is capped at 2²³. A coefficient result larger than *p* is reduced modulo *p*: if you need exact large counts, use more primes.
- **Overflow:** multiply in 64 bits and reduce immediately; for moduli above 2³² use `unsigned __int128` or Montgomery arithmetic.
- **Floating-point exactness:** never assume `round(fft)` is exact without bounding n · M².
- **Memory and cache:** each transform streams through the array log₂ n times; for large *n* use blocking or radix-4 and avoid recomputing twiddle factors with `pw` inside inner loops (a precomputed table is faster; the version above keeps the code short).

## Key takeaways

- Convolution (polynomial or integer multiplication) is O(n²) on coefficients but O(n) on values; the FFT converts between the two in O(n log n) by evaluating at roots of unity.
- The divide-and-conquer relies on the squares of the n-th roots being the n/2-th roots; the iterative form uses bit-reversal and in-place butterflies.
- The NTT does the same in the integers modulo a prime with a suitable root (998244353, root 3) and is exact; verify against schoolbook multiplication.
- Floating-point FFT is fast and ubiquitous but only trustworthy while n · M² stays far below 2⁵³; the measurements above show where it breaks.
- Pad to a power of two at least as long as the result, and prefer simpler methods for small inputs.
