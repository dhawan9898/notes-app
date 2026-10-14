#include <stdio.h>

/* Three loops with different growth. Each counts its basic steps, so the experiment does not depend on machine speed. */
static long linear(long n)    { long steps = 0; for (long i = 0; i < n; i++) steps++; return steps; }
static long nlogn(long n)     { long steps = 0; for (long i = 0; i < n; i++) for (long k = 1; k < n; k *= 2) steps++; return steps; }
static long quadratic(long n) { long steps = 0; for (long i = 0; i < n; i++) for (long j = 0; j < n; j++) steps++; return steps; }

int main(void) {
    printf("%8s %14s %14s %16s %10s\n", "n", "O(n) steps", "O(n log n)", "O(n^2) steps", "n^2 ratio");
    long prev = 0;
    for (long n = 1000; n <= 16000; n *= 2) {
        long q = quadratic(n);
        char ratio[16] = "-";
        if (prev) snprintf(ratio, sizeof ratio, "%.1fx", (double)q / (double)prev);
        printf("%8ld %14ld %14ld %16ld %10s\n", n, linear(n), nlogn(n), q, ratio);
        prev = q;
    }

    return 0;
}
