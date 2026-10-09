#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

static long depth;
static void recurse(void) {
    char pad[256];
    pad[0] = (char)depth;          /* keep the frame alive */
    depth++;
    if (depth % 5000 == 0) { printf("depth %ld\n", depth); fflush(stdout); }
    recurse();
    pad[1] = pad[0];               /* prevent tail-call optimization */
}

int main(void) {
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    printf("stack limit: %lu KiB\n", (unsigned long)rl.rlim_cur / 1024);
    recurse();
    return 0;
}
