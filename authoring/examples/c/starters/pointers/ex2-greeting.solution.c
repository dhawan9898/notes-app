void make_greeting_buf(const char *name, char *out, size_t cap) {
    snprintf(out, cap, "Hello, %s", name);
}

char *make_greeting_heap(const char *name) {
    size_t n = strlen("Hello, ") + strlen(name) + 1;
    char *s = malloc(n);
    if (s) snprintf(s, n, "Hello, %s", name);
    return s;
}
