---
title: "Caches: LRU, LFU, CLOCK and Belady's OPT"
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 1
level: advanced
summary: "Replacement policies simulated on Zipf, scan-polluted and looping workloads against the offline optimum, plus an O(1) hash-plus-list LRU cache with a pooled allocator, invariant checks and a throughput measurement."
prerequisites:
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
  - Linked Lists
sources:
  - label: "Belady — A Study of Replacement Algorithms for a Virtual-Storage Computer (IBM Systems Journal 5(2), 1966)"
  - label: "Corbató — A Paging Experiment with the Multics System (1968) (origin of the CLOCK algorithm)"
  - label: "Johnson & Shasha — 2Q, A Low Overhead High Performance Buffer Management Replacement Algorithm (VLDB 1994)"
  - label: "Megiddo & Modha — ARC, A Self-Tuning, Low Overhead Replacement Cache (FAST 2003)"
  - label: "Einziger, Friedman, Manes — TinyLFU, A Highly Efficient Cache Admission Policy (ACM Trans. Storage 13(4), 2017)"
  - label: "Sleator & Tarjan — Amortized Efficiency of List Update and Paging Rules (CACM 28(2), 1985)"
  - label: "Linux kernel documentation — Multi-Gen LRU"
    url: https://www.kernel.org/doc/html/latest/admin-guide/mm/multigen_lru.html
  - label: "Redis documentation — Key eviction"
    url: https://redis.io/docs/latest/develop/reference/eviction/
---

A **cache** keeps a small, fast copy of a larger, slower store. When it is full, a **replacement policy** decides which entry to evict. The policy is a bet about the future, and the entire field is the question: *what does the past tell us about the next request?* Hardware caches, the Linux page cache, database buffer pools, CDNs, Redis and memoization tables all face the same decision.

## The policies

| Policy | Evicts | Idea | Cost per hit |
|---|---|---|---|
| FIFO | oldest insertion | trivial | O(1), no update |
| **LRU** | least recently used | recency predicts reuse (temporal locality) | move node to the front |
| **LFU** | least frequently used | popularity predicts reuse | update count / heap |
| **CLOCK** (second chance) | first frame whose reference bit is 0 | LRU approximation | set one bit — cheap and lock-friendly |
| Random | a random entry | surprisingly competitive; no metadata | none |
| 2Q / ARC / LIRS / TinyLFU | combinations of the above | recency *and* frequency, scan resistance, adaptive sizing | more metadata |
| **OPT** (Belady) | key used farthest in the future | needs the future — offline upper bound | n/a |

**Belady's OPT** is the optimal replacement policy and the yardstick: no online policy can beat it, and the gap to OPT is the room for improvement. **CLOCK**, from the Multics paging experiments, keeps frames in a ring with a reference bit; a hit merely sets the bit, and on a miss the "hand" sweeps forward, clearing bits and evicting the first frame whose bit is already clear. It approximates LRU with no list manipulation on the hit path, which matters when many threads hit concurrently — it is the basis of page-replacement in most operating systems.

## Simulating them

All the policies are implemented over a trace of integer keys and compared on three workloads, with OPT as the ceiling. The code is the simulator; each policy returns the per-request hit flags so a workload can report only the requests that matter.

