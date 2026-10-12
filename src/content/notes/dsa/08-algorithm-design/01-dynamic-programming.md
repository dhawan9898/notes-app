---
title: Dynamic Programming
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 1
level: advanced
summary: "A method for designing DP solutions — state, transition, order, answer — with knapsack, coin change, LIS, matrix chain, subset sum and tree DP, every one verified against brute force, plus measured space and time optimizations and the loop-direction bug."
sources:
  - label: "Bellman — Dynamic Programming (Princeton University Press, 1957)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Dynamic Programming)"
  - label: "Kleinberg & Tardos — Algorithm Design, Ch. 6 (Dynamic Programming)"
  - label: "Skiena — The Algorithm Design Manual, 3rd ed., Ch. 10 (Dynamic Programming)"
  - label: "Fredman — On Computing the Length of Longest Increasing Subsequences (Discrete Mathematics 11(1), 1975)"
  - label: "Hirschberg — A Linear Space Algorithm for Computing Maximal Common Subsequences (CACM 18(6), 1975)"
  - label: "Garey & Johnson — Computers and Intractability (pseudo-polynomial algorithms and strong NP-completeness)"
---

**Dynamic programming** (DP) solves a problem by combining solutions of overlapping subproblems, computing each exactly once. It applies when two properties hold:

1. **Optimal substructure** — an optimal solution is built from optimal solutions of subproblems (the shortest path from `a` to `c` through `b` uses shortest paths `a→b` and `b→c`).
2. **Overlapping subproblems** — the same subproblems recur, so remembering answers saves work. (If subproblems do not overlap, you have *divide and conquer*: merge sort has optimal substructure and no overlap, so memoization buys nothing.)

The term, coined by Richard Bellman, predates computers as a word for "planning"; what it means in practice is: *turn an exponential recursion into a polynomial table fill*.

## A four-step method

1. **State.** What is the minimum information that identifies a subproblem? `f(i, c)` = "best value using the first `i` items with capacity `c`". The state is the most important decision; its size (number of distinct states) times the work per state is the running time.
2. **Transition.** Express a state through smaller states: `f(i, c) = max(f(i−1, c), v_i + f(i−1, c − w_i))`.
3. **Base cases and order.** Smallest states are known directly; evaluate in an order that guarantees dependencies are ready (increasing `i`; or recursion with memoization finds the order for you).
4. **Answer and reconstruction.** The answer is some state (here `f(n, cap)`); to recover *which* choices achieve it, store the decision (or recompute backwards from the table).

**Top-down** (recursion + memo table) writes the recurrence literally and computes only reachable states, at the cost of recursion depth and call overhead. **Bottom-up** (loops over a table) is usually faster, makes the evaluation order explicit, and allows **space optimization**: if row `i` depends only on row `i−1`, keep one row.

### Overlap, measured

Fibonacci is the smallest example of why it matters:

```c
/* ---- 1. overlapping subproblems: Fibonacci three ways, counting calls ---- */
static unsigned long long calls;
static ll fib_naive(int n) { calls++; return n < 2 ? n : fib_naive(n - 1) + fib_naive(n - 2); }
static ll memo[100];
static ll fib_memo(int n) { calls++; if (n < 2) return n; if (memo[n]) return memo[n]; return memo[n] = fib_memo(n - 1) + fib_memo(n - 2); }
```

The complete demonstration program (listing below) runs the numbers: the naive recursion makes **331 million calls** for `fib(40)` while the memoized version makes **79** — a state space of 40 entries, each computed once.

## The library

