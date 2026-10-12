#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ===== 1. Interval scheduling: choose the maximum number of pairwise non-overlapping intervals.
 *          Greedy: sort by END time, take each interval that starts after the last one taken ended. ===== */
typedef struct { int start, end; } Iv;
static int cmp_end(const void *a, const void *b) { return ((const Iv *)a)->end - ((const Iv *)b)->end; }
static int schedule_greedy(Iv *iv, int n) {
    qsort(iv, (size_t)n, sizeof *iv, cmp_end);
    int cnt = 0, last_end = -1; for (int i = 0; i < n; i++) if (iv[i].start >= last_end) { cnt++; last_end = iv[i].end; }
    return cnt;
}
static int schedule_brute(const Iv *iv, int n) {
    int best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {
        int ok = 1, cnt = 0;
        for (int i = 0; i < n && ok; i++) if (m >> i & 1) { cnt++; for (int j = i + 1; j < n; j++) if ((m >> j & 1) && iv[i].start < iv[j].end && iv[j].start < iv[i].end) { ok = 0; break; } }
        if (ok && cnt > best) best = cnt;
    }
    return best;
}
/* two plausible-looking greedy rules that are WRONG: earliest start, shortest duration */
static int cmp_start(const void *a, const void *b) { return ((const Iv *)a)->start - ((const Iv *)b)->start; }
static int cmp_len(const void *a, const void *b) { return (((const Iv *)a)->end - ((const Iv *)a)->start) - (((const Iv *)b)->end - ((const Iv *)b)->start); }
static int schedule_by(Iv *iv, int n, int (*cmp)(const void *, const void *)) {
    qsort(iv, (size_t)n, sizeof *iv, cmp); int cnt = 0, last_end = -1;
    for (int i = 0; i < n; i++) if (iv[i].start >= last_end) { cnt++; last_end = iv[i].end; }
    return cnt;
}

/* ===== 2. Fractional knapsack: greedy by value/weight ratio is optimal (0/1 knapsack by ratio is NOT) ===== */
typedef struct { double w, v; } Item;
static int cmp_ratio(const void *a, const void *b) { double x = ((const Item *)a)->v / ((const Item *)a)->w, y = ((const Item *)b)->v / ((const Item *)b)->w; return x < y ? 1 : x > y ? -1 : 0; }
static double fractional(Item *it, int n, double cap) {
    qsort(it, (size_t)n, sizeof *it, cmp_ratio); double total = 0;
    for (int i = 0; i < n && cap > 0; i++) { double take = it[i].w < cap ? it[i].w : cap; total += it[i].v * take / it[i].w; cap -= take; }
    return total;
}
static double fractional_brute(const Item *it, int n, double cap) {      /* an optimum has at most one fractional item: try every full set + one fractional extra */
    double best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {
        double w = 0, v = 0; for (int i = 0; i < n; i++) if (m >> i & 1) { w += it[i].w; v += it[i].v; }
        if (w > cap) continue;
        if (v > best) best = v;
        for (int j = 0; j < n; j++) if (!(m >> j & 1)) { double room = cap - w, take = room < it[j].w ? room : it[j].w; double tv = v + it[j].v * take / it[j].w; if (tv > best) best = tv; }
    }
    return best;
}

/* ===== 3. Job sequencing with deadlines: unit-time jobs, each with a deadline and profit; maximize profit.
 *          Greedy: consider jobs by decreasing profit, place each in the LATEST free slot <= its deadline (union-find finds free slots). ===== */