```c
#ifndef POLICIES_H
#define POLICIES_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Cache replacement policies simulated over a trace of integer keys in [0, NK).  Each returns the number of hits.
 * Key-indexed arrays stand in for the hash map (all that matters here is the replacement decision). */
typedef long long ll;

/* ---------- FIFO: evict the oldest insertion ---------- */
static inline ll sim_fifo(const int *tr, size_t n, int NK, int cap, unsigned char *hf) {
    char *in = calloc((size_t)NK, 1); int *q = malloc((size_t)cap * sizeof(int)); int head = 0, size = 0; ll hits = 0;
    for (size_t i = 0; i < n; i++) {
        int k = tr[i]; if (in[k]) { (hits++, hf ? (void)(hf[i] = 1) : (void)0); continue; }
        if (size == cap) { in[q[head]] = 0; head = (head + 1) % cap; size--; }
        q[(head + size) % cap] = k; size++; in[k] = 1;
    }
    free(in); free(q); return hits;
}

/* ---------- LRU: doubly linked list ordered by recency + O(1) lookup.  head = most recent, tail = victim ---------- */
static inline ll sim_lru(const int *tr, size_t n, int NK, int cap, unsigned char *hf) {
    int *prev = malloc((size_t)NK * sizeof(int)), *next = malloc((size_t)NK * sizeof(int)); char *in = calloc((size_t)NK, 1);
    int head = -1, tail = -1, size = 0; ll hits = 0;
    for (size_t i = 0; i < n; i++) {
        int k = tr[i];
        if (in[k]) {                                                      /* hit: unlink and move to the front */
            (hits++, hf ? (void)(hf[i] = 1) : (void)0); if (head == k) continue;
            next[prev[k]] = next[k]; if (next[k] >= 0) prev[next[k]] = prev[k]; else tail = prev[k];
        } else {
            if (size == cap) { int v = tail; in[v] = 0; tail = prev[v]; if (tail >= 0) next[tail] = -1; else head = -1; size--; }
            in[k] = 1; size++;
        }
        prev[k] = -1; next[k] = head; if (head >= 0) prev[head] = k; head = k; if (tail < 0) tail = k;
    }
    free(prev); free(next); free(in); return hits;
}

/* ---------- CLOCK (second chance): a circular buffer of frames, each with a reference bit; the hand clears bits until it finds a 0 ---------- */
static inline ll sim_clock(const int *tr, size_t n, int NK, int cap, unsigned char *hf) {
    int *frame = malloc((size_t)cap * sizeof(int)), *slot = malloc((size_t)NK * sizeof(int)); char *ref = calloc((size_t)cap, 1); int used = 0, hand = 0; ll hits = 0;
    for (int i = 0; i < NK; i++) slot[i] = -1;
    for (size_t i = 0; i < n; i++) {
        int k = tr[i];
        if (slot[k] >= 0) { (hits++, hf ? (void)(hf[i] = 1) : (void)0); ref[slot[k]] = 1; continue; }          /* a hit only sets a bit: no list manipulation, no lock needed on the fast path */
        int s;
        if (used < cap) s = used++;
        else { while (ref[hand]) { ref[hand] = 0; hand = (hand + 1) % cap; } s = hand; slot[frame[s]] = -1; hand = (hand + 1) % cap; }
        frame[s] = k; slot[k] = s; ref[s] = 0;
    }
    free(frame); free(slot); free(ref); return hits;
}

/* ---------- LFU: evict the least frequently used key (ties: least recently used).  Lazy-deletion min-heap of (freq, time, key). ---------- */
typedef struct { ll freq, time; int key; } LE;
static inline int le_less(const LE *a, const LE *b) { return a->freq != b->freq ? a->freq < b->freq : a->time < b->time; }
static inline ll sim_lfu(const int *tr, size_t n, int NK, int cap, unsigned char *hf) {
    ll *freq = calloc((size_t)NK, sizeof(ll)), *last = calloc((size_t)NK, sizeof(ll)); char *in = calloc((size_t)NK, 1);
    size_t hcap = 1 << 20, hn = 0; LE *h = malloc(hcap * sizeof *h); int size = 0; ll hits = 0, now = 0;
    for (size_t i = 0; i < n; i++) {
        int k = tr[i]; now++;
        if (in[k]) (hits++, hf ? (void)(hf[i] = 1) : (void)0);
        else {
            if (size == cap) {
                for (;;) {                                                /* pop until the entry matches the key's CURRENT (freq, time): others are stale */
                    LE top = h[0]; h[0] = h[--hn]; size_t j = 0;
                    for (;;) { size_t c = 2 * j + 1; if (c >= hn) break; if (c + 1 < hn && le_less(&h[c + 1], &h[c])) c++; if (!le_less(&h[c], &h[j])) break; LE t = h[j]; h[j] = h[c]; h[c] = t; j = c; }
                    if (in[top.key] && top.freq == freq[top.key] && top.time == last[top.key]) { in[top.key] = 0; size--; break; }
                }
            }
            in[k] = 1; size++; freq[k] = 0;
        }
        freq[k]++; last[k] = now;
        if (hn == hcap) { hcap *= 2; h = realloc(h, hcap * sizeof *h); }
        h[hn] = (LE){ freq[k], now, k }; size_t j = hn++; while (j && le_less(&h[j], &h[(j - 1) / 2])) { LE t = h[j]; h[j] = h[(j - 1) / 2]; h[(j - 1) / 2] = t; j = (j - 1) / 2; }
        if (hn > 8 * (size_t)cap + 1024) {                                /* rebuild the heap from live entries so stale ones cannot accumulate without bound */
            size_t m = 0; for (int key = 0; key < NK; key++) if (in[key]) h[m++] = (LE){ freq[key], last[key], key };
            hn = m; for (size_t s = hn / 2; s-- > 0; ) { size_t jj = s; for (;;) { size_t c = 2 * jj + 1; if (c >= hn) break; if (c + 1 < hn && le_less(&h[c + 1], &h[c])) c++; if (!le_less(&h[c], &h[jj])) break; LE t = h[jj]; h[jj] = h[c]; h[c] = t; jj = c; } }
        }
    }
    free(freq); free(last); free(in); free(h); return hits;
}

/* ---------- Belady's OPT: evict the key whose next use is farthest in the future (offline; the upper bound for any policy) ---------- */
static inline ll sim_opt(const int *tr, size_t n, int NK, int cap, unsigned char *hf) {
    int *next_use = malloc(n * sizeof(int)), *last_seen = malloc((size_t)NK * sizeof(int)); for (int i = 0; i < NK; i++) last_seen[i] = INT32_MAX;
    for (size_t i = n; i-- > 0; ) { next_use[i] = last_seen[tr[i]]; last_seen[tr[i]] = (int)i; }
    int *nu = malloc((size_t)NK * sizeof(int)); char *in = calloc((size_t)NK, 1);
    typedef struct { int nu, key; } OE; size_t hn = 0, hcap = 1 << 20; OE *h = malloc(hcap * sizeof *h); int size = 0; ll hits = 0;      /* max-heap on next use, lazy deletion */
    for (size_t i = 0; i < n; i++) {
        int k = tr[i];
        if (in[k]) (hits++, hf ? (void)(hf[i] = 1) : (void)0);
        else {
            if (size == cap) {
                for (;;) {
                    OE top = h[0]; h[0] = h[--hn]; size_t j = 0;
                    for (;;) { size_t c = 2 * j + 1; if (c >= hn) break; if (c + 1 < hn && h[c + 1].nu > h[c].nu) c++; if (h[c].nu <= h[j].nu) break; OE t = h[j]; h[j] = h[c]; h[c] = t; j = c; }
                    if (in[top.key] && nu[top.key] == top.nu) { in[top.key] = 0; size--; break; }
                }
            }
            in[k] = 1; size++;
        }
        nu[k] = next_use[i];
        if (hn == hcap) { hcap *= 2; h = realloc(h, hcap * sizeof *h); }
        h[hn] = (OE){ nu[k], k }; size_t j = hn++; while (j && h[j].nu > h[(j - 1) / 2].nu) { OE t = h[j]; h[j] = h[(j - 1) / 2]; h[(j - 1) / 2] = t; j = (j - 1) / 2; }
    }
    free(next_use); free(last_seen); free(nu); free(in); free(h); return hits;
}
#endif
```

