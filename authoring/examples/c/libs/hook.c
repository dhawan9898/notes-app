#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Interpose malloc: count calls, then forward to the real implementation. */
static unsigned long calls;
void *malloc(size_t n) {
    static void *(*real)(size_t);
    if (!real) real = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    calls++;
    return real(n);
}
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "[hook] malloc was called %lu times\n", calls);
}
