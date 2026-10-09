#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *s = strdup("duplicate");                  /* POSIX, not ISO C11 */
    char *line = NULL; size_t cap = 0;
    printf("%s %d\n", s, fileno(stdin) >= 0);       /* fileno: POSIX */
    free(s); free(line);
    (void)cap;
    return 0;
}
