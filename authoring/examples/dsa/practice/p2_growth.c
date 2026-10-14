#include <stdio.h>

/* Count how many element copies N pushes cost when the array grows by doubling versus by a fixed +10. */
static long copies_for(long pushes, int doubling) {
    long cap = 1, size = 0, copies = 0;
    for (long i = 0; i < pushes; i++) {
        if (size == cap) {                        /* full: allocate a bigger array and copy everything over */
            copies += size;
            cap = doubling ? cap * 2 : cap + 10;
        }
        size++;
    }
    return copies;
}

int main(void) {
    printf("%10s %16s %16s\n", "pushes", "copies (double)", "copies (+10)");
    for (long n = 1000; n <= 1000000; n *= 10)
        printf("%10ld %16ld %16ld\n", n, copies_for(n, 1), copies_for(n, 0));
    return 0;
}