```c
#ifndef DP_H
#define DP_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef long long ll;
#define MAXV 100000

/* ---- 0/1 knapsack: dp[c] = best value using capacity c.  Iterate capacity DOWNWARD so each item is used at most once. ---- */
static inline ll knap01(const int *w, const int *v, int n, int cap) {
    ll *dp = calloc((size_t)cap + 1, sizeof *dp);
    for (int i = 0; i < n; i++) for (int c = cap; c >= w[i]; c--) if (dp[c - w[i]] + v[i] > dp[c]) dp[c] = dp[c - w[i]] + v[i];
    ll r = dp[cap]; free(dp); return r;
}
/* ---- unbounded knapsack: the same recurrence with the capacity loop UPWARD, so an item can be reused ---- */
static inline ll knap_unbounded(const int *w, const int *v, int n, int cap) {
    ll *dp = calloc((size_t)cap + 1, sizeof *dp);
    for (int i = 0; i < n; i++) for (int c = w[i]; c <= cap; c++) if (dp[c - w[i]] + v[i] > dp[c]) dp[c] = dp[c - w[i]] + v[i];
    ll r = dp[cap]; free(dp); return r;
}
/* ---- coin change: minimum number of coins for amount (or -1), and number of distinct combinations ---- */
static inline int coin_min(const int *coin, int k, int amount) {
    int *dp = malloc(((size_t)amount + 1) * sizeof(int)); dp[0] = 0;
    for (int a = 1; a <= amount; a++) { dp[a] = 1 << 30; for (int i = 0; i < k; i++) if (coin[i] <= a && dp[a - coin[i]] + 1 < dp[a]) dp[a] = dp[a - coin[i]] + 1; }
    int r = dp[amount] >= 1 << 30 ? -1 : dp[amount]; free(dp); return r;
}
static inline ll coin_ways(const int *coin, int k, int amount) {      /* order of coins does not matter: coins in the OUTER loop */
    ll *dp = calloc((size_t)amount + 1, sizeof *dp); dp[0] = 1;
    for (int i = 0; i < k; i++) for (int a = coin[i]; a <= amount; a++) dp[a] += dp[a - coin[i]];
    ll r = dp[amount]; free(dp); return r;
}
/* ---- longest increasing subsequence, O(n log n): tails[k] = smallest possible last element of an increasing subsequence of length k+1 ---- */
static inline int lis_nlogn(const int *a, int n) {
    int *tails = malloc((size_t)n * sizeof(int)), len = 0;
    for (int i = 0; i < n; i++) {
        int lo = 0, hi = len; while (lo < hi) { int m = (lo + hi) / 2; if (tails[m] < a[i]) lo = m + 1; else hi = m; }   /* first tail >= a[i] (strict increase) */
        tails[lo] = a[i]; if (lo == len) len++;
    }
    free(tails); return len;
}
static inline int lis_n2(const int *a, int n) {
    int *d = malloc((size_t)n * sizeof(int)), best = 0;
    for (int i = 0; i < n; i++) { d[i] = 1; for (int j = 0; j < i; j++) if (a[j] < a[i] && d[j] + 1 > d[i]) d[i] = d[j] + 1; if (d[i] > best) best = d[i]; }
    free(d); return best;
}
/* ---- matrix-chain multiplication: dp[i][j] = fewest scalar multiplications for A_i..A_j; interval DP, O(n^3) ---- */
static inline ll matrix_chain(const int *dim, int n) {                 /* matrix i is dim[i] x dim[i+1] */
    ll *dp = calloc((size_t)n * n, sizeof *dp);
    for (int len = 2; len <= n; len++) for (int i = 0; i + len - 1 < n; i++) {
        int j = i + len - 1; ll best = -1;
        for (int k = i; k < j; k++) { ll c = dp[i * n + k] + dp[(k + 1) * n + j] + (ll)dim[i] * dim[k + 1] * dim[j + 1]; if (best < 0 || c < best) best = c; }
        dp[i * n + j] = best;
    }
    ll r = dp[n - 1]; free(dp); return r;
}
/* ---- subset sum with a bitset: bit s of `reach` says "some subset sums to s"; adding item x is one shift-or over the whole word array ---- */
static inline int subset_sum_bitset(const int *x, int n, int target) {
    size_t words = ((size_t)target >> 6) + 1; uint64_t *b = calloc(words, 8); b[0] = 1;
    for (int i = 0; i < n; i++) {
        int sh = x[i], ws = sh >> 6, bs = sh & 63;
        for (long k = (long)words - 1; k >= ws; k--) {
            uint64_t v = b[k - ws] << bs; if (bs && k - ws - 1 >= 0) v |= b[k - ws - 1] >> (64 - bs);
            b[k] |= v;
        }
    }
    int r = (int)((b[target >> 6] >> (target & 63)) & 1); free(b); return r;
}
static inline int subset_sum_plain(const int *x, int n, int target) {
    char *dp = calloc((size_t)target + 1, 1); dp[0] = 1;
    for (int i = 0; i < n; i++) for (int s = target; s >= x[i]; s--) dp[s] |= dp[s - x[i]];
    int r = dp[target]; free(dp); return r;
}
#endif
```

