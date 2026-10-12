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
