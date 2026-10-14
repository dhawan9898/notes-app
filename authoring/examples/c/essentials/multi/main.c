#include <stdio.h>
#include "geometry.h"

static int calls = 0;                     /* 'static' at file level: private to main.c */

int main(void) {
    Vec a = {3, 4}, b = {1, 2};
    Vec sum = vec_add(a, b);
    calls++;
    printf("length of a = %.1f, sum = (%.1f, %.1f), calls = %d\n", vec_length(a), sum.x, sum.y, calls);
    return 0;
}
