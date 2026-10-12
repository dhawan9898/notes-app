#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argv;
    if (argc == 1) {                      /* bug 1: dereferencing NULL */
        int *p = NULL;
        printf("about to read through a NULL pointer...\n"); fflush(stdout);
        printf("%d\n", *p);
    } else if (argc == 2) {               /* bug 2: using memory after free */
        int *p = malloc(sizeof *p); *p = 7; free(p);
        printf("%d\n", *p);
    } else {                              /* bug 3: reading past the end of an array */
        int a[3] = { 1, 2, 3 }; volatile int i = 3;
        printf("%d\n", a[i]);
    }
    return 0;
}
