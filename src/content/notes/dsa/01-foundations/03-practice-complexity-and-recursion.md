---
title: "Practice: Complexity and Recursion"
track: dsa
module: Foundations
moduleOrder: 1
order: 3
level: intermediate
summary: "Four exercises on the foundations module: classifying seven loops by counting their steps and reading the doubling ratio, explaining why naive Fibonacci explodes and memoisation fixes it, counting Tower of Hanoi moves, and computing powers in O(log n) with square-and-multiply, with verified C answers."
prerequisites:
  - "Complexity Analysis and Amortized Analysis"
  - "Recursion, Divide and Conquer, and Loop Invariants"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 3 (Characterizing Running Times), Ch. 4 (Divide-and-Conquer)"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §1.4 (Analysis of Algorithms)"
  - label: "Knuth — The Art of Computer Programming, Vol. 1, §1.2 (Mathematical Preliminaries)"
---

Complexity analysis is learned by classifying many small loops until it is automatic. First do each one on paper (write the exact count for `n` as a formula if you can), then check with the programs.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Classify the loops.** For each loop shape, give the Big-O of the number of times the innermost statement runs, then predict the ratio when `n` doubles from 64 to 128.

- A: `for i in 0..n` (single loop)
- B: `for (i = 1; i < n; i *= 2)` (the counter doubles)
- C: `for i in 0..n { for j in 0..n }`
- D: `for i in 0..n { for j in i..n }` (the inner loop starts at `i`)
- E: `for (i = 1; i < n; i *= 2) { for j in 0..i }`
- F: `for i in 0..n { for (j = 1; j < n; j *= 2) }`
- G: two separate loops of `n` iterations, one after the other

**2. Why is naive Fibonacci slow?** `fib(n) = fib(n-1) + fib(n-2)`. How many calls does `fib(30)` make? Fix it with memoisation and count again. What is the complexity of each version?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: Fibonacci with memoisation. Same answers, far fewer calls.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long long calls;      /* the tests count calls: keep the calls++ line */
long long memo[91];         /* memo[n] == 0 means "not computed yet" */

/* ===== Your code: edit below this line ===== */
long long fib_memo(int n) {
    calls++;
    if (n < 2) return n;
    return fib_memo(n - 1) + fib_memo(n - 2);      /* naive: recomputes the same values again and again */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    calls = 0;
    check_int("fib_memo(30)", fib_memo(30), 832040);
    printf("     fib_memo(30) made %lld calls\n", calls);
    check_int("fib_memo(30) made at most 2 * 30 calls", calls <= 60, 1);
    if (calls <= 60) {
        check_int("fib_memo(90)", fib_memo(90), 2880067194370816120LL);
    } else {
        checks++; failed++;
        printf("FAIL fib_memo(90): skipped, it would take years without memoisation\n");
    }
    return summary();
}
```

</details>

**3. Tower of Hanoi.** Moving *n* disks from one peg to another (larger disks never on smaller ones) takes how many moves? Write the recurrence and the closed form. If one move takes one second, how long do 20 and 64 disks take?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: Tower of Hanoi. Count the moves, then make them.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests provide move(from, to) and check that every move is legal. Pegs are 0, 1 and 2. */
void move(int from, int to);

/* ===== Your code: edit below this line ===== */
/* Number of moves for n disks, from the recurrence. */
long long hanoi_moves(int n) {
    (void)n;
    return 0;
}

/* Move n disks from peg "from" to peg "to", using "via", by calling move(). */
void hanoi(int n, int from, int to, int via) {
    (void)n; (void)from; (void)to; (void)via;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int peg[3][16], height[3], made, illegal;

void move(int from, int to) {
    made++;
    if (from < 0 || from > 2 || to < 0 || to > 2 || height[from] == 0) { illegal++; return; }
    int disk = peg[from][height[from] - 1];
    if (height[to] > 0 && peg[to][height[to] - 1] < disk) { illegal++; return; }     /* larger on smaller */
    height[from]--;
    peg[to][height[to]++] = disk;
}

static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("hanoi_moves(1)", hanoi_moves(1), 1);
    check_int("hanoi_moves(5)", hanoi_moves(5), 31);
    check_int("hanoi_moves(20)", hanoi_moves(20), 1048575);
    check_int("hanoi_moves(63)", hanoi_moves(63), 9223372036854775807LL);
    for (int d = 0; d < 6; d++) peg[0][d] = 6 - d;          /* six disks on peg 0, largest at the bottom */
    height[0] = 6;
    hanoi(6, 0, 2, 1);
    check_int("hanoi(6): illegal moves", illegal, 0);
    check_int("hanoi(6): moves made", made, 63);
    check_int("hanoi(6): disks on peg 2 at the end", height[2], 6);
    return summary();
}
```