```c
#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "policies.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
static double urand(void) { return (double)(rnd() >> 11) / 9007199254740992.0; }

/* Zipf sampler by inverse CDF with a precomputed cumulative table and binary search */
static int *make_zipf(size_t n, int NK, double alpha) {
    double *cdf = malloc((size_t)NK * sizeof *cdf), sum = 0; for (int i = 0; i < NK; i++) { sum += 1.0 / pow(i + 1.0, alpha); cdf[i] = sum; }
    int *perm = malloc((size_t)NK * sizeof(int)); for (int i = 0; i < NK; i++) perm[i] = i;
    for (int i = NK - 1; i > 0; i--) { int j = (int)(rnd() % (uint64_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }   /* popular keys are scattered, not 0,1,2... */
    int *tr = malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) { double u = urand() * sum; int lo = 0, hi = NK - 1; while (lo < hi) { int m = (lo + hi) / 2; if (cdf[m] < u) lo = m + 1; else hi = m; } tr[i] = perm[lo]; }
    free(cdf); free(perm); return tr;
}

/* hot[i] = 1 marks the requests whose hit ratio is reported (NULL = all requests). */
static void table(const char *title, const int *tr, size_t n, int NK, const int *sizes, int ns, const unsigned char *hot) {
    printf("%s\n%-10s %8s %8s %8s %8s %8s\n", title, "cache size", "FIFO", "CLOCK", "LRU", "LFU", "OPT");
    unsigned char *hf = calloc(n, 1); size_t denom = 0; for (size_t i = 0; i < n; i++) denom += hot ? hot[i] : 1;
    for (int i = 0; i < ns; i++) {
        int c = sizes[i]; double r[5];
        for (int p = 0; p < 5; p++) {
            memset(hf, 0, n);
            ll tot = p == 0 ? sim_fifo(tr, n, NK, c, hf) : p == 1 ? sim_clock(tr, n, NK, c, hf) : p == 2 ? sim_lru(tr, n, NK, c, hf) : p == 3 ? sim_lfu(tr, n, NK, c, hf) : sim_opt(tr, n, NK, c, hf);
            ll counted = 0; for (size_t j = 0; j < n; j++) counted += hf[j] && (!hot || hot[j]); (void)tot;
            r[p] = 100.0 * (double)counted / (double)denom;
        }
        assert(r[4] >= r[0] && r[4] >= r[1] && r[4] >= r[2] && r[4] >= r[3]);   /* Belady's OPT is optimal: no policy may beat it */
        printf("%5d (%2.0f%%) %7.1f%% %7.1f%% %7.1f%% %7.1f%% %7.1f%%\n", c, 100.0 * c / NK, r[0], r[1], r[2], r[3], r[4]);
    }
    printf("\n"); free(hf);
}

int main(void) {
    int NK = 100000; size_t n = 2000000; int sizes[] = { 100, 500, 1000, 5000, 20000 };
    /* sanity: the policies on a tiny hand-checkable trace.  Capacity 3, trace 1 2 3 4 1 2 5 1 2 3 4 5 (Belady's anomaly example for FIFO) */
    { int t[] = { 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 };
      printf("tiny trace 1 2 3 4 1 2 5 1 2 3 4 5, capacity 3:  FIFO %lld hits, LRU %lld, CLOCK %lld, LFU %lld, OPT %lld;  FIFO with capacity 4: %lld hits (MORE memory, FEWER hits: Belady's anomaly)\n\n",
             sim_fifo(t, 12, 6, 3, NULL), sim_lru(t, 12, 6, 3, NULL), sim_clock(t, 12, 6, 3, NULL), sim_lfu(t, 12, 6, 3, NULL), sim_opt(t, 12, 6, 3, NULL), sim_fifo(t, 12, 6, 4, NULL)); }

    int *z = make_zipf(n, NK, 1.0);
    table("1. Zipf workload (alpha = 1.0, 100,000 keys, 2M requests): hit ratio over all requests", z, n, NK, sizes, 5, NULL);

    /* scan pollution: 70% Zipf requests, 30% a sequential sweep over 100,000 keys that are each touched once per sweep (backup, table scan) */
    int *mix = malloc(n * sizeof(int)); unsigned char *hot = calloc(n, 1); int scan_pos = 0; int NK2 = 2 * NK;
    for (size_t i = 0; i < n; i++) { if (rnd() % 10 < 3) { mix[i] = NK + scan_pos; scan_pos = (scan_pos + 1) % NK; } else { mix[i] = z[i]; hot[i] = 1; } }
    table("2. Scan pollution: 70% Zipf + 30% sequential sweep; hit ratio of the Zipf (useful) requests only", mix, n, NK2, sizes, 5, hot);

    /* loop: cyclic access over a working set slightly larger than the cache */
    int *loop = malloc(n * sizeof(int)); int L = 1100; for (size_t i = 0; i < n; i++) loop[i] = (int)(i % (size_t)L);
    int lsz[] = { 1000, 1100 };
    table("3. Loop over 1,100 keys (working set slightly larger than a 1,000-entry cache)", loop, n, NK, lsz, 2, NULL);
    free(z); free(mix); free(loop); free(hot); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined sim.c -o sim_asan -lm && ./sim_asan
tiny trace 1 2 3 4 1 2 5 1 2 3 4 5, capacity 3:  FIFO 3 hits, LRU 2, CLOCK 2, LFU 2, OPT 5;  FIFO with capacity 4: 2 hits (MORE memory, FEWER hits: Belady's anomaly)

1. Zipf workload (alpha = 1.0, 100,000 keys, 2M requests): hit ratio over all requests
cache size     FIFO    CLOCK      LRU      LFU      OPT
  100 ( 0%)    25.0%    30.2%    28.9%    40.9%    47.1%
  500 ( 0%)    39.8%    45.1%    43.9%    53.9%    60.7%
 1000 ( 1%)    46.4%    51.7%    50.5%    59.4%    66.4%
 5000 ( 5%)    62.6%    67.4%    66.4%    72.4%    79.5%
20000 (20%)    77.8%    81.5%    80.9%    83.7%    89.6%

2. Scan pollution: 70% Zipf + 30% sequential sweep; hit ratio of the Zipf (useful) requests only
cache size     FIFO    CLOCK      LRU      LFU      OPT
  100 ( 0%)    21.2%    26.5%    25.1%    40.5%    47.1%
  500 ( 0%)    35.0%    40.8%    39.4%    53.7%    60.7%
 1000 ( 0%)    41.2%    46.9%    45.6%    59.2%    66.4%
 5000 ( 2%)    55.7%    61.4%    60.0%    71.6%    79.4%
20000 (10%)    68.2%    73.8%    72.5%    80.9%    87.5%

3. Loop over 1,100 keys (working set slightly larger than a 1,000-entry cache)
cache size     FIFO    CLOCK      LRU      LFU      OPT
 1000 ( 1%)     0.0%     0.0%     0.0%     0.0%    90.9%
 1100 ( 1%)    99.9%    99.9%    99.9%    99.9%    99.9%
```

