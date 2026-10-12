#include <stdio.h>

static unsigned long long factorial(int n) { return n <= 1 ? 1 : (unsigned long long)n * factorial(n - 1); }   /* n! = n * (n-1)!, base case 1! = 1 */

static long calls;
static long fib(int n) { calls++; return n < 2 ? n : fib(n - 1) + fib(n - 2); }                                /* simple but repeats work */

static long memo[100], memo_calls;
static long fib_memo(int n) { memo_calls++; if (n < 2) return n; if (memo[n]) return memo[n]; return memo[n] = fib_memo(n - 1) + fib_memo(n - 2); }

static int moves, verbose;
static void hanoi(int n, char from, char to, char via) {                                                       /* move n disks from `from` to `to` */
    if (n == 0) return;
    hanoi(n - 1, from, via, to);                                                                                /* 1. clear the way */
    moves++; if (verbose) printf("  move disk %d from %c to %c\n", n, from, to);                                  /* 2. move the biggest */
    hanoi(n - 1, via, to, from);                                                                                /* 3. put the others back on top */
}

static void countdown(int n) {                                                                                  /* watch the call stack grow and unwind */
    printf("%*senter %d\n", (3 - n) * 2, "", n);
    if (n > 0) countdown(n - 1);
    printf("%*sleave %d\n", (3 - n) * 2, "", n);
}

int main(void) {
    printf("5! = %llu, 10! = %llu, 20! = %llu\n", factorial(5), factorial(10), factorial(20));
    calls = 0; long f = fib(30); printf("fib(30) = %ld using %ld calls;  ", f, calls);
    long fm = fib_memo(30); printf("with memoization fib(30) = %ld using %ld calls\n", fm, memo_calls);
    printf("Towers of Hanoi with 3 disks:\n"); moves = 0; verbose = 1; hanoi(3, 'A', 'C', 'B');
    printf("%d moves (2^3 - 1); with 20 disks: ", moves); moves = 0; verbose = 0; hanoi(20, 'A', 'C', 'B'); printf("%d moves (2^20 - 1)\n", moves);
    printf("call stack for countdown(3):\n"); countdown(3);
    return 0;
}
