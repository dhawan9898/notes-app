#include <limits.h>
#include <stdio.h>

int main(void) {
    int big = INT_MAX;
    int wrapped = (int)((unsigned)big + 1u);            /* defined: do the arithmetic unsigned, convert back (implementation-defined) */
    printf("computed through unsigned: %d\n", wrapped);
    int big2 = INT_MAX;
    printf("about to overflow a signed int...\n");
    fflush(stdout);
    big2++;                                              /* undefined behaviour: UBSan reports it */
    printf("%d\n", big2);
    return 0;
}
