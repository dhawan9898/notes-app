#include <stdio.h>
#include <stddef.h>

static int count_down_wrong(size_t n) {
    int steps = 0;
    for (size_t i = n - 1; i >= 0 && steps < 5; i--) steps++;    /* i >= 0 is always true for an unsigned type */
    return steps;
}

static int count_down_right(size_t n) {
    int steps = 0;
    for (size_t i = n; i-- > 0; ) steps++;                       /* the "goes to" idiom: test, then decrement */
    return steps;
}

int main(void) {
    printf("wrong version stops only because of the extra safety counter: %d steps for n = 3\n", count_down_wrong(3));
    printf("right version: %d steps for n = 3, %d for n = 0\n", count_down_right(3), count_down_right(0));
    return 0;
}