Notes on the code:

- **0/1 vs unbounded knapsack differ only in loop direction.** In 0/1 knapsack the inner loop runs capacity *downward*, so `dp[c − w]` still holds the value from *before* this item was considered; running it *upward* lets the same item be added repeatedly — which is exactly the unbounded problem. Picking the wrong direction produces plausible but wrong answers (shown below).
- **Counting combinations vs sequences in coin change** is the order of the loops: coins in the *outer* loop counts each multiset once (`1+2` and `2+1` are the same); amount in the outer loop would count ordered sequences.
- **LIS in O(n log n)** keeps `tails[k]`, the smallest last element of any increasing subsequence of length `k+1`. It is always sorted, so each new element replaces the first tail ≥ it by binary search — "patience sorting". The array does not hold a subsequence itself, only its length is the answer.
- **Matrix-chain multiplication** is an *interval DP*: `dp[i][j]` for the best parenthesization of matrices `i..j`, trying every split point `k`. States are `O(n²)`, each does `O(n)` work → `O(n³)`.
- **Subset sum** uses a bitset: bit `s` records "some subset sums to `s`", and adding an item `x` is `reach |= reach << x` applied across machine words.

## Verification

Every DP is checked against an exhaustive or exponential-recursion oracle on 3000 random instances: all subsets for knapsack and LIS and subset sum, enumeration of copy counts for unbounded knapsack, plain recursion for coin change and matrix chain.

