#include <stdio.h>

static unsigned long factorial(unsigned n) {
    if (n <= 1) return 1;                     /* base case: stops the recursion */
    return n * factorial(n - 1);              /* recursive case: a smaller copy of the same problem */
}

static void countdown(int n) {
    if (n == 0) { printf("liftoff\n"); return; }
    printf("%d ", n);
    countdown(n - 1);
}

static int sum_digits(int n) {                /* 1234 -> 4 + sum_digits(123) */
    if (n < 10) return n;
    return n % 10 + sum_digits(n / 10);
}

int main(void) {
    printf("5! = %lu, 10! = %lu\n", factorial(5), factorial(10));
    countdown(5);
    printf("digits of 98765 add up to %d\n", sum_digits(98765));
    return 0;
}
