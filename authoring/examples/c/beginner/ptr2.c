#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int a[5] = { 10, 20, 30, 40, 50 };
    int *p = a;                                   /* an array name becomes a pointer to its first element */
    printf("a[2] = %d, *(p + 2) = %d, p[2] = %d   (all the same element)\n", a[2], *(p + 2), p[2]);
    printf("(p + 1) is %td bytes after p: pointer arithmetic moves in whole elements, not bytes\n", (char *)(p + 1) - (char *)p);

    for (int *q = a; q < a + 5; q++) printf("%d ", *q);       /* walk the array with a pointer */
    printf("\n");

    /* Heap memory: you ask for it, you must give it back exactly once. */
    int n = 4;
    int *buf = malloc(n * sizeof *buf);           /* sizeof *buf = size of one int, whatever the type of buf is */
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); return 1; }
    for (int i = 0; i < n; i++) buf[i] = i * i;
    for (int i = 0; i < n; i++) printf("%d ", buf[i]);
    printf("\n");
    free(buf);
    buf = NULL;                                   /* so an accidental later use fails loudly instead of corrupting memory */

    char *copy = malloc(strlen("hello") + 1);     /* +1 for the terminating zero byte */
    strcpy(copy, "hello");
    printf("copy = %s\n", copy);
    free(copy);
    return 0;
}
