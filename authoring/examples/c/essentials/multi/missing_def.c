#include <stdio.h>

int square(int x);                  /* declared but never defined anywhere */

int main(void) {
    printf("%d\n", square(7));
    return 0;
}
