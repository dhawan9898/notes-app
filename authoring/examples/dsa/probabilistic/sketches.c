#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

/* ================= Count-Min sketch: frequency estimates that NEVER underestimate ================= */
typedef struct { uint32_t *cell; size_t w; int d; } CMS;
static CMS cms_new(size_t w, int d) { CMS s = { calloc(w * (size_t)d, sizeof(uint32_t)), w, d }; return s; }
static void cms_add(CMS *s, uint64_t key) { for (int r = 0; r < s->d; r++) s->cell[(size_t)r * s->w + hash64(key, (uint64_t)r) % s->w]++; }
static uint32_t cms_est(const CMS *s, uint64_t key) {
    uint32_t m = UINT32_MAX;
    for (int r = 0; r < s->d; r++) { uint32_t c = s->cell[(size_t)r * s->w + hash64(key, (uint64_t)r) % s->w]; if (c < m) m = c; }
    return m;                                                    /* the minimum over rows is the least-polluted counter */
}

/* ================= HyperLogLog: count distinct elements with ~1.04/sqrt(m) relative error ================= */
typedef struct { uint8_t *reg; int p; size_t m; } HLL;           /* m = 2^p registers of 1 byte each */
static HLL hll_new(int p) { HLL h = { calloc((size_t)1 << p, 1), p, (size_t)1 << p }; return h; }
static void hll_add(HLL *h, uint64_t key) {
    uint64_t x = hash64(key, 99);
    size_t idx = (size_t)(x >> (64 - h->p));                     /* first p bits select the register */
    uint64_t rest = (x << h->p) | ((uint64_t)1 << (h->p - 1));   /* remaining bits; sentinel bit bounds the leading-zero count */
    uint8_t rho = (uint8_t)(__builtin_clzll(rest) + 1);          /* position of the first 1 bit */
    if (rho > h->reg[idx]) h->reg[idx] = rho;
}
static double hll_count(const HLL *h) {
    double alpha = h->m == 16 ? 0.673 : h->m == 32 ? 0.697 : h->m == 64 ? 0.709 : 0.7213 / (1.0 + 1.079 / (double)h->m);
    double sum = 0; size_t zeros = 0;
    for (size_t i = 0; i < h->m; i++) { sum += ldexp(1.0, -h->reg[i]); zeros += h->reg[i] == 0; }
    double est = alpha * (double)h->m * (double)h->m / sum;
    if (est <= 2.5 * (double)h->m && zeros) est = (double)h->m * log((double)h->m / (double)zeros);   /* small-range correction: linear counting */
    return est;
}
static void hll_merge(HLL *dst, const HLL *src) { for (size_t i = 0; i < dst->m; i++) if (src->reg[i] > dst->reg[i]) dst->reg[i] = src->reg[i]; }   /* union = register-wise max */

int main(void) {
    /* ---- Count-Min on a Zipf-like stream: item i appears about 1/i times as often as item 1 ---- */
    size_t items = 100000; double H = 0; for (size_t i = 1; i <= items; i++) H += 1.0 / (double)i;
    size_t stream = 5000000; uint32_t *truth = calloc(items + 1, sizeof *truth);
    CMS s = cms_new(2048, 5);
    uint64_t r = 88172645463325252ULL;
    for (size_t t = 0; t < stream; t++) {
        r ^= r << 13; r ^= r >> 7; r ^= r << 17;
        double u = (double)(r >> 11) / 9007199254740992.0 * H, acc = 0; size_t i = 1;      /* inverse-CDF sampling by scanning: fine for a demo */
        size_t lo = 1, hi = items;                                                         /* binary search on the harmonic prefix sums */
        while (lo < hi) { size_t mid = (lo + hi) / 2; double ph = log((double)mid) + 0.5772156649 + 1.0 / (2.0 * (double)mid); if (ph < u) lo = mid + 1; else hi = mid; }
        i = lo; (void)acc;
        truth[i]++; cms_add(&s, i);
    }
    printf("Count-Min sketch: 5 rows x 2048 counters = %.0f KB for a stream of %zu events over %zu distinct items\n", 5 * 2048 * 4 / 1024.0, stream, items);
    printf("%8s %12s %12s %10s\n", "item", "true count", "estimate", "error");
    size_t probe[] = { 1, 2, 3, 10, 100, 1000, 10000, 100000 };
    for (int i = 0; i < 8; i++) printf("%8zu %12u %12u %9.2f%%\n", probe[i], truth[probe[i]], cms_est(&s, probe[i]), 100.0 * ((double)cms_est(&s, probe[i]) - truth[probe[i]]) / (truth[probe[i]] ? truth[probe[i]] : 1));
    int under = 0; for (size_t i = 1; i <= items; i++) if (cms_est(&s, i) < truth[i]) under++;
    printf("underestimates among all %zu items: %d (guaranteed 0). Error bound: estimate <= true + e*N/w = true + %.0f with probability 1 - e^-5\n\n", items, under, 2.718281828 * (double)stream / 2048);

    /* ---- HyperLogLog accuracy ---- */
    printf("HyperLogLog with p = 14 (16384 one-byte registers = 16 KB); expected relative error 1.04/sqrt(m) = %.2f%%\n", 100 * 1.04 / sqrt(16384.0));
    printf("%14s %14s %10s\n", "true distinct", "estimate", "error");
    size_t sizes[] = { 100, 1000, 10000, 100000, 1000000, 10000000 };
    for (int c = 0; c < 6; c++) {
        HLL h = hll_new(14);
        for (uint64_t i = 0; i < sizes[c]; i++) hll_add(&h, i * 7919 + 13);
        double est = hll_count(&h);
        printf("%14zu %14.0f %9.2f%%\n", sizes[c], est, 100.0 * (est - (double)sizes[c]) / (double)sizes[c]);
        free(h.reg);
    }
    HLL a = hll_new(14), b = hll_new(14);
    for (uint64_t i = 0; i < 600000; i++) hll_add(&a, i);
    for (uint64_t i = 400000; i < 1000000; i++) hll_add(&b, i);
    hll_merge(&a, &b);
    printf("merge of two sketches (overlapping sets of 600k and 600k, union = 1,000,000): estimate %.0f\n", hll_count(&a));
    free(s.cell); free(truth); free(a.reg); free(b.reg);
    return 0;
}
