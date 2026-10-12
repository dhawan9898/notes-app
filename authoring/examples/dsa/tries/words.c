#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ===== A. 26-way array trie: O(len) lookup, but 26 pointers per node ===== */
typedef struct T26 { struct T26 *c[26]; uint8_t term; } T26;
static size_t t26_nodes = 1;
static void t26_insert(T26 *r, const char *w) {
    for (; *w; w++) { int i = *w - 'a'; if (!r->c[i]) { r->c[i] = calloc(1, sizeof(T26)); t26_nodes++; } r = r->c[i]; }
    r->term = 1;
}
static void t26_collect(const T26 *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (int i = 0; i < 26; i++) if (n->c[i]) { buf[len] = (char)('a' + i); t26_collect(n->c[i], buf, len + 1, out, cnt, max); }
}
static int t26_complete(const T26 *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    for (; *p; p++) { r = r->c[*p - 'a']; if (!r) return 0; buf[len++] = *p; }
    t26_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void t26_free(T26 *n) { for (int i = 0; i < 26; i++) if (n->c[i]) t26_free(n->c[i]); free(n); }

/* ===== B. first-child / next-sibling trie: 24 bytes per node, children scanned linearly (kept sorted) ===== */
typedef struct TS { struct TS *first, *next; char ch; uint8_t term; } TS;
static size_t ts_nodes = 1;
static void ts_insert(TS *r, const char *w) {
    for (; *w; w++) {
        TS **link = &r->first;
        while (*link && (*link)->ch < *w) link = &(*link)->next;
        if (!*link || (*link)->ch != *w) { TS *n = calloc(1, sizeof *n); n->ch = *w; n->next = *link; *link = n; ts_nodes++; }
        r = *link;
    }
    r->term = 1;
}
static void ts_collect(const TS *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (const TS *c = n->first; c; c = c->next) { buf[len] = c->ch; ts_collect(c, buf, len + 1, out, cnt, max); }
}
static int ts_complete(const TS *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    for (; *p; p++) { const TS *c = r->first; while (c && c->ch < *p) c = c->next; if (!c || c->ch != *p) return 0; r = c; buf[len++] = *p; }
    ts_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void ts_free(TS *n) { while (n) { TS *nx = n->next; ts_free(n->first); free(n); n = nx; } }

/* ===== C. compressed radix tree (Patricia-style): edges carry whole substrings, so unary chains collapse ===== */
typedef struct RX { char *lab; int len; uint8_t term; struct RX *first, *next; } RX;
static size_t rx_nodes = 1, rx_label_bytes = 0;
static RX *rx_new(const char *lab, int len) { RX *n = calloc(1, sizeof *n); n->lab = malloc((size_t)len); memcpy(n->lab, lab, (size_t)len); n->len = len; rx_nodes++; rx_label_bytes += (size_t)len; return n; }
static void rx_insert(RX *r, const char *w) {
    for (;;) {
        if (!*w) { r->term = 1; return; }
        RX **link = &r->first;
        while (*link && (*link)->lab[0] < *w) link = &(*link)->next;
        RX *c = *link;
        if (!c || c->lab[0] != *w) { RX *n = rx_new(w, (int)strlen(w)); n->term = 1; n->next = c; *link = n; return; }
        int k = 0; while (k < c->len && w[k] && c->lab[k] == w[k]) k++;
        if (k < c->len) {                                          /* split the edge at k */
            RX *tail = rx_new(c->lab + k, c->len - k); tail->term = c->term; tail->first = c->first; tail->next = NULL;
            c->len = k; c->term = 0; c->first = tail;
        }
        r = c; w += k;
    }
}
static void rx_collect(const RX *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (const RX *c = n->first; c; c = c->next) { memcpy(buf + len, c->lab, (size_t)c->len); rx_collect(c, buf, len + c->len, out, cnt, max); }
}
static int rx_complete(const RX *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    while (*p) {
        const RX *c = r->first; while (c && c->lab[0] < *p) c = c->next;
        if (!c || c->lab[0] != *p) return 0;
        int k = 0; while (k < c->len && p[k] && c->lab[k] == p[k]) k++;
        if (p[k] && k < c->len) return 0;                          /* mismatch inside the edge */
        memcpy(buf + len, c->lab, (size_t)c->len); len += c->len;  /* prefix may end in the middle of an edge: take the whole edge */
        p += k; r = c;
        if (!*p) break;
    }
    rx_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void rx_free(RX *n) { while (n) { RX *nx = n->next; rx_free(n->first); free(n->lab); free(n); n = nx; } }

/* ===== D. sorted array of strings + binary search for the first word >= prefix ===== */
static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int arr_complete(char **w, size_t n, const char *p, char (*out)[32], int max) {
    size_t lo = 0, hi = n, pl = strlen(p);
    while (lo < hi) { size_t m = (lo + hi) / 2; if (strcmp(w[m], p) < 0) lo = m + 1; else hi = m; }
    int cnt = 0; for (; lo < n && cnt < max && strncmp(w[lo], p, pl) == 0; lo++) strcpy(out[cnt++], w[lo]);
    return cnt;
}

int main(void) {
    /* synthetic vocabulary: pronounceable-ish words built from syllables, deduplicated */
    static const char *syl[] = { "ka","lo","mi","ra","te","su","no","vi","an","el","or","ing","tion","er","st","pr","con","de","re","un","ta","ne","po","ly" };
    enum { WANT = 200000 };
    char **raw = malloc(WANT * sizeof *raw); size_t n = 0;
    while (n < WANT) { char w[32] = ""; int parts = 2 + (int)(rnd() % 3); for (int i = 0; i < parts; i++) strcat(w, syl[rnd() % 24]); if (strlen(w) < 30) raw[n++] = strdup(w); }
    qsort(raw, n, sizeof *raw, cmp_str);
    size_t m = 0; for (size_t i = 0; i < n; i++) { if (m && !strcmp(raw[m - 1], raw[i])) free(raw[i]); else raw[m++] = raw[i]; }
    n = m;
    size_t chars = 0; for (size_t i = 0; i < n; i++) chars += strlen(raw[i]);

    T26 *t26 = calloc(1, sizeof *t26); TS *ts = calloc(1, sizeof *ts); RX *rx = calloc(1, sizeof *rx);
    for (size_t i = 0; i < n; i++) { char *w = raw[(i * 7919) % n]; t26_insert(t26, w); ts_insert(ts, w); rx_insert(rx, w); }   /* scrambled insertion order */

    /* correctness: first 10 completions of 5000 random prefixes must be identical in all four structures */
    char (*o1)[32] = malloc(10 * 32), (*o2)[32] = malloc(10 * 32), (*o3)[32] = malloc(10 * 32), (*o4)[32] = malloc(10 * 32);
    int nonempty = 0;
    for (int q = 0; q < 5000; q++) {
        const char *w = raw[rnd() % n]; char p[32]; int pl = 1 + (int)(rnd() % (uint32_t)strlen(w)); memcpy(p, w, (size_t)pl); p[pl] = 0;
        int a = t26_complete(t26, p, o1, 10), b = ts_complete(ts, p, o2, 10), c = rx_complete(rx, p, o3, 10), d = arr_complete(raw, n, p, o4, 10);
        assert(a == b && b == c && c == d); nonempty += a > 0;
        for (int i = 0; i < a; i++) assert(!strcmp(o1[i], o2[i]) && !strcmp(o2[i], o3[i]) && !strcmp(o3[i], o4[i]));
    }
    printf("%zu distinct words (%.1f MB of characters); first-10 completions of 5000 prefixes identical in all structures (%d non-empty)\n\n", n, chars / 1048576.0, nonempty);

    enum { Q = 200000 }; char (*pre)[16] = malloc((size_t)Q * 16);
    for (int i = 0; i < Q; i++) { const char *w = raw[rnd() % n]; int pl = 2 + (int)(rnd() % 4); if (pl > (int)strlen(w)) pl = (int)strlen(w); memcpy(pre[i], w, (size_t)pl); pre[i][pl] = 0; }
    long sink = 0; double t0;
    printf("%-34s %12s %14s %16s\n", "structure", "nodes", "memory", "ns / complete(10)");
    t0 = now(); for (int i = 0; i < Q; i++) sink += t26_complete(t26, pre[i], o1, 10); double a = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "26-way array trie", t26_nodes, t26_nodes * sizeof(T26) / 1048576.0, a);
    t0 = now(); for (int i = 0; i < Q; i++) sink += ts_complete(ts, pre[i], o2, 10); double b = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "first-child/next-sibling trie", ts_nodes, ts_nodes * sizeof(TS) / 1048576.0, b);
    t0 = now(); for (int i = 0; i < Q; i++) sink += rx_complete(rx, pre[i], o3, 10); double c = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "compressed radix tree", rx_nodes, (rx_nodes * sizeof(RX) + rx_nodes * 16 + rx_label_bytes) / 1048576.0, c);
    t0 = now(); for (int i = 0; i < Q; i++) sink += arr_complete(raw, n, pre[i], o4, 10); double d = (now() - t0) / Q * 1e9;
    printf("%-34s %12s %11.1f MB %16.0f\n", "sorted array + binary search", "-", (n * sizeof(char *) + chars + n) / 1048576.0, d);
    printf("\n(radix memory is an estimate: node struct + label bytes + about 16 B of allocator overhead for each of the two allocations per node)\n");
    if (sink == 42) puts("");
    t26_free(t26); ts_free(ts); rx_free(rx); for (size_t i = 0; i < n; i++) free(raw[i]); free(raw); free(o1); free(o2); free(o3); free(o4); free(pre);
    return 0;
}
