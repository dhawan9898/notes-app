#include <stdio.h>
#include <stdint.h>

static uintptr_t first;

static void recurse(int depth) {
    volatile char pad[200];                       /* give each frame a known amount of local data */
    pad[0] = (char)depth;
    uintptr_t here = (uintptr_t)&pad[0];
    if (depth == 0) first = here;
    if (depth == 4) {
        printf("stack grew by %lu bytes over 4 calls (about %lu bytes per frame)\n",
               (unsigned long)(first - here), (unsigned long)((first - here) / 4));
        return;
    }
    recurse(depth + 1);
    pad[1] = pad[0];                              /* keep the call from becoming a loop */
}

int main(void) { recurse(0); return 0; }
