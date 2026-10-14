#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Exercise 3: add two ints without ever overflowing. Return false if the true sum does not fit. */
static bool safe_add(int a, int b, int *out) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return false;
    *out = a + b;
    return true;
}

int main(void) {
    /* Exercise 1: predict, then compare */
    uint8_t x = 200, y = 100;
    printf("x + y = %d (promoted to int), (uint8_t)(x + y) = %d\n", x + y, (uint8_t)(x + y));
    printf("(char)200 as int = %d on this platform (char is %s)\n", (int)(char)200, (char)-1 < 0 ? "signed" : "unsigned");
    printf("-1 < 1u is %d (the -1 converts to unsigned and becomes %u)\n", -1 < 1u, (unsigned)-1);
    printf("-7 / 2 = %d, -7 %% 2 = %d, 7 / -2 = %d\n", -7 / 2, -7 % 2, 7 / -2);
    printf("1.0f/3 == 1.0/3 is %d (float and double round differently)\n", 1.0f / 3 == 1.0 / 3);
    printf("0.1 + 0.2 == 0.3 is %d\n", 0.1 + 0.2 == 0.3);

    /* Exercise 2: sizes and limits */
    printf("sizeof: char %zu short %zu int %zu long %zu long long %zu void* %zu\n",
           sizeof(char), sizeof(short), sizeof(int), sizeof(long), sizeof(long long), sizeof(void *));
    printf("INT_MAX = %d, INT_MIN = %d, UINT_MAX = %u\n", INT_MAX, INT_MIN, UINT_MAX);

    /* Exercise 3: safe_add */
    int r = 0;
    bool ok1 = safe_add(2000000000, 2000000000, &r);
    printf("safe_add(2000000000, 2000000000) -> %s\n", ok1 ? "fits" : "would overflow");
    bool ok2 = safe_add(2000000000, -5, &r);                       /* separate statements: argument order is unspecified */
    printf("safe_add(2000000000, -5) -> %s (%d)\n", ok2 ? "fits" : "would overflow", r);

    /* Exercise 4: unsigned wrap-around IS defined; signed overflow is not */
    unsigned u = UINT_MAX;
    u++;
    printf("UINT_MAX + 1 wraps to %u (defined behaviour)\n", u);
    return 0;
}
