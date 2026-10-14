#include <limits.h>
#include <stdio.h>

static int average(int a, int b) { return (a + b) / 2; }

int main(void) {
    printf("average(2000000000, 2000000000) = %d\n", average(2000000000, 2000000000));
    printf("1 << 32 on an int: %d\n", 1 << (int)sizeof(int) * 8);
    return 0;
}
