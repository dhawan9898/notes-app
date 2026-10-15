---
kind: question
title: Judge a hash function
module: Hashing
difficulty: Medium
---

Hash 2,000 pseudo-random lowercase words into 256 buckets using (a) the word's length, (b) its first letter, (c) djb2 (`h = h*33 + c`, starting at 5381). For each, count the buckets used and the longest chain. Predict the ranking and say what the *ideal* chain length is.

## Explanation

```c
#include <stdio.h>
#include <string.h>

#define BUCKETS 256
#define WORDS 2000

static unsigned h_length(const char *s) { return (unsigned)strlen(s); }                 /* bad: only ~10 distinct values */
static unsigned h_first(const char *s) { return (unsigned char)s[0]; }                    /* bad: 26 distinct values at most */
static unsigned h_djb2(const char *s) { unsigned h = 5381; while (*s) h = h * 33 + (unsigned char)*s++; return h; }

static void measure(const char *name, unsigned (*h)(const char *), char (*words)[16]) {
    int count[BUCKETS] = {0}, used = 0, longest = 0;
    for (int i = 0; i < WORDS; i++) count[h(words[i]) % BUCKETS]++;
    for (int b = 0; b < BUCKETS; b++) { if (count[b]) used++; if (count[b] > longest) longest = count[b]; }
    printf("%-14s buckets used %3d of %d, longest chain %4d (ideal about %d)\n", name, used, BUCKETS, longest, WORDS / BUCKETS);
}

int main(void) {
    static char words[WORDS][16];
    unsigned seed = 7;                                           /* fixed seed: same words every run */
    for (int i = 0; i < WORDS; i++) {
        seed = seed * 1103515245u + 12345u;
        int len = 3 + (int)((seed >> 16) % 8);
        for (int j = 0; j < len; j++) { seed = seed * 1103515245u + 12345u; words[i][j] = (char)('a' + (seed >> 16) % 26); }
        words[i][len] = '\0';
    }
    measure("length", h_length, words);
    measure("first letter", h_first, words);
    measure("djb2", h_djb2, words);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_hash p4_hash.c && ./p4_hash; rm -f p4_hash
length         buckets used   8 of 256, longest chain  260 (ideal about 7)
first letter   buckets used  26 of 256, longest chain   90 (ideal about 7)
djb2           buckets used 256 of 256, longest chain   15 (ideal about 7)
```

With 2,000 keys in 256 buckets a perfect spread gives about 8 per bucket (the table prints the integer 7). Hashing by **length** can produce only about 8 distinct values (words here are 3 to 10 letters), so 8 buckets hold everything and the longest chain is 260: lookups degrade toward a linear scan. **First letter** allows at most 26 values: 26 buckets, chains up to 90. **djb2** uses every bucket and the longest chain is 15, close to what random placement gives (the maximum of 256 random bucket loads exceeds the mean by a few standard deviations). A good hash function must use *all* the input and spread the output *evenly*; "cheap to compute" is not enough.
