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
