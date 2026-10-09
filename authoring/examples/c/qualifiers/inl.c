#include <stdio.h>
inline int twice(int x) { return 2 * x; }     /* C99 inline definition: no external definition emitted here */
int main(void) { printf("%d\n", twice(21)); return 0; }
