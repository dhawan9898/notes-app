#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *make_copy(const char *s) {
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);                         /* p may be NULL here */
    return p;
}
void use_after_free(void) {
    char *q = malloc(10);
    if (!q) return;
    free(q);
    q[0] = 'x';
}
int leak(int flag) {
    FILE *f = fopen("/tmp/x", "r");
    if (flag) return -1;                  /* f leaked on this path */
    fclose(f);
    return 0;
}
