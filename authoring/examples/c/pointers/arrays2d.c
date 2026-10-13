// Pointer-to-array types, 2-D arrays, and how the same memory is seen through different pointer types.
#include <stdio.h>

int main(void) {
    int m[3][4];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) m[i][j] = i * 10 + j;

    printf("sizeof(m) = %zu, sizeof(m[0]) = %zu, sizeof(m[0][0]) = %zu\n", sizeof m, sizeof m[0], sizeof m[0][0]);

    int (*row)[4] = m;                         // pointer to an array of 4 ints: what m decays to
    int *flat = &m[0][0];                      // pointer to the first int
    printf("row + 1 advances by %td bytes; flat + 1 advances by %td bytes\n", (char *)(row + 1) - (char *)row, (char *)(flat + 1) - (char *)flat);
    printf("(*(row + 2))[3] = %d   m[2][3] = %d   *(*(m + 2) + 3) = %d\n", (*(row + 2))[3], m[2][3], *(*(m + 2) + 3));

    // Walking all 12 ints through the flat pointer is how row-major layout is usually exploited. It is accepted practice but
    // formally goes outside the first row's bounds; strictly conforming code indexes m[i][j].
    printf("row-major order in memory: ");
    for (int k = 0; k < 12; k++) printf("%d ", flat[k]);
    printf("\n");

    // A function taking a 2-D array needs the inner dimension in the type; C99 lets the dimension be a parameter.
    int total = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) total += (*(m + i))[j];
    printf("sum via pointer-to-array arithmetic: %d\n", total);

    int *rows[3] = { m[0], m[1], m[2] };       // an array of pointers is a different type: no contiguous layout is implied
    printf("sizeof(rows) = %zu (three pointers), rows[1][2] = %d\n", sizeof rows, rows[1][2]);
    return 0;
}
