#include <stdio.h>

#define SQUARE_BAD(x)   x * x
#define SQUARE_OK(x)    ((x) * (x))
#define MAX_BAD(a, b)   ((a) > (b) ? (a) : (b))               /* parenthesized, but evaluates arguments twice */
#define MAX_SAFE(a, b)  ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })   /* GNU statement expression */
#define SWAP_BAD(a, b)  int t = a; a = b; b = t;
#define SWAP_OK(T, a, b) do { T t_ = (a); (a) = (b); (b) = t_; } while (0)

static int calls;
static int next(void) { return ++calls; }

int main(void) {
    printf("SQUARE_BAD(1+2) = %d (expected 9)\n", SQUARE_BAD(1 + 2));      /* expands to 1 + 2 * 1 + 2 */
    printf("SQUARE_OK(1+2)  = %d\n", SQUARE_OK(1 + 2));

    int i = 3, j = 5;
    int m = MAX_BAD(i++, j++);                                              /* the larger argument is incremented twice */
    printf("MAX_BAD(i++, j++) = %d, i=%d, j=%d (j incremented twice)\n", m, i, j);

    calls = 0;
    int r = MAX_BAD(next(), 0);
    printf("MAX_BAD(next(), 0) = %d, next() was called %d times\n", r, calls);
    calls = 0;
    r = MAX_SAFE(next(), 0);
    printf("MAX_SAFE(next(), 0) = %d, next() was called %d time\n", r, calls);

    int a = 1, b = 2;
    SWAP_OK(int, a, b);
    printf("after SWAP_OK a=%d b=%d\n", a, b);

    if (a > b)
        SWAP_OK(int, a, b);       /* safe in an if/else because of do { } while (0); a naive two-statement macro would break the else */
    else
        puts("else branch reached correctly");
    return 0;
}
