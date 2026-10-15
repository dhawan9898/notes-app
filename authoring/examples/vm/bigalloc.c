/* Ask for 600 MiB of address space. Run it normally, then under a limit:  (ulimit -v 400000; ./bigalloc) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n = 600u * 1024 * 1024;
    char *p = malloc(n);
    if (!p) { printf("malloc of 600 MiB failed: %s\n", strerror(errno)); return 1; }
    memset(p, 1, 4096);                     /* touch one page only: the rest stays virtual */
    printf("allocated 600 MiB of address space\n");
    free(p);
    return 0;
}
