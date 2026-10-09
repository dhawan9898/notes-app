#include <stdlib.h>
#include <string.h>
static char *dup_name(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
int main(void) {
    char *a = dup_name("alpha");
    char *b = dup_name("beta");      /* leaked */
    (void)b;
    free(a);
    return 0;
}
