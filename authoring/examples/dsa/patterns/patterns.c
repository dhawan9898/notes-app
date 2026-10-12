#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

/* ===== 1. TWO POINTERS on a sorted array: is there a pair summing to target? O(n) instead of O(n^2) ===== */
static int pair_two_pointers(const int *a, int n, int target) { int i = 0, j = n - 1; while (i < j) { int sum = a[i] + a[j]; if (sum == target) return 1; if (sum < target) i++; else j--; } return 0; }
static int pair_brute(const int *a, int n, int target) { for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (a[i] + a[j] == target) return 1; return 0; }

/* ===== 2. SLIDING WINDOW: longest substring without repeated characters (window [l, r], last-seen index per byte) ===== */
static int longest_unique(const char *t, int n) { int last[256], best = 0, l = 0; for (int i = 0; i < 256; i++) last[i] = -1; for (int r = 0; r < n; r++) { unsigned char c = (unsigned char)t[r]; if (last[c] >= l) l = last[c] + 1; last[c] = r; if (r - l + 1 > best) best = r - l + 1; } return best; }
static int longest_unique_brute(const char *t, int n) { int best = 0; for (int i = 0; i < n; i++) { int seen[256] = {0}; for (int j = i; j < n; j++) { if (seen[(unsigned char)t[j]]++) break; if (j - i + 1 > best) best = j - i + 1; } } return best; }

/* ===== 3. MONOTONIC STACK: for each element, the index of the next strictly greater element (or -1) ===== */
static void next_greater(const int *a, int n, int *out) { int *st = malloc((size_t)n * sizeof(int)), sp = 0; for (int i = 0; i < n; i++) out[i] = -1; for (int i = 0; i < n; i++) { while (sp && a[st[sp - 1]] < a[i]) out[st[--sp]] = i; st[sp++] = i; } free(st); }
static void next_greater_brute(const int *a, int n, int *out) { for (int i = 0; i < n; i++) { out[i] = -1; for (int j = i + 1; j < n; j++) if (a[j] > a[i]) { out[i] = j; break; } } }

/* ===== 4. PREFIX SUM + HASH: number of contiguous subarrays whose sum equals k (works with negative numbers, unlike two pointers) ===== */
typedef struct { long long key; int cnt; int used; } Slot;
static long subarrays_with_sum(const int *a, int n, long long k) {
    size_t cap = 1; while (cap < (size_t)n * 2 + 2) cap <<= 1; Slot *tab = calloc(cap, sizeof *tab); long long pre = 0; long total = 0;
    #define H(x) ((size_t)(((uint64_t)(x) * 0x9e3779b97f4a7c15ULL) >> 20) & (cap - 1))
    size_t i0 = H(0); tab[i0] = (Slot){ 0, 1, 1 };                              /* the empty prefix */
    for (int i = 0; i < n; i++) {
        pre += a[i];
        for (size_t h = H(pre - k);; h = (h + 1) & (cap - 1)) { if (!tab[h].used) break; if (tab[h].key == pre - k) { total += tab[h].cnt; break; } }
        for (size_t h = H(pre);; h = (h + 1) & (cap - 1)) { if (!tab[h].used) { tab[h] = (Slot){ pre, 1, 1 }; break; } if (tab[h].key == pre) { tab[h].cnt++; break; } }
    }
    free(tab); return total;
}
static long subarrays_brute(const int *a, int n, long long k) { long c = 0; for (int i = 0; i < n; i++) { long long sum = 0; for (int j = i; j < n; j++) { sum += a[j]; c += sum == k; } } return c; }

/* ===== 5. MERGING INTERVALS: sort by start, extend the current interval while the next one overlaps ===== */
typedef struct { int l, r; } Seg;
static int cmp_seg(const void *a, const void *b) { return ((const Seg *)a)->l - ((const Seg *)b)->l; }
static int merge_intervals(Seg *v, int n, Seg *out) { qsort(v, (size_t)n, sizeof *v, cmp_seg); int m = 0; for (int i = 0; i < n; i++) { if (m && v[i].l <= out[m - 1].r) { if (v[i].r > out[m - 1].r) out[m - 1].r = v[i].r; } else out[m++] = v[i]; } return m; }
static int covered_brute(const Seg *v, int n) { int c = 0; for (int x = 0; x < 100; x++) { int in = 0; for (int i = 0; i < n; i++) if (v[i].l <= x && x <= v[i].r) in = 1; c += in; } return c; }

