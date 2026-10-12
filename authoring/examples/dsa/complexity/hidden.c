#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* Hidden O(n) inside a loop condition: strlen is re-evaluated each iteration because the loop writes to s. */
static void upper_quadratic(char *s) { for (size_t i = 0; i < strlen(s); i++) s[i] = (char)toupper((unsigned char)s[i]); }
static void upper_linear(char *s)    { size_t n = strlen(s); for (size_t i = 0; i < n; i++) s[i] = (char)toupper((unsigned char)s[i]); }

/* Repeated string concatenation: strcat re-scans the destination each time. */
static void build_naive(char *dst, int pieces) { dst[0] = 0; for (int i = 0; i < pieces; i++) strcat(dst, "item,"); }
static void build_tracked(char *dst, int pieces) { size_t len = 0; for (int i = 0; i < pieces; i++) { memcpy(dst + len, "item,", 5); len += 5; } dst[len] = 0; }

int main(void) {
    printf("%-10s %14s %14s\n", "n (chars)", "strlen in cond", "hoisted strlen");
    for (size_t n = 100000; n <= 400000; n *= 2) {
        char *s = malloc(n + 1); memset(s, 'a', n); s[n] = 0;
        double t0 = now(); upper_quadratic(s); double t1 = now();
        memset(s, 'a', n); t0 = now(); upper_quadratic(s); t1 = now();
        memset(s, 'a', n); double t2 = now(); upper_linear(s); double t3 = now();
        printf("%-10zu %11.2f ms %11.2f ms\n", n, (t1 - t0) * 1e3, (t3 - t2) * 1e3);
        free(s);
    }
    printf("\n%-10s %14s %14s\n", "pieces", "strcat loop", "tracked length");
    for (int p = 20000; p <= 80000; p *= 2) {
        char *d = malloc((size_t)p * 5 + 1);
        double t0 = now(); build_naive(d, p); double t1 = now(); build_tracked(d, p); double t2 = now();
        printf("%-10d %11.2f ms %11.2f ms\n", p, (t1 - t0) * 1e3, (t2 - t1) * 1e3);
        free(d);
    }
    return 0;
}