typedef struct { int deadline, profit; } Job;
static int cmp_profit(const void *a, const void *b) { return ((const Job *)b)->profit - ((const Job *)a)->profit; }
static int find(int *p, int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
static long jobs_greedy(Job *j, int n, int maxd) {
    qsort(j, (size_t)n, sizeof *j, cmp_profit);
    int *p = malloc((size_t)(maxd + 1) * sizeof(int)); for (int i = 0; i <= maxd; i++) p[i] = i;   /* p[t] = latest free slot <= t (0 = none) */
    long total = 0;
    for (int i = 0; i < n; i++) { int d = j[i].deadline < maxd ? j[i].deadline : maxd, slot = find(p, d); if (slot > 0) { total += j[i].profit; p[slot] = slot - 1; } }
    free(p); return total;
}
static long jobs_brute(const Job *j, int n) {
    long best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {                              /* a set is feasible iff after sorting by deadline the k-th job has deadline >= k */
        int cnt[16] = {0}, k = 0; long profit = 0; for (int i = 0; i < n; i++) if (m >> i & 1) { cnt[j[i].deadline < 15 ? j[i].deadline : 15]++; k++; profit += j[i].profit; }
        int ok = 1, upto = 0; for (int d = 0; d < 16; d++) { upto += cnt[d]; if (upto > d) ok = 0; }
        if (ok && profit > best) best = profit;
    }
    return best;
}

/* ===== 4. Minimum number of meeting rooms = maximum overlap: sort starts and ends separately, sweep ===== */
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static int rooms(const Iv *iv, int n) {
    int *st = malloc((size_t)n * sizeof(int)), *en = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) { st[i] = iv[i].start; en[i] = iv[i].end; }
    qsort(st, (size_t)n, sizeof(int), cmp_int); qsort(en, (size_t)n, sizeof(int), cmp_int);
    int cur = 0, best = 0, j = 0; for (int i = 0; i < n; i++) { while (en[j] <= st[i]) { j++; cur--; } cur++; if (cur > best) best = cur; }
    free(st); free(en); return best;
}
static int rooms_brute(const Iv *iv, int n) { int best = 0; for (int t = 0; t < 64; t++) { int c = 0; for (int i = 0; i < n; i++) c += iv[i].start <= t && t < iv[i].end; if (c > best) best = c; } return best; }

int main(void) {
    int diff_start = 0, diff_len = 0;
    for (int round = 0; round < 5000; round++) {
        int n = 1 + (int)(rnd() % 12); Iv iv[12], c1[12], c2[12], c3[12];
        for (int i = 0; i < n; i++) { iv[i].start = (int)(rnd() % 30); iv[i].end = iv[i].start + 1 + (int)(rnd() % 12); }
        memcpy(c1, iv, sizeof iv); memcpy(c2, iv, sizeof iv); memcpy(c3, iv, sizeof iv);
        int opt = schedule_brute(iv, n); assert(schedule_greedy(c1, n) == opt);
        if (schedule_by(c2, n, cmp_start) != opt) diff_start++;
        if (schedule_by(c3, n, cmp_len) != opt) diff_len++;
        assert(rooms(iv, n) == rooms_brute(iv, n));
    }
    printf("interval scheduling, 5000 random instances: earliest-FINISH-first equals the brute-force optimum every time;\n");
    printf("  the plausible rules 'earliest start' and 'shortest interval' were suboptimal in %d and %d of them.  Meeting-room sweep == max overlap.\n\n", diff_start, diff_len);
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 7); Item it[7], c[7]; double cap = 1 + (double)(rnd() % 30);
        for (int i = 0; i < n; i++) { it[i].w = 1 + (double)(rnd() % 12); it[i].v = 1 + (double)(rnd() % 30); }
        memcpy(c, it, sizeof it); double g = fractional(c, n, cap), b = fractional_brute(it, n, cap);
        assert(g - b < 1e-9 && b - g < 1e-9);
    }
    printf("fractional knapsack, 3000 random instances: greedy by value/weight ratio equals the exhaustive optimum\n");
    {   /* the textbook counterexample for the 0/1 version: greedy by ratio vs exhaustive search */
        Item it[3] = { {10, 60}, {20, 100}, {30, 120} }; double cap = 50, w = 0, v = 0, best = 0;
        qsort(it, 3, sizeof *it, cmp_ratio); for (int i = 0; i < 3; i++) if (w + it[i].w <= cap) { w += it[i].w; v += it[i].v; }        /* whole items only */
        for (unsigned m = 0; m < 8; m++) { double W = 0, V = 0; for (int i = 0; i < 3; i++) if (m >> i & 1) { W += it[i].w; V += it[i].v; } if (W <= cap && V > best) best = V; }
        printf("  but 0/1 knapsack (whole items only), capacity 50, items (w,v) = (10,60) (20,100) (30,120): ratio-greedy gets %.0f, the optimum is %.0f\n\n", v, best); }
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 10); Job j[10], c[10]; for (int i = 0; i < n; i++) { j[i].deadline = 1 + (int)(rnd() % 6); j[i].profit = 1 + (int)(rnd() % 50); }
        memcpy(c, j, sizeof j); assert(jobs_greedy(c, n, 12) == jobs_brute(j, n));
    }
    printf("job sequencing with deadlines, 3000 random instances: greedy (highest profit first, latest free slot) equals brute force\n");
    return 0;
}