</details>

**4. Fast power.** Compute `b^e` with O(log e) multiplications instead of `e`. Why does halving the exponent work?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 4: b^e with O(log e) multiplications instead of e.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Multiply only through mul(): the tests count the multiplications. */
static long long mults;
static long long mul(long long a, long long b) { mults++; return a * b; }

/* ===== Your code: edit below this line ===== */
long long power_fast(long long b, int e) {
    long long r = 1;
    for (int i = 0; i < e; i++) r = mul(r, b);     /* e multiplications: make it O(log e) */
    return r;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("power_fast(3, 13)", power_fast(3, 13), 1594323);
    check_int("power_fast(2, 62)", power_fast(2, 62), 4611686018427387904LL);
    check_int("power_fast(7, 0)", power_fast(7, 0), 1);
    check_int("power_fast(-2, 5)", power_fast(-2, 5), -32);
    mults = 0;
    check_int("power_fast(1, 1000)", power_fast(1, 1000), 1);
    printf("     power_fast(1, 1000) used %lld multiplications\n", mults);
    check_int("power_fast(1, 1000) used at most 20 multiplications", mults <= 20, 1);
    return summary();
}
```

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>

/* Count how many times the innermost statement runs for n = 64 and n = 128, then read the growth. */
static long a(long n) { long c = 0; for (long i = 0; i < n; i++) c++; return c; }                                   /* A */
static long b(long n) { long c = 0; for (long i = 1; i < n; i *= 2) c++; return c; }                                /* B */
static long c3(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 0; j < n; j++) c++; return c; }     /* C */
static long d(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = i; j < n; j++) c++; return c; }      /* D */
static long e(long n) { long c = 0; for (long i = 1; i < n; i *= 2) for (long j = 0; j < i; j++) c++; return c; }   /* E */
static long f(long n) { long c = 0; for (long i = 0; i < n; i++) for (long j = 1; j < n; j *= 2) c++; return c; }   /* F */
static long g(long n) { long c = 0; for (long i = 0; i < n; i++) { c++; } for (long j = 0; j < n; j++) { c++; } return c; } /* G */

int main(void) {
    struct { const char *name; long (*fn)(long); } loops[] = {{"A", a}, {"B", b}, {"C", c3}, {"D", d}, {"E", e}, {"F", f}, {"G", g}};
    printf("%-4s %10s %10s %8s\n", "loop", "n = 64", "n = 128", "ratio");
    for (int i = 0; i < 7; i++) {
        long x = loops[i].fn(64), y = loops[i].fn(128);
        printf("%-4s %10ld %10ld %8.2f\n", loops[i].name, x, y, (double)y / (double)x);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p1_loops p1_loops.c && ./p1_loops; rm -f p1_loops
loop     n = 64    n = 128    ratio
A            64        128     2.00
B             6          7     1.17
C          4096      16384     4.00
D          2080       8256     3.97
E            63        127     2.02
F           384        896     2.33
G           128        256     2.00
```