Reading the results:

**Sanity check — Belady's anomaly.** On the trace `1 2 3 4 1 2 5 1 2 3 4 5` FIFO with capacity 3 gets 3 hits, but with capacity **4 it gets only 2**: more memory, fewer hits. FIFO is not a *stack algorithm*; LRU and OPT are, and for them a bigger cache never hurts. (The simulator also asserts, on every row, that no policy beats OPT.)

**1. Zipf workload.** Real request streams are heavily skewed — a few keys account for most traffic — and a Zipf distribution models that. Here:
- All policies improve steadily with size; even a cache holding 1 % of the keys catches half or more of the requests.
- **LFU is best of the online policies** (up to 12 points above LRU at the smallest sizes, narrowing to about 3 when the cache holds 20 % of the keys), which matches theory: when requests are independent draws from a fixed distribution, keeping the most popular keys is optimal.
- **LRU and CLOCK are within about a point of each other** — CLOCK buys LRU-like behavior with a bit flip — and both beat FIFO clearly, because FIFO evicts popular keys on schedule even when they are being used.
- OPT is another 6–8 points above LFU: the cost of not knowing the future.

**2. Scan pollution.** A single sequential pass over a large data set (a backup, a full table scan, `cat bigfile`) touches many keys exactly once. LRU treats each as "recently used", so the scan **flushes the genuinely hot entries** out of the cache. The second table reports the hit ratio over the Zipf (useful) requests only, while 30 % of all requests are a never-reused sweep: LRU's hot-request hit ratio falls well below the 50.5 % it reached without the sweep (at 1000 entries it is about 45.6 %), FIFO and CLOCK degrade similarly, and **LFU is nearly unaffected** (59.2 % vs 59.4 %) because sweep keys have frequency one and are evicted first. This is why production systems do not use plain LRU:

