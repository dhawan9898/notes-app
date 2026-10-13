// Hash flooding: a chained table keyed by strings, with (a) a weak polynomial hash and (b) SipHash-2-4 with a secret key.
// The attacker's keys all collide under h = 31*h + c (as Java's String.hashCode did), so one bucket gets everything.
#include "siphash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node { struct node *next; char *key; } node;
typedef struct { node **b; size_t nb, n; } table_t;
static uint8_t secret[16] = {0x13,0x37,0xc0,0xde,0x5e,0xed,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};   // random per process in real code

static uint64_t h_poly(const char *s, size_t len) { uint32_t h = 0; for (size_t i = 0; i < len; i++) h = 31 * h + (unsigned char)s[i]; return h; }
static uint64_t h_sip(const char *s, size_t len)  { return siphash24(s, len, secret); }

static void insert(table_t *t, uint64_t (*hash)(const char *, size_t), const char *key, size_t len, size_t *steps) {
    size_t i = hash(key, len) % t->nb;
    for (node *p = t->b[i]; p; p = p->next) { (*steps)++; if (strcmp(p->key, key) == 0) return; }   // search before insert
    node *x = malloc(sizeof *x); x->key = strdup(key); x->next = t->b[i]; t->b[i] = x; t->n++;
}

static void run(const char *name, uint64_t (*hash)(const char *, size_t), char **keys, size_t n) {
    table_t t = { calloc(1 << 16, sizeof(node *)), 1 << 16, 0 };
    size_t steps = 0, longest = 0;
    for (size_t i = 0; i < n; i++) insert(&t, hash, keys[i], strlen(keys[i]), &steps);
    for (size_t i = 0; i < t.nb; i++) { size_t c = 0; for (node *p = t.b[i]; p; p = p->next) c++; if (c > longest) longest = c; }
    printf("  %-22s %7zu keys: %12zu chain steps, longest chain %6zu\n", name, n, steps, longest);
    for (size_t i = 0; i < t.nb; i++) for (node *p = t.b[i], *q; p; p = q) { q = p->next; free(p->key); free(p); }
    free(t.b);
}

// 2^k keys: every key is k blocks, each block "Aa" or "BB" (both hash to 2112 under 31*h + c, and so every concatenation collides)
static char **make_attack(int k, size_t *n) {
    *n = (size_t)1 << k; char **keys = malloc(*n * sizeof *keys);
    for (size_t i = 0; i < *n; i++) {
        char *s = malloc(2 * k + 1);
        for (int b = 0; b < k; b++) { const char *blk = (i >> b) & 1 ? "BB" : "Aa"; s[2*b] = blk[0]; s[2*b+1] = blk[1]; }
        s[2*k] = 0; keys[i] = s;
    }
    return keys;
}

int main(void) {
    printf("SipHash-2-4 self-test (reference vector, key 00..0f, message 00..0e): ");
    uint8_t k[16], msg[15]; for (int i = 0; i < 16; i++) k[i] = i; for (int i = 0; i < 15; i++) msg[i] = i;
    printf("%s\n", siphash24(msg, 15, k) == 0xa129ca6149be45e5ULL ? "ok" : "FAIL");

    for (int bits = 12; bits <= 16; bits += 2) {
        size_t n; char **keys = make_attack(bits, &n);
        printf("attack keys: %zu strings of length %d, all with the same polynomial hash\n", n, 2 * bits);
        run("polynomial 31*h+c", h_poly, keys, n);
        run("SipHash-2-4 (keyed)", h_sip, keys, n);
        for (size_t i = 0; i < n; i++) free(keys[i]);
        free(keys);
    }
    return 0;
}
