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
