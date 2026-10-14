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