```c
#include <assert.h>
#include <stdio.h>
#include "dp.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ---- brute-force oracles: enumerate everything ---- */
static ll brute_knap(const int *w, const int *v, int n, int cap) {
    ll best = 0; for (unsigned m = 0; m < (1u << n); m++) { int W = 0; ll V = 0; for (int i = 0; i < n; i++) if (m >> i & 1) { W += w[i]; V += v[i]; } if (W <= cap && V > best) best = V; } return best;
}
static ll unb_enum(const int *w, const int *v, int n, int i, int cap) {      /* choose how many copies of item i, then recurse */
    if (i == n) return 0;
    ll best = 0; for (int c = 0; c * w[i] <= cap; c++) { ll x = c * v[i] + unb_enum(w, v, n, i + 1, cap - c * w[i]); if (x > best) best = x; }
    return best;
}
static int coin_rec(const int *c, int k, int a) {
    if (a == 0) return 0;
    int best = 1 << 30;
    for (int i = 0; i < k; i++) if (c[i] <= a) { int x = coin_rec(c, k, a - c[i]) + 1; if (x < best) best = x; }
    return best;
}
static ll ways_rec(const int *c, int k, int idx, int a) {                   /* combinations: choose coin types in non-decreasing index order */
    if (a == 0) return 1;
    ll t = 0;
    for (int i = idx; i < k; i++) if (c[i] <= a) t += ways_rec(c, k, i, a - c[i]);
    return t;
}
static int lis_brute(const int *a, int n) {
    int best = 0; for (unsigned m = 0; m < (1u << n); m++) { int last = -(1 << 30), ok = 1, cnt = 0; for (int i = 0; i < n && ok; i++) if (m >> i & 1) { if (a[i] <= last) ok = 0; last = a[i]; cnt++; } if (ok && cnt > best) best = cnt; } return best;
}
static ll mc_rec(const int *d, int i, int j) {
    if (i == j) return 0;
    ll best = -1;
    for (int k = i; k < j; k++) { ll c = mc_rec(d, i, k) + mc_rec(d, k + 1, j) + (ll)d[i] * d[k + 1] * d[j + 1]; if (best < 0 || c < best) best = c; }
    return best;
}
static int ss_brute(const int *x, int n, int t) { for (unsigned m = 0; m < (1u << n); m++) { int sum = 0; for (int i = 0; i < n; i++) if (m >> i & 1) sum += x[i]; if (sum == t) return 1; } return 0; }

int main(void) {
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 10), cap = (int)(rnd() % 30); int w[10], v[10];
        for (int i = 0; i < n; i++) { w[i] = 1 + (int)(rnd() % 9); v[i] = 1 + (int)(rnd() % 20); }
        assert(knap01(w, v, n, cap) == brute_knap(w, v, n, cap));
        if (n <= 5) assert(knap_unbounded(w, v, n, cap) == unb_enum(w, v, n, 0, cap));
        int coins[4], k = 1 + (int)(rnd() % 3), amount = (int)(rnd() % 12); for (int i = 0; i < k; i++) coins[i] = 1 + (int)(rnd() % 9);
        int cm = coin_min(coins, k, amount), cr = coin_rec(coins, k, amount); assert(cm == (cr >= 1 << 30 ? -1 : cr));
        /* distinct coin values for the combination count (duplicates would count the same combination twice) */
        int dc[4], dk = 0; for (int i = 0; i < k; i++) { int dup = 0; for (int j = 0; j < dk; j++) dup |= dc[j] == coins[i]; if (!dup) dc[dk++] = coins[i]; }
        assert(coin_ways(dc, dk, amount) == ways_rec(dc, dk, 0, amount));
        int a[12], an = 1 + (int)(rnd() % 12); for (int i = 0; i < an; i++) a[i] = (int)(rnd() % 15);
        assert(lis_nlogn(a, an) == lis_brute(a, an)); assert(lis_n2(a, an) == lis_nlogn(a, an));
        int dim[8], mn = 2 + (int)(rnd() % 6); for (int i = 0; i <= mn; i++) dim[i] = 1 + (int)(rnd() % 30);
        assert(matrix_chain(dim, mn) == mc_rec(dim, 0, mn - 1));
        int xs[10], xn = 1 + (int)(rnd() % 10), t = (int)(rnd() % 80); for (int i = 0; i < xn; i++) xs[i] = 1 + (int)(rnd() % 30);
        int sb = ss_brute(xs, xn, t); assert(subset_sum_plain(xs, xn, t) == sb); assert(subset_sum_bitset(xs, xn, t) == sb);
    }
    printf("3000 random instances: every DP agrees with an exhaustive/recursive oracle (0/1 and unbounded knapsack, coin min/ways, LIS both ways, matrix chain, subset sum plain and bitset)\n");
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_dp.c -o test_dp && ./test_dp
3000 random instances: every DP agrees with an exhaustive/recursive oracle (0/1 and unbounded knapsack, coin min/ways, LIS both ways, matrix chain, subset sum plain and bitset)
```

## Demonstrations

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include "dp.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ---- 1. overlapping subproblems: Fibonacci three ways, counting calls ---- */
static unsigned long long calls;
static ll fib_naive(int n) { calls++; return n < 2 ? n : fib_naive(n - 1) + fib_naive(n - 2); }
static ll memo[100];
static ll fib_memo(int n) { calls++; if (n < 2) return n; if (memo[n]) return memo[n]; return memo[n] = fib_memo(n - 1) + fib_memo(n - 2); }

/* ---- 2. greedy vs DP on coin change ---- */
static int greedy_coins(const int *coins_desc, int k, int amount) { int cnt = 0; for (int i = 0; i < k; i++) { cnt += amount / coins_desc[i]; amount %= coins_desc[i]; } return amount ? -1 : cnt; }

/* ---- 3. DP on a tree: maximum weight independent set (no two adjacent chosen).  Children are always numbered higher than parents,
 *         so processing vertices in DECREASING index order is a post-order with no recursion. ---- */
static ll tree_mwis(const int *parent, const int *w, int n) {
    ll *take = malloc((size_t)n * sizeof *take), *skip = calloc((size_t)n, sizeof *skip);
    for (int v = 0; v < n; v++) take[v] = w[v];
    for (int v = n - 1; v > 0; v--) { int p = parent[v]; take[p] += skip[v]; skip[p] += take[v] > skip[v] ? take[v] : skip[v]; }
    ll r = take[0] > skip[0] ? take[0] : skip[0]; free(take); free(skip); return r;
}
static ll mwis_brute(const int *parent, const int *w, int n) {
    ll best = 0;
    for (unsigned m = 0; m < (1u << n); m++) { int ok = 1; ll sum = 0; for (int v = 0; v < n; v++) if (m >> v & 1) { sum += w[v]; if (v && (m >> parent[v] & 1)) ok = 0; } if (ok && sum > best) best = sum; }
    return best;
}

