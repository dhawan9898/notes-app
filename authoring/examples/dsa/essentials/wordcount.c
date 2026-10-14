#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUCKETS 8                                    /* tiny on purpose, so you can see collisions */

typedef struct entry { char *word; int count; struct entry *next; } Entry;
static Entry *table[BUCKETS];

static unsigned hash(const char *s) {                /* djb2: turns text into a number */
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static char *copy_string(const char *s) {           /* strdup is not standard C17, so do it by hand */
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) exit(1);
    return memcpy(p, s, n);
}

static void add(const char *word) {
    unsigned b = hash(word) % BUCKETS;               /* which bucket (list) to use */
    for (Entry *e = table[b]; e; e = e->next)
        if (strcmp(e->word, word) == 0) { e->count++; return; }      /* seen before: bump the count */
    Entry *e = malloc(sizeof *e);
    if (!e) exit(1);
    e->word = copy_string(word); e->count = 1; e->next = table[b];        /* new word: add to the bucket's chain */
    table[b] = e;
}

static int lookup(const char *word) {
    for (Entry *e = table[hash(word) % BUCKETS]; e; e = e->next)
        if (strcmp(e->word, word) == 0) return e->count;
    return 0;
}

int main(void) {
    const char *text[] = {"the", "cat", "sat", "on", "the", "mat", "the", "cat", "ran", "to", "a", "mat"};
    int n = (int)(sizeof text / sizeof text[0]);
    for (int i = 0; i < n; i++) add(text[i]);

    printf("the=%d cat=%d mat=%d dog=%d\n", lookup("the"), lookup("cat"), lookup("mat"), lookup("dog"));
    for (int b = 0; b < BUCKETS; b++) {
        printf("bucket %d:", b);
        for (Entry *e = table[b]; e; e = e->next) printf(" %s(%d)", e->word, e->count);
        printf("\n");
    }
    for (int b = 0; b < BUCKETS; b++) for (Entry *e = table[b], *next; e; e = next) { next = e->next; free(e->word); free(e); }
    return 0;
}
