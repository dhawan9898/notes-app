#include <stdio.h>

static size_t size_in_function(int arr[]) { return sizeof arr; }      /* arr is really an int * here */

int main(void) {
    int a[5] = {10, 20, 30, 40, 50};
    int *p = a + 1;
    printf("*p = %d, p[2] = %d, *(a + 3) = %d\n", *p, p[2], *(a + 3));
    printf("&a[4] - p = %td elements\n", &a[4] - p);
    printf("sizeof a = %zu, sizeof p = %zu, sizeof inside function = %zu\n", sizeof a, sizeof p, size_in_function(a));
    printf("(char *)(p + 1) - (char *)p = %td bytes\n", (char *)(p + 1) - (char *)p);
    printf("a == &a[0]: %d, (void *)&a == (void *)a: %d\n", a == &a[0], (void *)&a == (void *)a);
    return 0;
}
