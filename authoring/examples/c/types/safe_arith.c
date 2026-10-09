#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool add_checked(int a, int b, int *out) { return !__builtin_add_overflow(a, b, out); }

/* portable pre-check for size computations: a * b fits in size_t? */
static bool mul_size(size_t a, size_t b, size_t *out) {
    if (b != 0 && a > SIZE_MAX / b) return false;
    *out = a * b;
    return true;
}

int main(void) {
    int r;
    bool ok = add_checked(INT_MAX, 1, &r);
    printf("INT_MAX + 1 via builtin: %s\n", ok ? "ok" : "overflow detected");
    ok = add_checked(100, 23, &r);                       /* evaluate first: printf argument order is unspecified */
    printf("100 + 23 via builtin:    %s (r=%d)\n", ok ? "ok" : "overflow", r);
    int c;
    bool of = ckd_mul(&c, 100000, 100000);              /* C23 <stdckdint.h> */
    printf("ckd_mul(100000,100000): overflow=%d wrapped result=%d\n", of, c);
    size_t bytes;
    ok = mul_size(SIZE_MAX / 2, 4, &bytes);
    printf("mul_size(SIZE_MAX/2, 4): %s\n", ok ? "ok" : "overflow detected");
    ok = mul_size(1000, 24, &bytes);
    printf("mul_size(1000, 24):      %s (%zu)\n", ok ? "ok" : "overflow", bytes);
    return 0;
}