- **A** is **O(n)**: exactly n steps; doubling n doubles the work (ratio 2.00).
- **B** is **O(log n)**: the counter doubles each time, so about log₂ n steps (6, then 7: one extra step when n doubles; ratio 1.17).
- **C** is **O(n²)**: n × n steps; doubling n quadruples the work (4.00).
- **D** is **O(n²)** too: the inner loop runs n, n−1, ..., 1 times, a sum of n(n+1)/2 steps (2080 for n = 64: 64 × 65 / 2). It is half of C, but the constant does not change the class; the ratio approaches 4 (3.97).
- **E** is **O(n)**, a classic surprise: the inner loop runs 1 + 2 + 4 + ... + n/2 times, a geometric series that sums to about n (63 for n = 64). Two nested loops are not automatically quadratic; look at how many times the inner one runs *in total*. Ratio 2.02.
- **F** is **O(n log n)**: an outer loop of n and an inner loop of log n (n × 6 = 384 for n = 64). Ratio 2.33, a little over 2.
- **G** is **O(n)**: loops *in sequence* add (n + n = 2n); only *nested* loops multiply. Ratio 2.00.

The doubling ratio (1, 2, slightly above 2, 4) is the practical test from *Measuring Growth: Counting and Timing in Practice*.

**2, 3 and 4.**

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p1_recursion p1_recursion.c && ./p1_recursion; rm -f p1_recursion
fib(30) = 832040: naive used 2692537 calls, memoised used 59 calls
hanoi moves for 1, 5, 10, 20 disks: 1 31 1023 1048575 (2^n - 1)
3^13: slow 1594323, fast 1594323 (a 13-step loop versus 4 levels of recursion)
```

*Fibonacci.* The naive version recomputes the same values over and over: `fib(30)` makes **2,692,537 calls** to produce one number, and the call count itself grows like the Fibonacci numbers (about 1.618ⁿ, **exponential**), so `fib(50)` would need tens of billions. Memoisation stores each `fib(k)` the first time and reuses it: **59 calls** (2n − 1), so O(n) time and O(n) space. This is the core idea of dynamic programming: when subproblems overlap, remember them. (An iterative loop with two variables does the same in O(n) time and O(1) space.)

*Hanoi.* To move n disks: move n−1 disks to the spare peg, move the largest disk, move the n−1 disks back on top, so `T(n) = 2·T(n−1) + 1` with `T(0) = 0`, giving `T(n) = 2ⁿ − 1` (1, 31, 1,023 and 1,048,575 moves for 1, 5, 10 and 20 disks). At one move per second, 20 disks take about 12 days, and 64 disks take 2⁶⁴ − 1 ≈ 1.8 × 10¹⁹ seconds, about 585 billion years. Exponential growth means some problems are out of reach however fast the computer: here the *answer itself* is exponentially long, so no algorithm can do better.

*Fast power.* `b^e` equals `(b^(e/2))²` when `e` is even and `b · (b^(e/2))²` when `e` is odd, so each step halves the exponent: O(log e) multiplications (4 levels of recursion for `e` = 13, instead of a 13-step loop). Halving works because exponents add (`b^(2k) = b^k · b^k`). The same trick computes modular powers (used in cryptography), matrix powers and Fibonacci numbers in O(log n) steps. This is **divide and conquer** with a single subproblem of half the size, giving the recurrence `T(e) = T(e/2) + O(1) = O(log e)`.

</details>

## Key takeaways

1. Count steps, then read the doubling ratio: 2 means linear, about 4 means quadratic, slightly above 2 means n log n, barely rising means logarithmic.
2. Nested loops are not automatically quadratic (a doubling inner loop sums to about n); sequential loops add, nested loops multiply.
3. Overlapping subproblems make naive recursion exponential; memoisation turns it linear.
4. Halving the problem each step gives O(log n); recurrences such as T(n) = 2T(n−1) + 1 solve to exponential growth.
