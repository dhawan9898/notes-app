#include <stdio.h>
#include <stdint.h>

static void show(int depth) {
    int local = depth;
    printf("depth %d: &local=%p frame=%p return address=%p\n", depth, (void *)&local, __builtin_frame_address(0), __builtin_return_address(0));
    if (depth < 2) show(depth + 1);
}

int main(void) {
    printf("main is at %p, show is at %p\n", (void *)main, (void *)show);
    show(0);
    return 0;
}
