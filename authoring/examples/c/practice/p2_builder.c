#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t len, cap; } Builder;

static int sb_init(Builder *b) { b->cap = 16; b->len = 0; b->data = malloc(b->cap); if (!b->data) return 0; b->data[0] = '\0'; return 1; }

static int sb_append(Builder *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        while (b->len + n + 1 > cap) cap *= 2;                      /* grow geometrically: amortised O(1) per byte */
        char *bigger = realloc(b->data, cap);
        if (!bigger) return 0;                                      /* keep the old buffer valid on failure */
        b->data = bigger; b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
    return 1;
}

int main(void) {
    Builder b;
    if (!sb_init(&b)) return 1;
    for (int i = 1; i <= 5; i++) {
        char piece[32];
        snprintf(piece, sizeof piece, "item-%d;", i);
        if (!sb_append(&b, piece)) return 1;
    }
    printf("\"%s\" length %zu capacity %zu\n", b.data, b.len, b.cap);
    free(b.data);
    return 0;
}