- The Linux page cache keeps **active** and **inactive** lists so a page must be touched twice to be promoted (a 2Q-like design), and since kernel 6.1 offers the multi-generational LRU (MGLRU), which tracks several generations of pages to estimate working sets (see *Virtual Memory, Paging and the Page Cache*).
- 2Q, ARC, LIRS and TinyLFU add admission control or ghost lists to resist scans while still adapting to recency.
- Databases tag sequential scans so they use a small ring of buffers instead of the whole pool (PostgreSQL does this for large sequential scans).

**3. The loop.** A cyclic access pattern over 1,100 keys with a 1,000-entry cache is LRU's, FIFO's and CLOCK's worst case: each key is evicted just before it is needed again, so the hit ratio is **0 %** — and LFU also fails here. OPT achieves 90.9 % by keeping most of the loop and sacrificing a few keys. With a cache of 1,100 entries every policy reaches 99.9 % (only the first lap misses). A cache that is slightly too small can be worth *nothing* — a cliff, not a slope — which is why capacity planning on working-set size matters more than policy choice.

## An O(1) LRU cache

LRU's textbook implementation combines two structures: a **hash table** for O(1) lookup and a **doubly linked list** ordered by recency for O(1) "move to front" and "evict the tail". This implementation allocates all entries once from a fixed pool (no `malloc`/`free` on the hot path), recycles evicted entries through a free list, and keeps the list and hash table consistent — an invariant checker verifies after tens of thousands of operations that they hold exactly the same set of entries and that the list's back-pointers are right. A differential test requires its hit counts to equal the array-based simulator's on the same trace.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "policies.h"
#include "../../c/perf/timing.h"

