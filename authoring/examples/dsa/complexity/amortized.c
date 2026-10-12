#include <stdio.h>
#include <stdlib.h>

/* Count how many element copies each growth policy performs over n appends. */
static unsigned long copies_for(unsigned long n, double factor, unsigned long add, unsigned long *max_single) {
    unsigned long cap = 1, size = 0, total = 0;
    *max_single = 0;
    for (unsigned long i = 0; i < n; i++) {
        if (size == cap) {                                     /* full: allocate a bigger block and copy everything */
            total += size;
            if (size > *max_single) *max_single = size;
            cap = add ? cap + add : (unsigned long)((double)cap * factor + 1);
        }
        size++;
    }
    return total;
}

int main(void) {
    printf("%-18s %12s %14s %16s %12s\n", "growth policy", "n appends", "total copies", "copies per append", "worst single");
    unsigned long n = 1000000, worst;
    struct { const char *name; double f; unsigned long add; } pol[] = { {"double (x2)", 2.0, 0}, {"x1.5", 1.5, 0}, {"x1.1", 1.1, 0}, {"add 16", 0, 16}, {"add 1024", 0, 1024} };
    for (int p = 0; p < 5; p++) {
        unsigned long c = copies_for(n, pol[p].f, pol[p].add, &worst);
        printf("%-18s %12lu %14lu %16.2f %12lu\n", pol[p].name, n, c, (double)c / (double)n, worst);
    }
    printf("\ndoubling: copies per append stays bounded as n grows (amortized O(1)):\n");
    for (unsigned long m = 1000; m <= 100000000UL; m *= 100) {
        unsigned long c = copies_for(m, 2.0, 0, &worst);
        printf("  n=%-10lu total copies=%-10lu per append=%.3f\n", m, c, (double)c / (double)m);
    }
    printf("add 1024: copies per append GROWS with n (O(n) amortized):\n");
    for (unsigned long m = 10000; m <= 100000000UL; m *= 10) {
        unsigned long c = copies_for(m, 0, 1024, &worst);
        printf("  n=%-10lu total copies=%-12lu per append=%.1f\n", m, c, (double)c / (double)m);
    }
    return 0;
}
