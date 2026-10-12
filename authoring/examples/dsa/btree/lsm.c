#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A toy LSM tree that counts what a real one would do on disk.
 *   memtable : sorted in-memory array (MEM entries), flushed as an immutable sorted run ("SSTable")
 *   L0       : up to L0_MAX runs, newest first (runs may overlap)
 *   L1..     : ONE sorted run per level, capacity grows by the size ratio T; when a level overflows it is merged into the next
 * Cost model: every entry written to a run counts as 1 write unit; every run consulted by a lookup (binary search) counts as 1 read probe. */
typedef struct { uint32_t key, val; } Entry;
#define TOMB 0xFFFFFFFFu
#define MEM 16384
#define L0_MAX 4
#define MAXLVL 8

typedef struct { Entry *e; size_t n; uint64_t *bloom; size_t bits; } Run;
typedef struct {
    Entry mem[MEM + 1]; size_t mem_n;
    Run l0[L0_MAX]; int l0_n;                           /* l0[0] is the NEWEST */
    Run lv[MAXLVL];                                     /* lv[i] is level i+1 */
    int T, use_bloom;
    unsigned long long user_writes, entries_written, probes, lookups, bloom_skips;
} LSM;

static inline uint64_t mix64(uint64_t x) { x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; x ^= x >> 31; return x; }
static void bloom_add(Run *r, uint32_t key) { for (int i = 0; i < 7; i++) { size_t b = mix64(key + 0x9e3779b97f4a7c15ULL * (uint64_t)(i + 1)) % r->bits; r->bloom[b >> 6] |= 1ull << (b & 63); } }
static int bloom_has(const Run *r, uint32_t key) { for (int i = 0; i < 7; i++) { size_t b = mix64(key + 0x9e3779b97f4a7c15ULL * (uint64_t)(i + 1)) % r->bits; if (!(r->bloom[b >> 6] >> (b & 63) & 1)) return 0; } return 1; }
static void run_finish(Run *r) {                        /* 10 bits per key bloom filter, k = 7 */
    r->bits = r->n * 10 + 64; r->bloom = calloc((r->bits + 63) / 64, 8);
    for (size_t i = 0; i < r->n; i++) bloom_add(r, r->e[i].key);
}
static void run_free(Run *r) { free(r->e); free(r->bloom); *r = (Run){0}; }

/* merge runs (index 0 = newest wins on equal keys); tombstones are dropped only if `bottom` */
static Run merge_runs(LSM *m, Run **in, int cnt, int bottom) {
    size_t total = 0; for (int i = 0; i < cnt; i++) total += in[i]->n;
    Run out = { malloc((total ? total : 1) * sizeof(Entry)), 0, NULL, 0 }; size_t *pos = calloc((size_t)cnt, sizeof *pos);
    for (;;) {
        int best = -1; uint32_t bk = 0;
        for (int i = 0; i < cnt; i++) if (pos[i] < in[i]->n && (best < 0 || in[i]->e[pos[i]].key < bk)) { best = i; bk = in[i]->e[pos[i]].key; }
        if (best < 0) break;
        Entry win = in[best]->e[pos[best]];
        for (int i = 0; i < cnt; i++) if (pos[i] < in[i]->n && in[i]->e[pos[i]].key == bk) pos[i]++;   /* skip older versions */
        if (!(bottom && win.val == TOMB)) out.e[out.n++] = win;
    }
    free(pos); m->entries_written += out.n; run_finish(&out); return out;
}
static int deepest(const LSM *m) { int d = -1; for (int i = 0; i < MAXLVL; i++) if (m->lv[i].n) d = i; return d; }
static size_t level_cap(const LSM *m, int i) { size_t c = (size_t)MEM * L0_MAX; for (int k = 0; k < i; k++) c *= (size_t)m->T; return c; }   /* lv[i] */