/* ===== 6. BINARY SEARCH ON THE ANSWER: minimum capacity of a ship that delivers packages in order within D days (feasibility is monotone in capacity) ===== */
static int feasible(const int *w, int n, int cap, int days) { int d = 1, cur = 0; for (int i = 0; i < n; i++) { if (w[i] > cap) return 0; if (cur + w[i] > cap) { d++; cur = 0; } cur += w[i]; } return d <= days; }
static int min_capacity(const int *w, int n, int days) { int lo = 1, hi = 0; for (int i = 0; i < n; i++) hi += w[i]; while (lo < hi) { int mid = lo + (hi - lo) / 2; if (feasible(w, n, mid, days)) hi = mid; else lo = mid + 1; } return lo; }
static int min_capacity_brute(const int *w, int n, int days) { for (int cap = 1;; cap++) if (feasible(w, n, cap, days)) return cap; }

/* ===== 7. TOP-K with a bounded heap: the k largest of a stream in O(n log k) (see the heaps note); here verified against sorting ===== */
static void topk(const int *a, int n, int k, int *out) {           /* out[0..k) = k largest, kept as a min-heap */
    int h = 0;
    for (int i = 0; i < n; i++) {
        if (h < k) { int j = h++; out[j] = a[i]; while (j && out[(j - 1) / 2] > out[j]) { int t = out[j]; out[j] = out[(j - 1) / 2]; out[(j - 1) / 2] = t; j = (j - 1) / 2; } }
        else if (a[i] > out[0]) { out[0] = a[i]; int j = 0; for (;;) { int c = 2 * j + 1; if (c >= k) break; if (c + 1 < k && out[c + 1] < out[c]) c++; if (out[j] <= out[c]) break; int t = out[j]; out[j] = out[c]; out[c] = t; j = c; } }
    }
}

int main(void) {
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 14), a[16], b[16]; for (int i = 0; i < n; i++) a[i] = (int)(rnd() % 41) - 20;
        memcpy(b, a, sizeof a); qsort(b, (size_t)n, sizeof b[0], cmp_int); int target = (int)(rnd() % 61) - 30;
        assert(pair_two_pointers(b, n, target) == pair_brute(b, n, target));

        char t[20]; int tn = (int)(rnd() % 19); for (int i = 0; i < tn; i++) t[i] = (char)('a' + rnd() % 4);
        assert(longest_unique(t, tn) == longest_unique_brute(t, tn));

        int ng[16], nb[16]; next_greater(a, n, ng); next_greater_brute(a, n, nb); assert(memcmp(ng, nb, (size_t)n * sizeof(int)) == 0);

        long long k = (long long)(rnd() % 21) - 10; assert(subarrays_with_sum(a, n, k) == subarrays_brute(a, n, k));

        Seg v[10], out[10], v2[10]; int sn = 1 + (int)(rnd() % 8); for (int i = 0; i < sn; i++) { v[i].l = (int)(rnd() % 90); v[i].r = v[i].l + (int)(rnd() % 10); }
        memcpy(v2, v, sizeof v); int m = merge_intervals(v, sn, out); int cov = 0; for (int i = 0; i < m; i++) cov += out[i].r - out[i].l + 1; for (int i = 1; i < m; i++) assert(out[i].l > out[i - 1].r);
        assert(cov == covered_brute(v2, sn));

        int w[10], wn = 1 + (int)(rnd() % 10), days = 1 + (int)(rnd() % 5); for (int i = 0; i < wn; i++) w[i] = 1 + (int)(rnd() % 30);
        assert(min_capacity(w, wn, days) == min_capacity_brute(w, wn, days));

        int kk = 1 + (int)(rnd() % 5); if (kk > n) kk = n; int tk[8]; topk(a, n, kk, tk); qsort(tk, (size_t)kk, sizeof tk[0], cmp_int); qsort(b, (size_t)n, sizeof b[0], cmp_int);
        for (int i = 0; i < kk; i++) assert(tk[i] == b[n - kk + i]);
    }
    printf("3000 random instances of each pattern match brute force: two pointers, sliding window, monotonic stack, prefix sums + hash, interval merging, binary search on the answer, top-k heap\n");
    return 0;
}
