#include <stdio.h>

/* Count how many times the innermost statement runs for n = 64 and n = 128, then read the growth. */
static long a(long n) { long c = 0; for (long i = 0; i < n; i++) c++; return c; }                                   /* A */
static long b(long n) { long c = 0; for (long i = 1; i < n; i *= 2) c++; return c; }                                /* B */
static long c3(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 0; j < n; j++) c++; return c; }     /* C */
static long d(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = i; j < n; j++) c++; return c; }      /* D */
static long e(long n) { long c = 0; for (long i = 1; i < n; i *= 2) for (long j = 0; j < i; j++) c++; return c; }   /* E */
static long f(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 1; j < n; j *= 2) c++; return c; }   /* F */
static long g(long n) { long c = 0; for (long i = 0; i < n; i++) { c++; } for (long j = 0; j < n; j++) { c++; } return c; } /* G */

int main(void) {
    struct { const char *name; long (*fn)(long); } loops[] = {{"A", a}, {"B", b}, {"C", c3}, {"D", d}, {"E", e}, {"F", f}, {"G", g}};
    printf("%-4s %10s %10s %8s\n", "loop", "n = 64", "n = 128", "ratio");
    for (int i = 0; i < 7; i++) {
        long x = loops[i].fn(64), y = loops[i].fn(128);
        printf("%-4s %10ld %10ld %8.2f\n", loops[i].name, x, y, (double)y / (double)x);
    }
    return 0;
}
