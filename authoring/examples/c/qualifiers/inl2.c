#include <stdio.h>
static inline int twice(int x) { return 2 * x; }   /* the portable way in headers */
int main(void) { printf("%d\n", twice(21)); return 0; }
