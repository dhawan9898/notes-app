#include <stdio.h>

int main(void) {
    int unused;
    int x;
    printf("x is %d\n", x);          /* x was never given a value */
    printf("%d\n", "hello");         /* wrong format for a string */
    return 0;
}
