---
title: "Probabilistic Structures: Bloom, Count-Min, HyperLogLog and Consistent Hashing"
track: dsa
module: Hashing
moduleOrder: 4
order: 2
level: advanced
summary: "Trading exactness for orders of magnitude less memory — hash quality and avalanche, Bloom filters sized from the formulas, Count-Min sketches, HyperLogLog and consistent hashing, each measured against its theory."
sources:
  - label: "Bloom — Space/Time Trade-offs in Hash Coding with Allowable Errors (CACM 13(7), 1970)"
  - label: "Cormode & Muthukrishnan — An Improved Data Stream Summary, the Count-Min Sketch and its Applications (J. Algorithms 55(1), 2005)"
  - label: "Flajolet, Fusy, Gandouet, Meunier — HyperLogLog, the analysis of a near-optimal cardinality estimation algorithm (AofA 2007)"
  - label: "Karger et al. — Consistent Hashing and Random Trees (STOC 1997)"
  - label: "Kirsch & Mitzenmacher — Less Hashing, Same Performance, Building a Better Bloom Filter (2006)"
    url: https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf
  - label: "Appleby — MurmurHash3 and SMHasher (finalizer mix constants)"
  - label: "Aumasson & Bernstein — SipHash, a fast short-input PRF (INDOCRYPT 2012)"
    url: https://www.aumasson.jp/siphash/siphash.pdf
  - label: "Mitzenmacher & Upfal — Probability and Computing, Ch. 5 and 14"
---

A hash table answers "is x in the set?" exactly, at the price of storing every key. Many systems do not need exactness: a database wants to skip a disk read when a key is *definitely* absent, a network monitor wants the top talkers among billions of packets, an analytics pipeline wants the number of distinct visitors. All of these can be answered from a few kilobytes if you accept a small, **quantified** error. The structures here share one idea — hash the item, and keep only a tiny, lossy summary of the hash — and one discipline: the error is not "approximately right"; it has a proven bound, and we can measure that it holds.

## 1. Everything starts with hash quality

A sketch is only as good as the independence of its hash functions. The standard test is **avalanche**: flipping one input bit should flip each output bit with probability ½, so on average 32 of 64 output bits change. Weak hashes fail this in ways that silently break the guarantees below.

```c
#ifndef HASH_H
#define HASH_H
#include <stdint.h>
#include <string.h>

/* SplitMix64 / Murmur3 finalizer style mixing: every input bit affects every output bit with probability ~1/2 (avalanche). */
static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
/* Seeded 64-bit hash of a 64-bit key. */
static inline uint64_t hash64(uint64_t key, uint64_t seed) { return mix64(key + 0x9e3779b97f4a7c15ULL * (seed + 1)); }

/* FNV-1a over bytes: simple, but weak avalanche on short inputs. */
static inline uint64_t fnv1a(const void *p, size_t n) {
    const unsigned char *s = p; uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= s[i]; h *= 1099511628211ULL; }
    return h;
}
#endif
```

`mix64` is the SplitMix64 / Murmur3-style finalizer: two multiply-xorshift rounds. `hash64(key, seed)` derives an independent-looking family from one function by seeding — the usual trick for "k hash functions" (Kirsch & Mitzenmacher show two suffice for Bloom filters, but with a strong mixer, seeding is simpler to reason about).

