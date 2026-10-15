---
kind: question
title: Bloom filter arithmetic
module: Hashing
difficulty: Medium
---

A Bloom filter with `m = 8192` bits and `k = 4` hash functions holds `n = 1000` keys. Use the formula `(1 - e^(-kn/m))^k` to predict the false-positive rate, then build it and measure with 100,000 keys that were never inserted. Can a Bloom filter ever report a stored key as missing?

## Explanation

```c
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define M 8192            /* bits */
#define K 4               /* hash functions */

static uint8_t bits[M / 8];

static uint32_t mix(uint32_t x, uint32_t salt) {            /* a small integer hash; salt gives K different functions */
    x ^= salt * 0x9E3779B9u; x ^= x >> 16; x *= 0x85EBCA6Bu; x ^= x >> 13; x *= 0xC2B2AE35u; x ^= x >> 16;
    return x;
}
static void add(uint32_t key) { for (uint32_t k = 0; k < K; k++) { uint32_t b = mix(key, k + 1) % M; bits[b / 8] |= (uint8_t)(1u << (b % 8)); } }
static int maybe_contains(uint32_t key) {
    for (uint32_t k = 0; k < K; k++) { uint32_t b = mix(key, k + 1) % M; if (!(bits[b / 8] & (1u << (b % 8)))) return 0; }
    return 1;                                                /* all K bits set: probably present */
}

int main(void) {
    const int n = 1000;
    memset(bits, 0, sizeof bits);
    for (int i = 0; i < n; i++) add((uint32_t)i);
    int missed = 0, false_pos = 0, probes = 100000;
    for (int i = 0; i < n; i++) if (!maybe_contains((uint32_t)i)) missed++;
    for (int i = 0; i < probes; i++) if (maybe_contains((uint32_t)(1000000 + i))) false_pos++;
    double predicted = pow(1.0 - exp(-(double)K * n / M), K);
    printf("members reported missing (must be 0): %d\n", missed);
    printf("false positives: %d of %d = %.2f%% (formula predicts %.2f%%)\n", false_pos, probes, 100.0 * false_pos / probes, 100.0 * predicted);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_bloom p4_bloom.c -lm && ./p4_bloom; rm -f p4_bloom
members reported missing (must be 0): 0
false positives: 2259 of 100000 = 2.26% (formula predicts 2.23%)
```

The formula predicts about 2.23%; the measurement lands within a few hundredths of a percent of it. A false positive happens when all `k` bits of a never-inserted key were set by *other* keys. Bits are only ever set, never cleared, so a stored key always finds its `k` bits set: **no false negatives**, the first line prints 0 missing. The trade-off is the table's size: 8 bits per key here gives a 2% error rate, and about 10 bits per key with the optimal `k = (m/n) ln 2` gives 1%. You cannot delete from a plain Bloom filter (clearing a bit would also forget other keys); counting variants exist.