static void compact_level(LSM *m, int i);
static void flush(LSM *m) {
    Run r = { malloc(m->mem_n * sizeof(Entry)), m->mem_n, NULL, 0 }; memcpy(r.e, m->mem, m->mem_n * sizeof(Entry)); m->entries_written += r.n; run_finish(&r);
    m->mem_n = 0;
    if (m->l0_n == L0_MAX) {                            /* L0 full: merge all L0 runs (+ level 1) into level 1 */
        Run *in[L0_MAX + 2]; int c = 0; for (int i = 0; i < m->l0_n; i++) in[c++] = &m->l0[i];
        if (m->lv[0].n) in[c++] = &m->lv[0];
        int bottom = deepest(m) <= 0;
        Run out = merge_runs(m, in, c, bottom);
        for (int i = 0; i < m->l0_n; i++) run_free(&m->l0[i]);
        run_free(&m->lv[0]); m->lv[0] = out; m->l0_n = 0;
        compact_level(m, 0);
    }
    memmove(&m->l0[1], &m->l0[0], (size_t)m->l0_n * sizeof(Run)); m->l0[0] = r; m->l0_n++;
}
static void compact_level(LSM *m, int i) {              /* if lv[i] is over capacity, merge it into lv[i+1] */
    while (i + 1 < MAXLVL && m->lv[i].n > level_cap(m, i)) {
        Run *in[2] = { &m->lv[i], &m->lv[i + 1] }; int c = m->lv[i + 1].n ? 2 : 1;
        int bottom = deepest(m) <= i + 1;
        Run out = merge_runs(m, in, c, bottom);
        run_free(&m->lv[i]); run_free(&m->lv[i + 1]); m->lv[i + 1] = out; i++;
    }
}
static size_t mem_lower(const LSM *m, uint32_t key) { size_t lo = 0, hi = m->mem_n; while (lo < hi) { size_t k = (lo + hi) / 2; if (m->mem[k].key < key) lo = k + 1; else hi = k; } return lo; }
static void lsm_put(LSM *m, uint32_t key, uint32_t val) {
    m->user_writes++;
    size_t i = mem_lower(m, key);
    if (i < m->mem_n && m->mem[i].key == key) { m->mem[i].val = val; return; }
    memmove(&m->mem[i + 1], &m->mem[i], (m->mem_n - i) * sizeof(Entry)); m->mem[i] = (Entry){ key, val }; m->mem_n++;
    if (m->mem_n == MEM) flush(m);
}
static int run_get(LSM *m, const Run *r, uint32_t key, uint32_t *val) {
    if (!r->n) return 0;
    if (m->use_bloom && !bloom_has(r, key)) { m->bloom_skips++; return 0; }
    m->probes++;                                        /* we would read a block from this run on disk */
    size_t lo = 0, hi = r->n; while (lo < hi) { size_t k = (lo + hi) / 2; if (r->e[k].key < key) lo = k + 1; else hi = k; }
    if (lo < r->n && r->e[lo].key == key) { *val = r->e[lo].val; return 1; }
    return 0;
}
/* returns 1 if found (value in *val, TOMB means deleted) */
static int lsm_get(LSM *m, uint32_t key, uint32_t *val) {
    m->lookups++;
    size_t i = mem_lower(m, key); if (i < m->mem_n && m->mem[i].key == key) { *val = m->mem[i].val; return 1; }
    for (int r = 0; r < m->l0_n; r++) if (run_get(m, &m->l0[r], key, val)) return 1;
    for (int l = 0; l < MAXLVL; l++) if (run_get(m, &m->lv[l], key, val)) return 1;
    return 0;
}
static size_t stored(const LSM *m) { size_t n = m->mem_n; for (int i = 0; i < m->l0_n; i++) n += m->l0[i].n; for (int i = 0; i < MAXLVL; i++) n += m->lv[i].n; return n; }
static void lsm_free(LSM *m) { for (int i = 0; i < m->l0_n; i++) run_free(&m->l0[i]); for (int i = 0; i < MAXLVL; i++) run_free(&m->lv[i]); }

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    enum { KEYS = 1000000, OPS = 4000000 };
    static uint32_t ref[KEYS];                           /* reference: last value per key, TOMB = absent/deleted, 0xFFFFFFFE = never written */
    printf("%2s %6s | %10s %12s %10s | %10s %10s | %10s %10s\n", "T", "levels", "write amp", "space amp", "stored", "hit probes", "miss probes", "(bloom) hit", "(bloom) miss");
    int Ts[] = { 4, 10 };
    for (int ti = 0; ti < 2; ti++) {
        LSM *m = calloc(1, sizeof *m); m->T = Ts[ti];
        for (int k = 0; k < KEYS; k++) ref[k] = 0xFFFFFFFEu;
        for (long i = 0; i < OPS; i++) {
            uint32_t k = rnd() % KEYS;
            if (rnd() % 10 == 0) { lsm_put(m, k, TOMB); ref[k] = TOMB; } else { uint32_t v = rnd() & 0x7FFFFFFF; lsm_put(m, k, v); ref[k] = v; }
        }
        size_t live = 0; for (int k = 0; k < KEYS; k++) live += ref[k] != 0xFFFFFFFEu && ref[k] != TOMB;
        int levels = deepest(m) + 2;
        double wamp = (double)m->entries_written / (double)m->user_writes, samp = (double)stored(m) / (double)live;
        /* correctness + read cost, with and without bloom filters; hits = keys ever written & alive, misses = keys outside the range */
        double hp[2], mp[2];
        for (int bl = 0; bl < 2; bl++) {
            m->use_bloom = bl; unsigned long long p0, l0; int hits = 0, misses = 0;
            p0 = m->probes; l0 = m->lookups;
            for (int q = 0; q < 200000; q++) {
                uint32_t k = rnd() % KEYS, v; int f = lsm_get(m, k, &v);
                int exp_live = ref[k] != 0xFFFFFFFEu && ref[k] != TOMB;
                assert((f && v != TOMB) == exp_live); if (exp_live) { assert(v == ref[k]); hits++; }
            }
            hp[bl] = (double)(m->probes - p0) / (double)(m->lookups - l0);
            p0 = m->probes; l0 = m->lookups;
            for (int q = 0; q < 200000; q++) { uint32_t v; assert(!lsm_get(m, KEYS + (rnd() % KEYS), &v)); misses++; }
            mp[bl] = (double)(m->probes - p0) / (double)(m->lookups - l0);
            (void)hits; (void)misses;
        }
        printf("%2d %6d | %10.1f %12.2f %10zu | %10.2f %10.2f | %10.2f %10.2f\n", m->T, levels, wamp, samp, stored(m), hp[0], mp[0], hp[1], mp[1]);
        lsm_free(m); free(m);
    }
    printf("\nwrite amp = entries written by flushes and compactions / user writes; space amp = entries stored / live keys;\n");
    printf("probes = runs actually searched per lookup (each would be at least one disk read); %d keys, %d operations (10%% deletes)\n", KEYS, OPS);
    return 0;
}