/* A production-shaped LRU cache: hash table (separate chaining) for O(1) lookup + intrusive doubly linked list for O(1) recency updates.
 * Entries are allocated once, up front, from a fixed pool (no malloc/free on the hot path) and recycled through a free list. */
typedef struct Entry { uint64_t key, value; struct Entry *hnext; struct Entry *prev, *next; } Entry;
typedef struct { Entry **bucket; size_t nbuckets; Entry *pool, *free_list, *head, *tail; size_t size, cap; unsigned long long hits, misses, evictions; } LRU;

static inline size_t hash_key(uint64_t k) { k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL; k ^= k >> 33; return (size_t)k; }
static LRU *lru_new(size_t cap) {
    LRU *c = calloc(1, sizeof *c); c->cap = cap; c->nbuckets = 1; while (c->nbuckets < cap * 2) c->nbuckets <<= 1;      /* power of two, load factor <= 0.5 */
    c->bucket = calloc(c->nbuckets, sizeof(Entry *)); c->pool = calloc(cap, sizeof(Entry));
    for (size_t i = 0; i < cap; i++) { c->pool[i].hnext = c->free_list; c->free_list = &c->pool[i]; }
    return c;
}
static void lru_free(LRU *c) { free(c->bucket); free(c->pool); free(c); }
static void list_unlink(LRU *c, Entry *e) { if (e->prev) e->prev->next = e->next; else c->head = e->next; if (e->next) e->next->prev = e->prev; else c->tail = e->prev; }
static void list_push_front(LRU *c, Entry *e) { e->prev = NULL; e->next = c->head; if (c->head) c->head->prev = e; c->head = e; if (!c->tail) c->tail = e; }
static Entry *lookup(LRU *c, uint64_t key) { for (Entry *e = c->bucket[hash_key(key) & (c->nbuckets - 1)]; e; e = e->hnext) if (e->key == key) return e; return NULL; }
static int lru_get(LRU *c, uint64_t key, uint64_t *value) {
    Entry *e = lookup(c, key);
    if (!e) { c->misses++; return 0; }
    c->hits++; if (c->head != e) { list_unlink(c, e); list_push_front(c, e); }                          /* touch: move to the front */
    *value = e->value; return 1;
}
static void lru_put(LRU *c, uint64_t key, uint64_t value) {
    Entry *e = lookup(c, key);
    if (e) { e->value = value; if (c->head != e) { list_unlink(c, e); list_push_front(c, e); } return; }
    if (c->size == c->cap) {                                                                             /* evict the tail (least recently used) */
        Entry *v = c->tail; list_unlink(c, v);
        Entry **pp = &c->bucket[hash_key(v->key) & (c->nbuckets - 1)]; while (*pp != v) pp = &(*pp)->hnext; *pp = v->hnext;
        v->hnext = c->free_list; c->free_list = v; c->size--; c->evictions++;
    }
    e = c->free_list; c->free_list = e->hnext; e->key = key; e->value = value;
    Entry **b = &c->bucket[hash_key(key) & (c->nbuckets - 1)]; e->hnext = *b; *b = e; list_push_front(c, e); c->size++;
}
static void lru_check(const LRU *c) {                                                                    /* invariants: list and hash table hold exactly the same entries */
    size_t n = 0; for (const Entry *e = c->head; e; e = e->next) { n++; assert(e->prev ? e->prev->next == e : c->head == e); assert(lookup((LRU *)c, e->key) == e); }
    assert(n == c->size && n <= c->cap); size_t m = 0; for (size_t b = 0; b < c->nbuckets; b++) for (const Entry *e = c->bucket[b]; e; e = e->hnext) m++; assert(m == n);
}

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

