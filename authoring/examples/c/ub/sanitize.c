#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    int big = INT_MAX, zero = 0, sh = 40, arr[4] = {1, 2, 3, 4};
    int *p = NULL;
    switch (which) {
    case 1: printf("%d\n", big + argc);            break;  /* signed overflow */
    case 2: printf("%d\n", 10 / (zero + argc - 2)); break;  /* division by zero */
    case 3: printf("%d\n", 1 << (sh + argc));       break;  /* shift exponent too large */
    case 4: printf("%d\n", arr[argc + 3]);          break;  /* out-of-bounds index */
    case 5: printf("%d\n", *p);                     break;  /* null dereference */
    case 6: { char buf[8]; int *q = (int *)(buf + 1); *q = 1; printf("%d\n", *q); } break;  /* misaligned */
    default: puts("ok");
    }
    return 0;
}
