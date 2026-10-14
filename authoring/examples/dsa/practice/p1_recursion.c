#include <stdio.h>

static long calls;

static long fib_naive(int n) { calls++; return n < 2 ? n : fib_naive(n - 1) + fib_naive(n - 2); }

static long memo[91];
static long fib_memo(int n) {
    calls++;
    if (n < 2) return n;
    if (memo[n]) return memo[n];                          /* already computed: reuse it */
    return memo[n] = fib_memo(n - 1) + fib_memo(n - 2);
}

static long hanoi_moves(int n) {                          /* move n disks: move n-1 aside, move the big one, move n-1 back on top */
    if (n == 0) return 0;
    return 2 * hanoi_moves(n - 1) + 1;
}

static long power_slow(long b, int e) { long r = 1; for (int i = 0; i < e; i++) r *= b; return r; }
static long power_fast(long b, int e) {                   /* square-and-multiply: halve the exponent each step, O(log e) */
    if (e == 0) return 1;
    long half = power_fast(b, e / 2);
    return e % 2 ? half * half * b : half * half;
}

int main(void) {
    int n = 30;
    calls = 0; long f1 = fib_naive(n);
    printf("fib(%d) = %ld: naive used %ld calls", n, f1, calls);
    calls = 0; long f2 = fib_memo(n);
    printf(", memoised used %ld calls\n", calls);
    printf("hanoi moves for 1, 5, 10, 20 disks: %ld %ld %ld %ld (2^n - 1)\n", hanoi_moves(1), hanoi_moves(5), hanoi_moves(10), hanoi_moves(20));
    printf("3^13: slow %ld, fast %ld (a 13-step loop versus 4 levels of recursion)\n", power_slow(3, 13), power_fast(3, 13));
    (void)f2;
    return 0;
}