int main(void) {
    /* differential test: hit count must equal the array-based LRU simulator on the same trace */
    int NK = 5000; size_t n = 300000; int *tr = malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) { uint64_t r = rnd(); tr[i] = (r & 3) ? (int)(rnd() % 200) : (int)(rnd() % (uint64_t)NK); }          /* hot set of 200 + a cold tail */
    for (int cap = 10; cap <= 1000; cap *= 10) {
        LRU *c = lru_new((size_t)cap); uint64_t v;
        for (size_t i = 0; i < n; i++) { if (!lru_get(c, (uint64_t)tr[i], &v)) lru_put(c, (uint64_t)tr[i], (uint64_t)tr[i] * 7); else assert(v == (uint64_t)tr[i] * 7); if (i % 50000 == 0) lru_check(c); }
        lru_check(c); ll ref = sim_lru(tr, n, NK, cap, NULL); assert((ll)c->hits == ref);
        printf("capacity %4d: %llu hits, %llu evictions; array simulator reports %lld hits (identical)\n", cap, c->hits, c->evictions, ref); lru_free(c);
    }
    free(tr);

    /* throughput */
    size_t N = 20000000; int NK2 = 1000000; uint64_t *keys = malloc(N * sizeof *keys); for (size_t i = 0; i < N; i++) { uint64_t r = rnd(); keys[i] = (r & 3) ? rnd() % 50000 : rnd() % (uint64_t)NK2; }
    LRU *c = lru_new(100000); uint64_t v; double t0 = now();
    for (size_t i = 0; i < N; i++) if (!lru_get(c, keys[i], &v)) lru_put(c, keys[i], keys[i]);
    double dt = now() - t0;
    printf("\n%.0f million requests/s single-threaded, capacity 100,000 (hit ratio %.1f%%, %llu evictions); ~%zu bytes per entry (%zu-byte entry + %.1f buckets)\n",
           (double)N / dt / 1e6, 100.0 * (double)c->hits / (double)N, c->evictions, sizeof(Entry) + (size_t)(8 * c->nbuckets / c->cap), sizeof(Entry), (double)c->nbuckets / (double)c->cap);
    lru_free(c); free(keys); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined lru_cache.c -o lru_asan && ./lru_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra lru_cache.c -o lru_cache && ./lru_cache | tail -1
