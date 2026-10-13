// Why exact predicates matter. Consecutive Fibonacci numbers satisfy Cassini's identity F(k+1)*F(k-1) - F(k)^2 = +-1, so the vectors
// b = (F(k+1), F(k)) and u = (F(k), F(k-1)) have a cross product of exactly +-1, while each product in it is about 10^17.
// Adding multiples of b to u keeps the cross product at +-1. These triples are as close to collinear as integer points can be.
#include "geom.h"
#include <stdio.h>

int main(void) {
    long long F[60]; F[0] = 0; F[1] = 1; for (int i = 2; i < 60; i++) F[i] = F[i - 1] + F[i - 2];
    long total = 0, collinear_by_double = 0, wrong_sign = 0, shown = 0;
    for (int k = 20; k < 44; k++)
        for (int m = 0; m < 400; m++) {
            pt a = {0, 0}, b = {F[k + 1], F[k]};
            pt c = {F[k] + m * b.x, F[k - 1] + m * b.y};
            if (c.x > 2000000000LL) break;
            int e = orient(a, b, c), d = orient_double(a, b, c);
            total++;
            if (d == 0) collinear_by_double++; else if (d != e) wrong_sign++;
            if (e != d && shown < 3) { shown++; printf("  example: b = (%lld, %lld), c = (%lld, %lld): exact orientation %+d, double says %+d\n", b.x, b.y, c.x, c.y, e, d); }
        }
    printf("%ld triples whose true orientation is never 0 (the cross product is +-1):\n", total);
    printf("  double precision reports 'collinear' for %ld of them and the opposite turn direction for %ld\n", collinear_by_double, wrong_sign);
    return 0;
}
