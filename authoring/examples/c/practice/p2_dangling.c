#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* BUG: returns the address of a local array. */
static char *make_greeting_bad(const char *name) {
    char buf[32];
    snprintf(buf, sizeof buf, "Hello, %s", name);
    return buf;
}

/* Fix 1: the caller supplies the buffer. */
static void make_greeting_buf(const char *name, char *out, size_t cap) { snprintf(out, cap, "Hello, %s", name); }

/* Fix 2: allocate; the caller must free. */
static char *make_greeting_heap(const char *name) {
    size_t n = strlen("Hello, ") + strlen(name) + 1;
    char *s = malloc(n);
    if (s) snprintf(s, n, "Hello, %s", name);
    return s;
}

int main(void) {
    char mine[32];
    make_greeting_buf("Ada", mine, sizeof mine);
    printf("%s\n", mine);
    char *h = make_greeting_heap("Grace");
    if (h) printf("%s\n", h);
    free(h);
#ifdef SHOW_BUG
    char *bad = make_greeting_bad("Linus");
    printf("%s\n", bad);                              /* undefined behaviour: the stack frame is gone */
#else
    (void)make_greeting_bad;
#endif
    return 0;
}
