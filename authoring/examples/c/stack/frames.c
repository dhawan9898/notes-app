#include <stdio.h>

/* 8 arguments: the first six travel in registers, the rest on the stack */
long many(long a, long b, long c, long d, long e, long f, long g, long h) {
    long local = a + b + c + d + e + f + g + h;
    return local;
}

struct big { long x, y, z; };
struct big make_big(long v) { struct big s = {v, v + 1, v + 2}; return s; }   /* returned through a hidden pointer */

int main(void) {
    long r = many(1, 2, 3, 4, 5, 6, 7, 8);
    struct big b = make_big(10);
    printf("%ld %ld\n", r, b.z);
    return 0;
}