```c
#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

typedef uint64_t (*hfn)(uint64_t);
static uint64_t h_identity(uint64_t x) { return x; }
static uint64_t h_modmul(uint64_t x) { return x * 0x9e3779b97f4a7c15ULL; }                 /* Fibonacci hashing: one multiply */
static uint64_t h_fnv(uint64_t x) { return fnv1a(&x, sizeof x); }
static uint64_t h_mix(uint64_t x) { return mix64(x); }

int main(void) {
    struct { const char *name; hfn f; } fs[] = { {"identity", h_identity}, {"x * golden ratio", h_modmul}, {"FNV-1a (8 bytes)", h_fnv}, {"mix64 (splitmix)", h_mix} };
    printf("avalanche test: flip ONE input bit, count how many of the 64 output bits change (ideal: 32 on average)\n");
    printf("%-18s %12s %12s %20s\n", "hash", "avg flipped", "worst bit", "min/max over input bits");
    uint64_t r = 88172645463325252ULL;
    for (int k = 0; k < 4; k++) {
        double total = 0; long trials = 0; double bit_avg[64] = {0};
        for (int t = 0; t < 20000; t++) {
            r ^= r << 13; r ^= r >> 7; r ^= r << 17;
            uint64_t x = r, hx = fs[k].f(x);
            for (int b = 0; b < 64; b++) { int d = __builtin_popcountll(hx ^ fs[k].f(x ^ (1ULL << b))); total += d; trials++; bit_avg[b] += d; }
        }
        double mn = 64, mx = 0;
        for (int b = 0; b < 64; b++) { bit_avg[b] /= 20000; if (bit_avg[b] < mn) mn = bit_avg[b]; if (bit_avg[b] > mx) mx = bit_avg[b]; }
        printf("%-18s %12.2f %12.2f %12.1f / %.1f\n", fs[k].name, total / trials, mn, mn, mx);
    }
    /* low bits matter: tables index with (hash & mask).  Keys that are multiples of 1024 collide in the low 10 bits for weak hashes. */
    printf("\nbucket spread for 100000 keys that are multiples of 1024, into 1024 buckets using the LOW 10 bits of the hash:\n");
    for (int k = 0; k < 4; k++) {
        static int count[1024]; for (int i = 0; i < 1024; i++) count[i] = 0;
        for (uint64_t i = 0; i < 100000; i++) count[fs[k].f(i * 1024) & 1023]++;
        int used = 0, mx = 0; for (int i = 0; i < 1024; i++) { if (count[i]) used++; if (count[i] > mx) mx = count[i]; }
        printf("  %-18s buckets used: %4d / 1024, fullest bucket: %6d (ideal ~98)\n", fs[k].name, used, mx);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra avalanche.c -o avalanche && ./avalanche
avalanche test: flip ONE input bit, count how many of the 64 output bits change (ideal: 32 on average)
hash                avg flipped    worst bit min/max over input bits
identity                   1.00         1.00          1.0 / 1.0
x * golden ratio          16.03         1.00          1.0 / 31.2
FNV-1a (8 bytes)          25.56         9.19          9.2 / 33.0
mix64 (splitmix)          32.00        31.92         31.9 / 32.1

bucket spread for 100000 keys that are multiples of 1024, into 1024 buckets using the LOW 10 bits of the hash:
  identity           buckets used:    1 / 1024, fullest bucket: 100000 (ideal ~98)
  x * golden ratio   buckets used:    1 / 1024, fullest bucket: 100000 (ideal ~98)
  FNV-1a (8 bytes)   buckets used: 1024 / 1024, fullest bucket:    102 (ideal ~98)
  mix64 (splitmix)   buckets used: 1024 / 1024, fullest bucket:    129 (ideal ~98)
```

Read the table carefully:

- **Identity** changes exactly 1 output bit per input bit flip: perfect locality, zero mixing. Fine for a table indexed by small dense integers; fatal for sketches.
- **Multiplying by the golden ratio** (Fibonacci hashing) spreads information *upward*: high output bits depend on many input bits, but the low bits depend only on the low input bits. Used correctly — take the **high** bits — it is a good, very cheap table index function. Used with the low bits it is useless, as the bucket test shows: every multiple of 1024 lands in one bucket.
- **FNV-1a** is byte-at-a-time and passes the bucket test, but its avalanche is incomplete on short inputs (some input bits move only ~9 output bits).
- **`mix64`** is within noise of ideal on every input bit.

Two more points. First, for *adversarial* inputs (network-facing hash tables), no fixed function is safe: an attacker who knows it can craft colliding keys and degrade a table to O(n) per operation (hash flooding). The defence is a keyed pseudo-random function — **SipHash** is what Python, Rust and the Linux kernel use for this — with a random per-process key. Second, the guarantees of the structures below are proven for *random* hash functions; a good mixer approximates that for non-adversarial data, and the experiments here check that it does.

