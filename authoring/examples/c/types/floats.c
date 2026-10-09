#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void bits(const char *label, float f) {
    uint32_t u; memcpy(&u, &f, sizeof u);             /* memcpy is the defined way to inspect the bits */
    printf("%-10s %-12.9g sign=%u exp=%3u (unbiased %4d) frac=0x%06x\n", label, f, u >> 31, (u >> 23) & 0xFF, (int)((u >> 23) & 0xFF) - 127, u & 0x7FFFFF);
}

int main(void) {
    bits("1.0", 1.0f); bits("0.1", 0.1f); bits("-2.5", -2.5f); bits("inf", INFINITY); bits("nan", NAN); bits("denorm", 1e-40f);
    printf("\n0.1 + 0.2 == 0.3 -> %d  (0.1+0.2 = %.17g)\n", 0.1 + 0.2 == 0.3, 0.1 + 0.2);
    printf("NAN == NAN -> %d, NAN != NAN -> %d, isnan -> %d\n", NAN == NAN, NAN != NAN, isnan(NAN));
    double big = 9007199254740992.0;                    /* 2^53 */
    printf("2^53 + 1 == 2^53 -> %d  (doubles hold 53 bits of integer precision)\n", big + 1 == big);
    printf("FLT_EPSILON=%g DBL_EPSILON=%g\n", FLT_EPSILON, DBL_EPSILON);
    float f = 16777216.0f;                              /* 2^24 */
    printf("float 2^24 + 1 = %.1f\n", f + 1.0f);
    double a = 1e308 * 10;
    printf("1e308 * 10 = %g, -1/0.0 = %g, 0.0/0.0 = %g\n", a, -1 / 0.0, 0.0 / 0.0);
    double sum = 0; for (int i = 0; i < 10; i++) sum += 0.1;
    printf("sum of ten 0.1 = %.17g, fabs(sum-1) < 1e-9 -> %d\n", sum, fabs(sum - 1.0) < 1e-9);
    return 0;
}
