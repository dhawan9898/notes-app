#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* 1. usual arithmetic conversions: signed is converted to unsigned */
    int i = -1;
    unsigned int u = 1;
    printf("-1 < 1u            -> %d   (i is converted to %u)\n", i < u, (unsigned)i);

    /* 2. integer promotion: small types become int before arithmetic */
    uint8_t a = 200, b = 100;
    printf("a + b (uint8_t)    -> %d   (computed in int, not wrapped)\n", a + b);
    uint8_t sum = a + b;
    printf("stored in uint8_t  -> %d   (implicit narrowing)\n", sum);

    /* 3. promotion surprises with unary operators */
    uint8_t m = 0x0F;
    printf("~m                 -> 0x%x (promoted to int)\n", ~m);
    printf("(uint8_t)~m        -> 0x%x\n", (uint8_t)~m);

    /* 4. plain char signedness */
    char c = (char)200;
    printf("(char)200          -> %d   (char is %s here)\n", c, (char)-1 < 0 ? "signed" : "unsigned");

    /* 5. size_t is unsigned: the countdown loop trap */
    size_t n = 3, steps = 0;
    for (size_t k = n - 1; k < n; k--) steps++;   /* correct idiom: stops when k wraps to SIZE_MAX */
    printf("countdown iterations = %zu (using the k < n wrap test)\n", steps);

    /* 6. a signed length compared with a size_t capacity */
    long len = -5;
    size_t cap = 10;
    printf("len(-5) < cap(10)  -> %d   (len is converted to a huge unsigned value)\n", len < cap);
    printf("len < 0 || (size_t)len < cap -> %d   (check the sign first)\n", len < 0 || (size_t)len < cap);

    /* 7. widening multiplication happens too late */
    int x = 100000, y = 100000;
    long long wrong = x * y;                 /* overflow in int arithmetic (UB!) */
    (void)wrong;
    long long right = (long long)x * y;
    printf("(long long)x * y   -> %lld\n", right);

    /* 8. abs and INT_MIN */
    printf("-INT_MIN as unsigned is %u; INT_MIN = %d\n", (unsigned)INT_MIN, INT_MIN);

    /* 9. narrowing conversions */
    long big = 0x1FFFFFFFFL;
    int narrowed = (int)big;                 /* implementation-defined (GCC: modulo 2^32) */
    printf("(int)0x1FFFFFFFF   -> %d\n", narrowed);
    return 0;
}