int main(void) {
    printf("== Fibonacci(40): overlapping subproblems ==\n");
    calls = 0; double t0 = now(); ll f = fib_naive(40); double t1 = now() - t0; printf("naive recursion : %lld with %llu calls (%.2f s)\n", f, calls, t1);
    calls = 0; f = fib_memo(40); printf("memoized        : %lld with %llu calls\n", f, calls);
    ll a = 0, b = 1; for (int i = 0; i < 40; i++) { ll c = a + b; a = b; b = c; } printf("bottom-up (O(1) space): %lld\n\n", a);

    printf("== loop direction decides 0/1 vs unbounded knapsack ==\n");
    int w[] = { 3, 4 }, v[] = { 4, 5 };
    printf("items (weight, value): (3, 4) and (4, 5); capacity 10\n");
    printf("capacity loop downward (0/1)   : %lld   (best: 3+4 -> 9, since each item is used once)\n", knap01(w, v, 2, 10));
    printf("capacity loop upward (unbounded): %lld  (3+3+4 -> 4+4+5 = 13: items reused)\n\n", knap_unbounded(w, v, 2, 10));

    printf("== greedy is not DP ==\n");
    int us[] = { 25, 10, 5, 1 }, odd[] = { 4, 3, 1 };
    printf("coins {25,10,5,1}, amount 63: greedy %d, DP %d\n", greedy_coins(us, 4, 63), coin_min((int[]){ 1, 5, 10, 25 }, 4, 63));
    printf("coins {4,3,1},     amount  6: greedy %d, DP %d   (greedy: 4+1+1, optimal: 3+3)\n\n", greedy_coins(odd, 3, 6), coin_min((int[]){ 1, 3, 4 }, 3, 6));

    printf("== space optimization: 0/1 knapsack, n = 2000 items, capacity 50,000 ==\n");
    int n = 2000, cap = 50000; int *ww = malloc((size_t)n * sizeof(int)), *vv = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) { ww[i] = 1 + (int)(rnd() % 500); vv[i] = 1 + (int)(rnd() % 1000); }
    t0 = now(); ll best1 = knap01(ww, vv, n, cap); double t1d = now() - t0;
    ll *dp2 = calloc((size_t)(n + 1) * (size_t)(cap + 1), sizeof *dp2);                 /* the textbook table dp[i][c] */
    t0 = now();
    for (int i = 1; i <= n; i++) for (int c = 0; c <= cap; c++) { ll x = dp2[(size_t)(i - 1) * (cap + 1) + c]; if (c >= ww[i - 1]) { ll y = dp2[(size_t)(i - 1) * (cap + 1) + c - ww[i - 1]] + vv[i - 1]; if (y > x) x = y; } dp2[(size_t)i * (cap + 1) + c] = x; }
    double t2d = now() - t0; assert(dp2[(size_t)n * (cap + 1) + cap] == best1);
    printf("optimum %lld;  2D table: %.0f MB, %.0f ms;   1D array: %.1f MB, %.0f ms\n\n", best1, (double)(n + 1) * (cap + 1) * 8 / 1e6, t2d * 1e3, (double)(cap + 1) * 8 / 1e6, t1d * 1e3);
    free(dp2); free(ww); free(vv);

    printf("== subset sum: byte array vs bitset (3000 even numbers, odd target: unreachable, so no early exit is possible) ==\n");
    int sn = 3000, target = 3000000; int *xs = malloc((size_t)sn * sizeof(int)); for (int i = 0; i < sn; i++) xs[i] = 2 + (int)(rnd() % 2500) * 2;     /* all even; the target is odd -> unreachable, the worst case */
    t0 = now(); int r1 = subset_sum_plain(xs, sn, target + 1); double tp = now() - t0;
    t0 = now(); int r2 = subset_sum_bitset(xs, sn, target + 1); double tb = now() - t0; assert(r1 == r2);
    printf("answer %d;  byte array: %.0f ms;  bitset: %.0f ms (%.0fx faster, 8x less memory for the table)\n\n", r1, tp * 1e3, tb * 1e3, tp / tb);
    free(xs);

    printf("== LIS: O(n^2) DP vs O(n log n) tails array ==\n");
    int ln = 40000; int *arr = malloc((size_t)ln * sizeof(int)); for (int i = 0; i < ln; i++) arr[i] = (int)(rnd() % 1000000);
    t0 = now(); int l1 = lis_n2(arr, ln); double q = now() - t0; t0 = now(); int l2 = lis_nlogn(arr, ln); double nl = now() - t0; assert(l1 == l2);
    printf("n = %d random values: LIS length %d;  O(n^2): %.0f ms;  O(n log n): %.2f ms (%.0fx faster)\n\n", ln, l1, q * 1e3, nl * 1e3, q / nl); free(arr);

    printf("== DP on a tree ==\n");
    for (int round = 0; round < 3000; round++) {
        int tn = 1 + (int)(rnd() % 14), par[16], wt[16]; par[0] = -1; wt[0] = 1 + (int)(rnd() % 20);
        for (int i = 1; i < tn; i++) { par[i] = (int)(rnd() % (unsigned)i); wt[i] = 1 + (int)(rnd() % 20); }
        assert(tree_mwis(par, wt, tn) == mwis_brute(par, wt, tn));
    }
    printf("max-weight independent set: 3000 random trees match exhaustive search\n");
    int big = 2000000; int *bp = malloc((size_t)big * sizeof(int)), *bw = malloc((size_t)big * sizeof(int)); bp[0] = -1; bw[0] = 1;
    for (int i = 1; i < big; i++) { bp[i] = i - 1; bw[i] = 1 + (int)(rnd() % 10); }          /* a path of two million vertices: recursion would overflow the stack */
    printf("path of %d vertices (worst depth): optimum %lld, computed iteratively\n", big, tree_mwis(bp, bw, big)); free(bp); free(bw);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra demo_dp.c -o demo_dp && ./demo_dp