## 2. Bloom filter: set membership in ~10 bits per key

A Bloom filter is an array of `m` bits and `k` hash functions. **Insert** sets the `k` bits `h_i(x) mod m`. **Query** reports "possibly present" only if all `k` bits are set. There are no false negatives (an inserted key's bits are never cleared), but false positives occur when other keys happened to set all `k` bits.

**The math.** After inserting `n` keys, a given bit is still 0 with probability `(1 − 1/m)^{kn} ≈ e^{−kn/m}`. A false positive needs `k` set bits, so

`p ≈ (1 − e^{−kn/m})^k`.

For fixed `m/n` this is minimized at `k = (m/n)·ln 2`, which makes exactly half of the bits set, giving `p = 2^{−k}`. Inverting for a target rate `p`:

- bits per key `m/n = −ln p / (ln 2)²  ≈ 1.44·log2(1/p)`
- hash count `k = log2(1/p)`

Each factor of 10 in accuracy costs 4.79 more bits per key. The information-theoretic minimum for a lossy set at rate `p` is `log2(1/p)` bits per key, so a Bloom filter is a factor 1.44 above optimal — the price of being simple and mergeable (union of two filters is a bitwise OR).

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

typedef struct { uint64_t *bits; size_t m; int k; } Bloom;      /* m bits, k hash functions */

static Bloom bloom_new(size_t m, int k) { Bloom b = { calloc((m + 63) / 64, 8), m, k }; return b; }
/* Kirsch-Mitzenmacher double hashing: k indices from just two hashes, h_i = h1 + i*h2 (no loss of asymptotic false-positive rate). */
static inline size_t bloom_idx(const Bloom *b, uint64_t h1, uint64_t h2, int i) { return (size_t)((h1 + (uint64_t)i * h2) % b->m); }
static void bloom_add(Bloom *b, uint64_t key) {
    uint64_t h1 = hash64(key, 1), h2 = hash64(key, 2) | 1;
    for (int i = 0; i < b->k; i++) { size_t j = bloom_idx(b, h1, h2, i); b->bits[j >> 6] |= 1ULL << (j & 63); }
}
static int bloom_maybe(const Bloom *b, uint64_t key) {
    uint64_t h1 = hash64(key, 1), h2 = hash64(key, 2) | 1;
    for (int i = 0; i < b->k; i++) { size_t j = bloom_idx(b, h1, h2, i); if (!(b->bits[j >> 6] >> (j & 63) & 1)) return 0; }
    return 1;
}

int main(void) {
    size_t n = 1000000;                                         /* items inserted */
    printf("Bloom filter, n = %zu inserted keys, false-positive rate measured with 2,000,000 absent keys\n", n);
    printf("%10s %8s %10s %14s %14s %12s\n", "bits/key", "k", "memory", "theory", "measured", "set bits");
    double bpk[] = { 4, 8, 10, 12, 16 };
    for (int c = 0; c < 5; c++) {
        size_t m = (size_t)(bpk[c] * (double)n);
        int kopt = (int)round(bpk[c] * log(2.0));              /* optimal k = (m/n) ln 2 */
        Bloom b = bloom_new(m, kopt);
        for (uint64_t i = 0; i < n; i++) bloom_add(&b, i);
        size_t fp = 0, trials = 2000000;
        for (uint64_t i = 0; i < trials; i++) fp += (size_t)bloom_maybe(&b, (1ULL << 40) + i);
        size_t ones = 0; for (size_t w = 0; w < (m + 63) / 64; w++) ones += (size_t)__builtin_popcountll(b.bits[w]);
        double theory = pow(1.0 - exp(-(double)kopt * (double)n / (double)m), kopt);
        printf("%10.0f %8d %7.2f MB %13.4f%% %13.4f%% %11.1f%%\n", bpk[c], kopt, (double)m / 8e6, theory * 100, 100.0 * (double)fp / (double)trials, 100.0 * (double)ones / (double)m);
        /* no false negatives, ever */
        for (uint64_t i = 0; i < n; i += 997) if (!bloom_maybe(&b, i)) { puts("FALSE NEGATIVE!"); return 1; }
        free(b.bits);
    }
    printf("\nsizing: for target false-positive rate p, bits per key = -ln(p)/(ln 2)^2 and k = -log2(p):\n");
    double ps[] = { 0.1, 0.01, 0.001, 0.0001 };
    for (int i = 0; i < 4; i++) printf("  p = %-7g -> %5.2f bits/key, k = %2.0f   (a hash set of 64-bit keys needs >= 64 bits/key)\n", ps[i], -log(ps[i]) / (log(2.0) * log(2.0)), ceil(-log2(ps[i])));
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bloom.c -o bloom && ./bloom
/bin/ld: /tmp/ccLhSXJ8.o: in function `main':
bloom.c:(.text.startup+0x2c3): undefined reference to `exp'
/bin/ld: bloom.c:(.text.startup+0x2de): undefined reference to `pow'
/bin/ld: bloom.c:(.text.startup+0x45b): undefined reference to `round'
/bin/ld: bloom.c:(.text.startup+0x4a9): undefined reference to `log2'
/bin/ld: bloom.c:(.text.startup+0x51e): undefined reference to `log'
collect2: error: ld returned 1 exit status
```

The measured false-positive rates match the formula to within the sampling noise of 2 million probes, and with `k` chosen optimally about half the bits are set, as predicted. The sizing table shows why people use them: a filter with a 1 % false-positive rate needs about 9.6 bits per key *regardless of key size* — whereas a hash set of 64-bit keys needs at least 64 bits, and a set of URLs or database rows far more.

**Where it earns its keep.** LSM-tree databases (LevelDB, RocksDB, Cassandra, HBase) keep a Bloom filter per on-disk table: a lookup for an absent key consults the in-memory filters and skips almost every disk read. CDNs use them to avoid caching "one-hit wonders" (cache an object only on its second request). Browsers and mail systems have used them as a fast pre-filter for blocklists, falling back to an exact check on a hit.

**Limits and variants.**

- No deletion: clearing a bit may remove other keys. A **counting Bloom filter** uses 4-bit counters instead of bits (4× the space) to support delete. **Cuckoo filters** store short fingerprints in a cuckoo hash table, support deletion, and beat Bloom filters below roughly 3 % false-positive rates.
- The filter must be **sized in advance**; if `n` exceeds the design capacity, `p` degrades quickly (the formula above with the real `n`). Scalable variants chain filters.
- Access is `k` random memory reads — `k` cache misses for a large filter. *Blocked* Bloom filters confine all `k` bits to one 64-byte cache line at the cost of a slightly higher false-positive rate.

## 3. Count-Min sketch: frequencies in sublinear space

A Count-Min sketch (CMS) is a `d × w` array of counters with one hash function per row. **Update(x)** increments `cell[r][h_r(x)]` in every row. **Estimate(x)** returns the **minimum** over the rows. Collisions only ever *add* to a counter, so every cell is ≥ the true count and the minimum is the least-polluted one: the estimate **never underestimates**.

**The guarantee.** With `w = ⌈e/ε⌉` and `d = ⌈ln(1/δ)⌉`, for a stream of total weight `N` the estimate satisfies

`true ≤ estimate ≤ true + ε·N` with probability at least `1 − δ`.

(Proof sketch: in one row the expected pollution of a cell is at most `N/w`; Markov's inequality gives pollution `> e·N/w` with probability `< 1/e`; the `d` rows are independent so all fail with probability `< e^{−d}`.)

The key subtlety is that the bound is **additive in N**, not relative to the item's own count. It is a heavy-hitter tool: items whose frequency is well above `εN` are estimated with small relative error, while rare items can be wildly overestimated. The experiment makes that concrete with a Zipf-distributed stream (the realistic shape of word frequencies, flow sizes, and page views).

```c
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
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra sketches.c -o sketches -lm && ./sketches
Count-Min sketch: 5 rows x 2048 counters = 40 KB for a stream of 5000000 events over 100000 distinct items
    item   true count     estimate      error
       1       445180       446422      0.28%
       2       182706       183329      0.34%
       3       132413       132855      0.33%
      10        41500        42114      1.48%
     100         4002         4758     18.89%
    1000          428         1098    156.54%
   10000           31          898   2796.77%
  100000            6          719  11883.33%
underestimates among all 100000 items: 0 (guaranteed 0). Error bound: estimate <= true + e*N/w = true + 6636 with probability 1 - e^-5

HyperLogLog with p = 14 (16384 one-byte registers = 16 KB); expected relative error 1.04/sqrt(m) = 0.81%
 true distinct       estimate      error
           100            100      0.31%
          1000           1000     -0.01%
         10000           9918     -0.82%
        100000          99370     -0.63%
       1000000         991284     -0.87%
      10000000       10020899      0.21%
merge of two sketches (overlapping sets of 600k and 600k, union = 1,000,000): estimate 993906
```

The first block is the Count-Min result. Observe:

1. **Zero underestimates** out of 100,000 items — the one-sided guarantee holds exactly, not just probabilistically.
2. The top items (the heavy hitters) are accurate to well under 1 %, from 40 KB of counters for a 5-million-event stream.
3. For the rare items the *relative* error is enormous (the item seen 6 times is reported as hundreds). The *absolute* error stays within the `e·N/w` bound printed on the last line — which is why the bound on its own tells you nothing about rare items. This is not a bug; it is the contract. Use CMS to find and rank heavy hitters (with a small heap alongside to track the top-k), never to count the long tail.

Two practical refinements: **conservative update** (increment only the cells equal to the current minimum) greatly reduces overestimation with the same memory; and sketches are **mergeable** — cell-wise addition of two sketches built with the same hash functions is the sketch of the combined stream, which is what makes them suitable for distributed aggregation.

## 4. HyperLogLog: counting distinct items in 16 KB

Counting distinct elements exactly needs memory proportional to the answer. HyperLogLog (HLL) estimates it with a relative standard error of `1.04/√m` using `m` tiny registers.

**The idea.** Hash each item to a uniform bit string. The probability that a hash starts with `j−1` zeros followed by a 1 is `2^{−j}`; so if the longest run of leading zeros seen is `ρ`, there are likely about `2^ρ` distinct items. A single such estimate has huge variance, so HLL splits the stream into `m = 2^p` sub-streams using the first `p` hash bits, keeps the maximum `ρ` per sub-stream in a register (one byte is plenty — `ρ ≤ 64`), and combines them with a **harmonic mean**, which damps the outliers from lucky large `ρ` values:

`E = α_m · m² / Σ 2^{−M[j]}`, with `α_m ≈ 0.7213/(1 + 1.079/m)`.

For small cardinalities (`E ≤ 2.5m`) with some empty registers, HLL switches to *linear counting*, `m·ln(m/V)` where `V` is the number of empty registers; this is the correction in the code above. Duplicates never change the state (the max is idempotent), and **union is the register-wise max**, so sketches from many machines merge exactly.

The second block of the output above uses `p = 14`: 16,384 registers, 16 KB, theoretical standard error `1.04/√16384 = 0.81 %`. The errors measured at cardinalities from 100 to 10 million are all within about one standard deviation of that, while the same structure would need ~80 MB as an exact set of 64-bit hashes at 10 million elements. The merge line shows two overlapping sets combining into an estimate of their union without any access to the originals. Production systems (Redis `PFADD`/`PFCOUNT`, BigQuery `APPROX_COUNT_DISTINCT`, Presto, Druid) use exactly this structure, usually with the bias corrections of HLL++ for mid-range cardinalities.

A caution on trust: the guarantee is on the *standard error* — an individual estimate can be about two standard errors (~1.6 % here) off. Since the single-run table above is one sample, the honest reading is "consistent with 0.8 %", not "always under 1 %".

## 5. Consistent hashing: sharding without a global reshuffle

You have `N` cache or storage servers and must decide which one holds a key. The obvious rule `server = hash(key) mod N` is perfectly balanced — and catastrophic when `N` changes, because almost every key maps somewhere new (a cache stampede on the database, or a mass data migration).

**Consistent hashing** (Karger et al., 1997) places both servers and keys on a circular hash space; a key belongs to the first server point clockwise from it. Adding or removing a server only moves the keys in the arc it takes over: an expected `1/N` of them. A lookup is a binary search over the sorted ring points.

One point per server gives terrible balance (arc lengths of random points vary by a factor of ~log N). The fix is **virtual nodes**: each server owns many points, which averages the arc lengths. Load imbalance falls like `1/√(points per server)`.

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

/* Hash ring: each node owns `vn` points; a key belongs to the first point clockwise from hash(key). */
typedef struct { uint64_t pos; int node; } Pt;
typedef struct { Pt *p; size_t n; } Ring;

static int cmp_pt(const void *a, const void *b) { uint64_t x = ((const Pt *)a)->pos, y = ((const Pt *)b)->pos; return x < y ? -1 : x > y; }
static Ring ring_build(int nodes, int vn) {
    Ring r = { malloc((size_t)nodes * (size_t)vn * sizeof(Pt)), 0 };
    for (int nd = 0; nd < nodes; nd++)
        for (int v = 0; v < vn; v++) r.p[r.n++] = (Pt){ hash64((uint64_t)nd * 1000003u + (uint64_t)v, 7), nd };
    qsort(r.p, r.n, sizeof(Pt), cmp_pt);
    return r;
}
static int ring_lookup(const Ring *r, uint64_t key) {      /* binary search: first point >= h, wrapping to 0 */
    uint64_t h = hash64(key, 3); size_t lo = 0, hi = r->n;
    while (lo < hi) { size_t m = (lo + hi) / 2; if (r->p[m].pos < h) lo = m + 1; else hi = m; }
    return r->p[lo == r->n ? 0 : lo].node;
}

int main(void) {
    enum { NODES = 10, KEYS = 1000000 };
    printf("Load balance: %d keys over %d nodes (ideal %d per node)\n", KEYS, NODES, KEYS / NODES);
    printf("%14s %12s %12s %12s\n", "points/node", "min load", "max load", "max/ideal");
    int vns[] = { 1, 10, 100, 1000 };
    for (int c = 0; c < 4; c++) {
        Ring r = ring_build(NODES, vns[c]); long load[NODES] = {0};
        for (uint64_t k = 0; k < KEYS; k++) load[ring_lookup(&r, k)]++;
        long mn = load[0], mx = load[0]; for (int i = 1; i < NODES; i++) { if (load[i] < mn) mn = load[i]; if (load[i] > mx) mx = load[i]; }
        printf("%14d %12ld %12ld %11.2fx\n", vns[c], mn, mx, (double)mx / (KEYS / NODES));
        free(r.p);
    }
    printf("\nAdding an 11th node: how many keys change owner?\n");
    long moved_mod = 0, moved_ring = 0;
    Ring a = ring_build(10, 200), b = ring_build(11, 200);
    for (uint64_t k = 0; k < KEYS; k++) {
        if (hash64(k, 3) % 10 != hash64(k, 3) % 11) moved_mod++;
        if (ring_lookup(&a, k) != ring_lookup(&b, k)) moved_ring++;
    }
    printf("  hash %% N      : %6.2f%% of keys move\n", 100.0 * (double)moved_mod / KEYS);
    printf("  hash ring     : %6.2f%% of keys move (ideal 1/11 = %.2f%%)\n", 100.0 * (double)moved_ring / KEYS, 100.0 / 11);
    free(a.p); free(b.p);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra consistent.c -o consistent -lm && ./consistent
Load balance: 1000000 keys over 10 nodes (ideal 100000 per node)
   points/node     min load     max load    max/ideal
             1        25090       224922        2.25x
            10        61299       150635        1.51x
           100        89298       117277        1.17x
          1000        94925       103564        1.04x

Adding an 11th node: how many keys change owner?
  hash % N      :  90.89% of keys move
  hash ring     :   9.36% of keys move (ideal 1/11 = 9.09%)
```

Two results to take away:

- **Balance**: with a single point per node, the busiest node carries over twice its fair share and the lightest only a quarter. Every ×10 in virtual nodes improves the spread roughly by √10; at 1000 points per node the busiest node is within a few percent of ideal. The price is ring size (`nodes × points` entries, 16 bytes each here) and a slower rebuild.
- **Movement**: adding the 11th node with `hash mod N` relocates about 91 % of the keys (a key stays only if `h mod 10 = h mod 11`, which happens about 1 time in 11). The ring moves about 9 %, close to the ideal `1/11`, and *only to the new node*.

Real systems: Dynamo-style stores (Cassandra, Riak), memcached client libraries (ketama), and CDN request routing use it. Variants address its weaknesses — **rendezvous (highest-random-weight) hashing** needs no ring and gives perfect balance for small `N` at O(N) cost per lookup; **jump consistent hash** (Lamping & Veach) is a 5-line, memory-free function when buckets are numbered 0..N−1 and only grow or shrink at the end; **bounded-load** consistent hashing caps any node at `(1+ε)×` the average.

## 6. Choosing among them

| Question | Structure | Memory | Error |
|---|---|---|---|
| Have I seen x? (set membership) | Bloom / Cuckoo filter | ~1.44·log2(1/p) bits per key | false positives at rate p, never false negatives |
| How often did x occur? | Count-Min sketch | `w·d` counters | overestimate ≤ ε·N with prob. 1−δ |
| How many distinct items? | HyperLogLog | `m` bytes | std. error 1.04/√m |
| Which node owns x, resilient to resizing? | Consistent / rendezvous hashing | ring of `nodes × vnodes` | none (balance only) |
| Top-k heavy hitters? | Count-Min + heap, or Misra–Gries / Space-Saving | O(k) to O(1/ε) | bounded undercount/overcount |

## Pitfalls

- **Weak hash, broken guarantees.** Low-bit use of a multiplicative hash, correlated seeds (e.g. `h(x) + i`), or a hash that ignores part of the key will make measured error far worse than the formulas. Always run an experiment like the ones above on your real data.
- **Sizing from the wrong `n`.** Bloom filters and CMS accuracy depend on the number of items actually inserted; plan for peak, not average.
- **Treating "possibly present" as "present".** A Bloom hit must lead to the authoritative lookup; the filter only removes work on the negative path.
- **Serializing sketches.** To merge or persist sketches across processes, the hash function, seed and layout must be identical and versioned — otherwise the merge silently produces garbage.
- **Adversarial inputs.** Everything here assumes keys are not chosen by an attacker who knows your hash; use a keyed hash (SipHash) if they might be.

## Key takeaways

1. These structures keep a lossy summary of hashes and prove an error bound; the value is memory (bits per item instead of bytes) and mergeability.
2. A Bloom filter needs `1.44·log2(1/p)` bits per key and `k = log2(1/p)` hashes — about 10 bits and 7 hashes for 1 %. Measured rates match the formula; there are no false negatives and no deletes.
3. Count-Min never underestimates, and its error is additive in the *stream size*, so it is for heavy hitters, not the long tail.
4. HyperLogLog estimates distinct counts with 1.04/√m relative error from `m` bytes and merges by register-wise max.
5. `hash mod N` moves almost all keys when `N` changes; consistent hashing moves ~1/N, and virtual nodes buy balance at the cost of ring size.
6. Hash quality is the foundation: test avalanche, use high bits of multiplicative hashes, and use a keyed hash against adversaries.
