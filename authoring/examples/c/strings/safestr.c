#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A length-tracking string builder: never overflows, always terminated. */
typedef struct { char *data; size_t len, cap; } Str;

static int str_reserve(Str *s, size_t need) {                 /* need includes the NUL */
    if (need <= s->cap) return 0;
    size_t cap = s->cap ? s->cap : 16;
    while (cap < need) { if (cap > (size_t)-1 / 2) return -1; cap *= 2; }
    char *p = realloc(s->data, cap);
    if (!p) return -1;
    s->data = p; s->cap = cap;
    return 0;
}

static int str_append(Str *s, const char *src, size_t n) {
    if (n > (size_t)-1 - s->len - 1) return -1;               /* overflow of len + n + 1 */
    if (str_reserve(s, s->len + n + 1)) return -1;
    memcpy(s->data + s->len, src, n);
    s->len += n;
    s->data[s->len] = '\0';
    return 0;
}

static int str_appendf(Str *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static int str_appendf(Str *s, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);                      /* first pass: measure */
    va_end(ap);
    if (n < 0 || str_reserve(s, s->len + (size_t)n + 1)) { va_end(ap2); return -1; }
    vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);     /* second pass: write */
    va_end(ap2);
    s->len += (size_t)n;
    return 0;
}

int main(void) {
    Str s = {0};
    str_append(&s, "user=", 5);
    str_appendf(&s, "%s;uid=%d;", "alice", 1000);
    for (int i = 0; i < 5; i++) str_appendf(&s, "k%d=%08x,", i, i * 0x1111);
    printf("%zu bytes (cap %zu): %s\n", s.len, s.cap, s.data);
    free(s.data);
    return 0;
}