== Fibonacci(40): overlapping subproblems ==
naive recursion : 102334155 with 331160281 calls (0.48 s)
memoized        : 102334155 with 79 calls
bottom-up (O(1) space): 102334155

== loop direction decides 0/1 vs unbounded knapsack ==
items (weight, value): (3, 4) and (4, 5); capacity 10
capacity loop downward (0/1)   : 9   (best: 3+4 -> 9, since each item is used once)
capacity loop upward (unbounded): 13  (3+3+4 -> 4+4+5 = 13: items reused)

== greedy is not DP ==
coins {25,10,5,1}, amount 63: greedy 6, DP 6
coins {4,3,1},     amount  6: greedy 3, DP 2   (greedy: 4+1+1, optimal: 3+3)

== space optimization: 0/1 knapsack, n = 2000 items, capacity 50,000 ==
optimum 368925;  2D table: 800 MB, 345 ms;   1D array: 0.4 MB, 120 ms

== subset sum: byte array vs bitset (3000 even numbers, odd target: unreachable, so no early exit is possible) ==
answer 0;  byte array: 3894 ms;  bitset: 117 ms (33x faster, 8x less memory for the table)

== LIS: O(n^2) DP vs O(n log n) tails array ==
n = 40000 random values: LIS length 390;  O(n^2): 2151 ms;  O(n log n): 1.70 ms (1263x faster)

