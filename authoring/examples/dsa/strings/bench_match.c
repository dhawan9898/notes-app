#include <stdio.h>
#include "match.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

typedef size_t (*Fn)(const char *, size_t, const char *, size_t);
static size_t rk_wrap(const char *t, size_t n, const char *p, size_t m) { return rk_count(t, n, p, m, NULL); }

static void row(const char *label, const char *t, size_t n, const char *p, size_t m) {
    static const char *names[] = { "naive", "KMP", "Z", "Rabin-Karp", "Horspool", "memmem" };
    Fn fns[] = { naive_count, kmp_count, z_count, rk_wrap, horspool_count, memmem_count };
    printf("%-22s m=%-4zu", label, m);
    size_t ref = 0;
    for (int i = 0; i < 6; i++) {
        double t0 = now(); size_t c = fns[i](t, n, p, m); double dt = now() - t0;
        if (i == 0) ref = c; else if (c != ref) { printf("  MISMATCH %s\n", names[i]); return; }
        printf(" %10.2f", dt / (double)n * 1e9);
    }
    printf("   (%zu matches)\n", ref);
}

int main(void) {
    size_t n = 32u << 20; char *dna = malloc(n), *eng = malloc(n);
    static const char *freq = "eeeeeeeeeeeettttttttttaaaaaaaaooooooooiiiiiiiinnnnnnnsssssshhhhhhrrrrrrddddllllcccuuummmwwffggyyppbbvkjxqz        ";
    size_t fl = strlen(freq);
    for (size_t i = 0; i < n; i++) { dna[i] = "ACGT"[rnd() & 3]; eng[i] = freq[rnd() % fl]; }
    printf("ns per text byte; text = 32 MB.  columns: naive, KMP, Z, Rabin-Karp, Horspool, memmem\n");
    printf("%-22s %-6s %10s %10s %10s %10s %10s %10s\n", "text", "", "naive", "KMP", "Z", "R-K", "Horspool", "memmem");
    size_t ms[] = { 4, 16, 64 };
    for (int k = 0; k < 3; k++) row("DNA (alphabet 4)", dna, n, dna + 1000003, ms[k]);
    for (int k = 0; k < 3; k++) row("English-like (27 symbols)", eng, n, eng + 1000003, ms[k]);

    printf("\nworst case for the naive scan: text = a^4M, pattern = a^(m-1) b (never matches)\n");
    size_t wn = 4u << 20; char *wt = malloc(wn); memset(wt, 'a', wn); char wp[600];
    for (int k = 0; k < 3; k++) { size_t m = k == 0 ? 8 : k == 1 ? 64 : 512; memset(wp, 'a', m - 1); wp[m - 1] = 'b'; row("a...a vs a..ab", wt, wn, wp, m); }
    free(dna); free(eng); free(wt); return 0;
}
