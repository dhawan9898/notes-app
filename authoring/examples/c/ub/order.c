#include <stdio.h>
static int n;
static int next(void) { return ++n; }
int main(void) {
    /* Unspecified: the order in which the arguments are evaluated. */
    printf("%d %d\n", next(), next());
    return 0;
}
