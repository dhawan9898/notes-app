// Pointer arithmetic limits: valid one-past-the-end pointers, and what the sanitizers say about the invalid cases.
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int a[4] = {1, 2, 3, 4}, b[4] = {5, 6, 7, 8};
    int *end = a + 4;                          // one past the end: valid to form and compare, invalid to dereference
    int sum = 0;
    for (int *p = a; p != end; p++) sum += *p;
    printf("loop to the one-past-the-end pointer: sum = %d\n", sum);
    printf("pointer difference (end - a) = %td elements, in units of the pointed-to type, not bytes\n", end - a);

    if (argc > 1) {
        int which = atoi(argv[1]);
        volatile int idx = 4;                  // volatile: keep the compiler from seeing the constant
        if (which == 1) printf("a[4] = %d\n", a[idx]);                                  // read one past the end of a: undefined behavior
        if (which == 2) { int *q = a + 100; printf("formed a + 100 (UB already), b is %s\n", q == b ? "there" : "not there"); }
        if (which == 3) { int *h = malloc(4 * sizeof *h); free(h); printf("use after free: %d\n", h[0]); }
        if (which == 4) { int *h = malloc(4 * sizeof *h); h[4] = 1; free(h); }          // heap overflow by one element
    }
    return 0;
}
