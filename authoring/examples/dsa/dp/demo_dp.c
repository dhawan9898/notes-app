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