== DP on a tree ==
max-weight independent set: 3000 random trees match exhaustive search
path of 2000000 vertices (worst depth): optimum 6319731, computed iteratively
```

What the output teaches:

- **Memoization collapses exponential recursion**: 331,160,281 calls vs 79 for the same answer.
- **Loop direction is semantics**: with items (3, 4) and (4, 5) and capacity 10, the downward loop gives 9 (each item once) and the upward loop gives 13 (the weight-3 item twice plus the weight-4 item).
- **Greedy is not DP.** For US coins greedy and DP agree, but for coins `{4, 3, 1}` and amount 6 greedy picks 4+1+1 (3 coins) while the optimum is 3+3 (2 coins). Whether greedy works depends on the structure of the problem — the next note's subject.
- **Space optimization** is dramatic: the textbook `n × cap` table for 2000 items and capacity 50,000 needs 800 MB (and a pass through memory), the single row needs 0.4 MB and runs several times faster because it stays in cache.
- **Bitset subset sum** is an order of magnitude faster than the byte array here even though the compiler vectorises the byte version; it also needs one eighth of the memory. (Bit-parallel DP is a general trick: edit distance, LCS and string matching all have bit-parallel versions.)
- **LIS**: the quadratic DP takes seconds on 40,000 elements; the patience-sorting version takes under 2 ms — a thousand-fold difference from replacing "scan all earlier elements" with "binary search a sorted summary".
- **Tree DP**: for each vertex keep two numbers (best total if this vertex is taken, best if skipped). Because children are numbered after parents, a loop from the last vertex to the first is a post-order traversal with no recursion, so a path of two million vertices (which would overflow a recursive solution's stack) is fine.

## Where DP appears

| Problem family | State | Complexity |
|---|---|---|
| 0/1 and unbounded knapsack | (items used, capacity) | `O(n·W)` — *pseudo-polynomial* |
| Coin change (min coins / ways) | amount | `O(amount·k)` |
| Longest common subsequence, edit distance | (i, j) prefixes of two strings | `O(nm)`; see the string-matching note |
| LIS | tail value per length | `O(n log n)` |
| Matrix chain, optimal BST, polygon triangulation | interval (i, j) | `O(n³)` |
| Shortest paths on DAGs | vertex | `O(V + E)` — this *is* a DP; Bellman–Ford is a DP over path length |
| Tree DP (independent set, diameter, rerooting) | (vertex, state) | `O(n)` |
| Bitmask DP (TSP, assignment) | (visited set, last vertex) | `O(2^n · n²)`; next-but-one note |
| Digit DP, probability and expected value DP | position + tight/loose flag | polynomial in digits |
| Parsing (CYK), Viterbi, sequence alignment | (position, state) | `O(n³)` and `O(nK²)` |

**Pseudo-polynomial** means polynomial in the *numeric value* `W` of an input rather than its bit length: the knapsack DP runs in `O(nW)`, which explodes for `W = 2^60`. Knapsack and subset sum are NP-hard in general; the DP is efficient only when the numbers are small.

## Pitfalls

- **Wrong state.** If the transition needs information the state does not carry (for example, "which items are used", or "was the previous character a match"), either enlarge the state or the recurrence is simply wrong.
- **Loop direction and order** (above): downward for 0/1, upward for unbounded; coins-outer for combinations. Test with a brute-force oracle on tiny inputs, as the verification program does.
- **Sentinel confusion in memo tables**: using 0 as "not computed" when 0 is a legal answer causes recomputation (as in `fib_memo`, where `memo[n]` is never 0 for `n ≥ 2` — in general use a separate flag or −1).
- **Overflow**: counting DPs grow quickly (use 64-bit or modular arithmetic); with an `INF` sentinel never add to it (the same bug as in Floyd–Warshall).
- **Recursion depth** in top-down DP: depth equals the longest dependency chain, which can be `n`; convert to bottom-up or use an explicit stack for large inputs.
- **Table size**: `O(n²)` memory is the usual limit (10⁵ × 10⁵ is impossible); use rolling rows, Hirschberg's divide-and-conquer to recover solutions in linear space, or banded DP.
- **Reconstruction**: keeping a choice array alongside the table is easy; recomputing the path backwards from the table avoids that memory but must follow the same tie-breaking.

## Key takeaways

1. DP = optimal substructure + overlapping subproblems; the work is *state design*, then transition, order and answer.
2. Memoization (top-down) is easiest to write; bottom-up tables are faster and enable rolling-row space optimization.
3. Loop direction and loop nesting order encode the problem's semantics (0/1 vs unbounded; combinations vs sequences).
4. Replace "scan all previous states" with a data structure — binary search, a bitset, a monotone deque — to cut a polynomial factor (LIS: `n²` → `n log n`).
5. Knapsack-type DPs are pseudo-polynomial; always check the magnitude of the numeric parameter.
6. Verify every DP against brute force on tiny random inputs before trusting it.
