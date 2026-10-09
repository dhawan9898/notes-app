#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void bye(void) { puts("atexit handler ran"); }

int main(int argc, char **argv) {
    atexit(bye);
    int x = argc > 1 ? atoi(argv[1]) : 1;
    assert(x > 0 && "x must be positive");      /* compiled out with -DNDEBUG: never put side effects in assert */
    printf("x=%d\n", x);
    if (x == 2) exit(3);                        /* runs atexit handlers, flushes stdio */
    if (x == 3) _Exit(4);                       /* does NOT */
    if (x == 4) abort();                        /* raises SIGABRT; no handlers, core dump */
    return 0;
}
