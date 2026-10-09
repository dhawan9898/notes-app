#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    char *p = malloc(16);
    switch (which) {
    case 1: p[16] = 'x'; break;                         /* heap buffer overflow by one byte */
    case 2: free(p); p[0] = 'x'; return 0;              /* use after free */
    case 3: free(p); free(p); return 0;                 /* double free */
    case 4: p = NULL; break;                            /* leak: the only pointer is lost */
    case 5: { int *a = malloc(4 * sizeof *a); int v = a[2]; printf("%d\n", v); free(a); } break;   /* uninitialized read (MSan/valgrind) */
    case 6: { char *q = realloc(p, 1 << 20); if (!q) return 1; p = q; memset(p, 0, 1 << 20); } break;
    default: break;
    }
    free(p);
    return 0;
}
