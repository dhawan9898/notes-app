int sb_init(Builder *b) {
    b->cap = 16;
    b->len = 0;
    b->data = malloc(b->cap);
    if (!b->data) return 0;
    b->data[0] = '\0';
    return 1;
}

int sb_append(Builder *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        while (b->len + n + 1 > cap) cap *= 2;
        char *bigger = realloc(b->data, cap);
        if (!bigger) return 0;              /* the old buffer is still valid and still ours */
        b->data = bigger;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
    return 1;
}
