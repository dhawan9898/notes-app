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
