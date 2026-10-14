#include <stdio.h>

#define MAXV 10                                                 /* values are known to be 0..10 */

int main(void) {
    int ages[] = {7, 3, 10, 3, 0, 7, 7, 1, 10, 3};
    int n = (int)(sizeof ages / sizeof ages[0]);
    int count[MAXV + 1] = {0}, out[10];
    for (int i = 0; i < n; i++) count[ages[i]]++;                /* 1. histogram */
    for (int v = 1; v <= MAXV; v++) count[v] += count[v - 1];   /* 2. prefix sums: count[v] = number of items <= v */
    for (int i = n - 1; i >= 0; i--) out[--count[ages[i]]] = ages[i];   /* 3. place from the back: keeps equal keys in order (stable) */
    printf("sorted:");
    for (int i = 0; i < n; i++) printf(" %d", out[i]);
    printf("\ncost: O(n + k) = %d + %d steps, no comparisons at all\n", n, MAXV + 1);
    return 0;
}
