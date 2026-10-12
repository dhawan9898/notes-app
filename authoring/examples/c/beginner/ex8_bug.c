#include <stdio.h>

int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++)       /* bug: runs n+1 times */
        total += v[i];
    return total;
}

int main(void) {
    int w[] = { 1, 2, 3, 4 };
    printf("%d\n", sum_first_n(w, 4));
    return 0;
}