capacity   10: 8678 hits, 291312 evictions; array simulator reports 8678 hits (identical)
capacity  100: 82265 hits, 217635 evictions; array simulator reports 82265 hits (identical)
capacity 1000: 239613 hits, 59387 evictions; array simulator reports 239613 hits (identical)
34 million requests/s single-threaded, capacity 100,000 (hit ratio 74.8%, 4934062 evictions); ~60 bytes per entry (40-byte entry + 2.6 buckets)
```

The hit counts match the simulator exactly at three capacities; the optimized build handles on the order of 10–20 million requests per second on one thread at about 60 bytes of metadata per entry. Concurrency is the practical problem: a single LRU list is a global lock. Production caches **shard** the keys over many independent LRUs, or move the recency update off the hit path (CLOCK bits, or buffered/batched promotions as in Caffeine and Linux's per-CPU batches).

## Production considerations

- **TTL / expiry**: store a deadline per entry and check on access (lazy), plus a background sweep or timing wheel for proactive removal; expiry and eviction are separate mechanisms.
- **Approximate LRU**: Redis samples a few random keys and evicts the best candidate among them, avoiding a global list entirely (`allkeys-lru`; `allkeys-lfu` uses a probabilistic frequency counter with decay).
- **Admission vs eviction**: TinyLFU compares the candidate's estimated frequency (a Count-Min sketch, see *Probabilistic Structures*) with the victim's and refuses to admit rarer keys — scan resistance without ghost lists.
- **Write policies**: write-through, write-back and write-around decide when the backing store sees updates; **cache stampede** (many misses for one hot key at expiry) is handled by request coalescing or probabilistic early refresh.
- **Size by working set**: measure the hit-ratio-versus-size curve (the "miss ratio curve") on a real trace; the cliff in workload 3 is why. Simulation with sampled traces (SHARDS) makes it cheap.
- **CPU caches** use the same ideas in hardware: set-associative placement with pseudo-LRU (tree bits) because exact LRU for 8 or 16 ways is too costly; the cost of a miss is what makes cache-aware data structures (B-trees, d-ary heaps, CSR graphs earlier in this track) win.

## Pitfalls

- **Measuring on the wrong trace.** Policies rank differently on different workloads; synthetic Zipf is a starting point, not a verdict. Replay real traces.
- **Unbounded metadata**: LFU with a lazy-deletion heap (used here) must be compacted periodically, and ARC/LIRS ghost lists consume memory not counted in the cache size.
- **Counting hits on all requests when part of the stream is one-time** (scans) hides the pollution damage; report the hit ratio of the requests that could have hit.
- **Cache stampede and thundering herd** at expiry or on a cold start.
- **Memory accounting**: an entry's real cost includes key, value, hash-table slots and allocator overhead; "capacity 100,000" is not "100,000 × payload size".
- **Concurrency hidden behind "O(1)"**: a global lock around the list makes a thread-safe LRU slower than a single thread.
- **Stale data**: caching correctness lives in invalidation, not in eviction.

## Key takeaways

1. Replacement is a prediction problem; Belady's OPT, which sees the future, bounds what any policy can achieve.
2. For skewed, independent requests LFU wins; LRU and CLOCK are close to each other and clearly better than FIFO; CLOCK is nearly free on the hit path.
3. Plain LRU is vulnerable to scans and to loops slightly larger than the cache (hit ratio collapses to zero); real systems add admission control or multiple lists (2Q, ARC, MGLRU, TinyLFU).
4. An O(1) LRU is a hash table plus an intrusive doubly linked list over a pre-allocated pool; test it with an invariant checker and a differential simulator.
5. Sizing the cache to the working set matters more than the policy — hit ratio versus size can be a cliff.
6. Evaluate on realistic traces and report hits on requests that could hit.
