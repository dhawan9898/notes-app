#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static unsigned long calls;

/* Binary exponentiation: x^n in O(log n) multiplications (divide and conquer on the exponent). */
static uint64_t power_naive(uint64_t x, unsigned n) { calls++; return n == 0 ? 1 : x * power_naive(x, n - 1); }
static uint64_t power_fast(uint64_t x, unsigned n) {
    calls++;
    if (n == 0) return 1;
    uint64_t half = power_fast(x, n / 2);
    return n % 2 ? half * half * x : half * half;
}

/* Tower of Hanoi: T(n) = 2T(n-1) + 1  =>  2^n - 1 moves; the recursion IS the proof of the algorithm. */
static unsigned long moves;
static void hanoi(int n, char from, char to, char via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);
    moves++;
    hanoi(n - 1, via, to, from);
}

/* Greatest common divisor: Euclid. The recursion shrinks (a, b) -> (b, a mod b); depth <= ~1.44 log2(b). */
static unsigned gcd(unsigned a, unsigned b) { calls++; return b ? gcd(b, a % b) : a; }

/* Generate all subsets by recursion (include / exclude): 2^n leaves. */
static void subsets(const char *items, int n, int i, char *cur, int len, unsigned long *count) {
    if (i == n) { (*count)++; return; }
    subsets(items, n, i + 1, cur, len, count);                  /* exclude items[i] */
    cur[len] = items[i];
    subsets(items, n, i + 1, cur, len + 1, count);              /* include items[i] */
}

int main(void) {
    calls = 0; uint64_t a = power_naive(3, 30);
    unsigned long c1 = calls;
    calls = 0; uint64_t b = power_fast(3, 30);
    printf("3^30 = %llu (both agree: %s)  naive calls=%lu  fast calls=%lu\n", (unsigned long long)b, a == b ? "yes" : "NO", c1, calls);

    for (int n = 5; n <= 25; n += 5) { moves = 0; hanoi(n, 'A', 'C', 'B'); printf("hanoi(%2d): %9lu moves  (2^n - 1 = %lu)\n", n, moves, (1UL << n) - 1); }

    calls = 0; unsigned g = gcd(1836311903u, 1134903170u);        /* consecutive Fibonacci numbers: worst case for Euclid */
    printf("gcd(F46, F45) = %u after %lu calls (log2 of the input ~ 30)\n", g, calls);

    char cur[16]; unsigned long count = 0;
    subsets("abcdefghijklmnop", 16, 0, cur, 0, &count);
    printf("subsets of 16 items: %lu\n", count);
    return 0;
}
