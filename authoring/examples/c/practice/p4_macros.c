#include <stdio.h>

#define SQUARE_BAD(x) x * x
#define SQUARE_OK(x) ((x) * (x))
#define MAX_BAD(a, b) ((a) > (b) ? (a) : (b))          /* parenthesised, but evaluates an argument twice */

static inline int max_int(int a, int b) { return a > b ? a : b; }   /* a function evaluates each argument once */

static int calls;
static int next(void) { return ++calls; }

int main(void) {
    printf("SQUARE_BAD(1 + 2) = %d (expands to 1 + 2 * 1 + 2)\n", SQUARE_BAD(1 + 2));
    printf("SQUARE_OK(1 + 2)  = %d\n", SQUARE_OK(1 + 2));
    int m = MAX_BAD(next(), 0);
    printf("MAX_BAD(next(), 0) = %d but next() ran %d times\n", m, calls);
    calls = 0;
    m = max_int(next(), 0);
    printf("max_int(next(), 0) = %d and next() ran %d time\n", m, calls);
    return 0;
}
