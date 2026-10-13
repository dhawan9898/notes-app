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
