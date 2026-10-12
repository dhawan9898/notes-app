#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long u64;
#define MAXSYM 256

/* ---------- build optimal code LENGTHS with the greedy rule: repeatedly merge the two least frequent subtrees ---------- */
typedef struct { u64 w; int left, right; } HNode;          /* leaves are 0..n-1, internal nodes follow */
static void hsift_down(int *h, int n, int i, const HNode *t) {
    int x = h[i];
    for (;;) { int c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && t[h[c + 1]].w < t[h[c]].w) c++; if (t[x].w <= t[h[c]].w) break; h[i] = h[c]; i = c; }
    h[i] = x;
}
static void hsift_up(int *h, int i, const HNode *t) { int x = h[i]; while (i && t[h[(i - 1) / 2]].w > t[x].w) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static void code_lengths(const u64 *freq, int n, int *len) {
    if (n == 1) { len[0] = 1; return; }                        /* a one-symbol alphabet still needs one bit per symbol */
    HNode *t = malloc((size_t)(2 * n) * sizeof *t); int *h = malloc((size_t)n * sizeof(int)), hn = 0, total = n;
    for (int i = 0; i < n; i++) { t[i] = (HNode){ freq[i], -1, -1 }; h[hn] = i; hsift_up(h, hn++, t); }
    while (hn > 1) {
        int a = h[0]; h[0] = h[--hn]; if (hn) hsift_down(h, hn, 0, t);
        int b = h[0]; h[0] = h[--hn]; if (hn) hsift_down(h, hn, 0, t);
        t[total] = (HNode){ t[a].w + t[b].w, a, b }; h[hn] = total; hsift_up(h, hn++, t); total++;
    }
    int root = h[0], *depth = calloc((size_t)total, sizeof(int)), *st = malloc((size_t)total * sizeof(int)), sp = 0; st[sp++] = root;
    while (sp) { int v = st[--sp]; if (t[v].left < 0) len[v] = depth[v]; else { depth[t[v].left] = depth[t[v].right] = depth[v] + 1; st[sp++] = t[v].left; st[sp++] = t[v].right; } }
    free(t); free(h); free(depth); free(st);
}

/* ---------- canonical codes (as in DEFLATE, RFC 1951 3.2.2): codes of equal length are consecutive integers in symbol order, so only lengths need be stored ---------- */
static void canonical(const int *len, int n, uint32_t *code) {
    int maxl = 0; for (int i = 0; i < n; i++) if (len[i] > maxl) maxl = len[i];
    int *count = calloc((size_t)maxl + 2, sizeof(int)); uint32_t *next = calloc((size_t)maxl + 2, sizeof(uint32_t));
    for (int i = 0; i < n; i++) count[len[i]]++;
    uint32_t c = 0; count[0] = 0; for (int l = 1; l <= maxl; l++) { c = (c + (uint32_t)count[l - 1]) << 1; next[l] = c; }
    for (int i = 0; i < n; i++) code[i] = next[len[i]]++;
    free(count); free(next);
}

/* ---------- bit-level encoder / decoder ---------- */
typedef struct { uint8_t *buf; u64 bits; } BitW;
static void put_bits(BitW *w, uint32_t code, int len) { for (int i = len - 1; i >= 0; i--) { if ((w->bits & 7) == 0) w->buf[w->bits >> 3] = 0; if (code >> i & 1) w->buf[w->bits >> 3] |= (uint8_t)(0x80 >> (w->bits & 7)); w->bits++; } }

int main(void) {
    /* ---- an English-like byte stream: 2,000,000 symbols drawn from letter frequencies ---- */
    static const char *pool = "eeeeeeeeeeeeettttttttttaaaaaaaaoooooooiiiiiiinnnnnnnsssssshhhhhhrrrrrrddddllllcccuuummmwwffggyyppbbvkjxqz          ,.\n";
    size_t pl = strlen(pool), N = 2000000; uint8_t *text = malloc(N); uint64_t r = 88172645463325252ULL;
    for (size_t i = 0; i < N; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; text[i] = (uint8_t)pool[(r >> 11) % pl]; }
    u64 freq[256] = {0}; for (size_t i = 0; i < N; i++) freq[text[i]]++;
    int sym[MAXSYM], n = 0; u64 f[MAXSYM]; for (int c = 0; c < 256; c++) if (freq[c]) { sym[n] = c; f[n++] = freq[c]; }
    int len[MAXSYM]; code_lengths(f, n, len);
    /* sort symbols by (length, symbol) is implicit: canonical() assigns codes in index order, so first reorder by length */
    int order[MAXSYM]; for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) { int x = order[i], j = i; while (j && (len[order[j - 1]] > len[x] || (len[order[j - 1]] == len[x] && sym[order[j - 1]] > sym[x]))) { order[j] = order[j - 1]; j--; } order[j] = x; }
    int slen[MAXSYM]; for (int i = 0; i < n; i++) slen[i] = len[order[i]];
    uint32_t scode[MAXSYM]; canonical(slen, n, scode);
    uint32_t code_of[256]; int len_of[256]; for (int i = 0; i < n; i++) { code_of[sym[order[i]]] = scode[i]; len_of[sym[order[i]]] = slen[i]; }

    double kraft = 0, entropy = 0; for (int i = 0; i < n; i++) { kraft += ldexp(1.0, -len[i]); double p = (double)f[i] / (double)N; entropy -= p * log2(p); }
    u64 coded_bits = 0; for (int i = 0; i < n; i++) coded_bits += f[i] * (u64)len[i];
    printf("%zu symbols, %d distinct\n", N, n);
    printf("%-34s %10s\n", "code", "bits/symbol");
    printf("%-34s %10.4f\n", "fixed 8-bit bytes", 8.0);
    printf("%-34s %10.4f\n", "fixed ceil(log2 n) bits", ceil(log2(n)));
    printf("%-34s %10.4f\n", "Huffman (measured)", (double)coded_bits / (double)N);
    printf("%-34s %10.4f   (Shannon entropy: no symbol code can beat it)\n", "entropy H", entropy);
    printf("Kraft sum of the code lengths = %.6f (a complete prefix code sums to exactly 1)\n", kraft);
    printf("code lengths: shortest %d bits, longest %d bits\n", slen[0], slen[n - 1]);

    /* ---- encode, then decode with the canonical decoder ---- */
    BitW w = { malloc((coded_bits + 7) / 8 + 8), 0 };   /* size from the computed total: guessing "N * 4 / 8" overflowed (AddressSanitizer caught it) */
    for (size_t i = 0; i < N; i++) put_bits(&w, code_of[text[i]], len_of[text[i]]);
    printf("encoded size: %llu bits = %llu bytes (%.1f%% of the %zu-byte original)\n", w.bits, (w.bits + 7) / 8, 100.0 * (double)((w.bits + 7) / 8) / (double)N, N);
    int maxl = slen[n - 1], *cnt = calloc((size_t)maxl + 2, sizeof(int)); for (int i = 0; i < n; i++) cnt[slen[i]]++;
    uint32_t *first = calloc((size_t)maxl + 2, sizeof(uint32_t)); int *offset = calloc((size_t)maxl + 2, sizeof(int)); { uint32_t c = 0; int idx = 0; for (int l = 1; l <= maxl; l++) { c <<= 1; first[l] = c; offset[l] = idx; c += (uint32_t)cnt[l]; idx += cnt[l]; } }
    uint8_t *out = malloc(N); size_t produced = 0; u64 pos = 0;
    while (produced < N) {
        uint32_t code = 0; int l = 0;
        for (;;) { code = code << 1 | ((w.buf[pos >> 3] >> (7 - (pos & 7))) & 1); pos++; l++; if (cnt[l] && code - first[l] < (uint32_t)cnt[l]) break; }   /* a code of length l is valid iff it falls in l's consecutive block */
        out[produced++] = (uint8_t)sym[order[offset[l] + (int)(code - first[l])]];
    }
    assert(memcmp(out, text, N) == 0);
    printf("decoded with the canonical decoder: identical to the input\n\n");

    /* ---- optimality: for small alphabets, search every length assignment that satisfies Kraft equality ---- */
    int checked = 0;
    for (int round = 0; round < 2000; round++) {
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        int m = 2 + (int)((r >> 33) % 5); u64 fr[8]; for (int i = 0; i < m; i++) { r = r * 6364136223846793005ULL + 1442695040888963407ULL; fr[i] = 1 + (r >> 33) % 50; }
        int L[8]; code_lengths(fr, m, L); u64 huff = 0; for (int i = 0; i < m; i++) huff += fr[i] * (u64)L[i];
        u64 best = ~0ULL; int lens[8]; int total = 1; for (int i = 0; i < m; i++) total *= (m - 1);
        for (int code = 0; code < total; code++) {
            int c = code; double k = 0; for (int i = 0; i < m; i++) { lens[i] = 1 + c % (m - 1); c /= (m - 1); k += ldexp(1.0, -lens[i]); }
            if (k == 1.0) { u64 cost = 0; for (int i = 0; i < m; i++) cost += fr[i] * (u64)lens[i]; if (cost < best) best = cost; }
        }
        assert(huff == best); checked++;
    }
    printf("%d random alphabets of 2-6 symbols: the greedy Huffman cost equals the minimum over all complete prefix-code length assignments\n", checked);
    free(text); free(w.buf); free(cnt); free(first); free(offset); free(out);
    return 0;
}
