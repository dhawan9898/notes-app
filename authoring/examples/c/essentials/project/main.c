#include "table.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORD 63

static int by_count_then_word(const void *x, const void *y) {
    const Entry *a = x, *b = y;
    if (a->count != b->count) return a->count < b->count ? 1 : -1;      /* higher counts first */
    return strcmp(a->word, b->word);                                    /* ties alphabetical, so output is deterministic */
}

/* Split the stream into lower-case words (letters only) and add each one. */
static int count_words(FILE *in, Table *t) {
    char word[MAX_WORD + 1];
    size_t len = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (isalpha(c)) {
            if (len < MAX_WORD) word[len++] = (char)tolower(c);        /* longer words are cut, never overflowed */
        } else if (len > 0) {
            word[len] = '\0'; len = 0;
            if (table_add(t, word) != 0) return -1;
        }
    }
    if (len > 0) { word[len] = '\0'; if (table_add(t, word) != 0) return -1; }
    return ferror(in) ? -1 : 0;
}

int main(int argc, char *argv[]) {
    int top = 5;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) top = atoi(argv[++i]);
        else path = argv[i];
    }
    FILE *in = path ? fopen(path, "r") : stdin;
    if (!in) { perror(path); return 1; }

    Table *t = table_new(64);
    if (!t) { fprintf(stderr, "out of memory\n"); return 1; }
    if (count_words(in, t) != 0) { fprintf(stderr, "error while reading or out of memory\n"); table_free(t); return 1; }
    if (in != stdin) fclose(in);

    size_t n = table_size(t);
    Entry *e = table_entries(t);
    if (!e) { table_free(t); return 1; }
    qsort(e, n, sizeof *e, by_count_then_word);
    for (size_t i = 0; i < n && i < (size_t)top; i++) printf("%4u  %s\n", e[i].count, e[i].word);
    printf("(%zu distinct words)\n", n);
    free(e);
    table_free(t);
    return 0;
}
