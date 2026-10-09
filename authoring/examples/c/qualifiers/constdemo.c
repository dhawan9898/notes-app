#include <stdio.h>
int main(void) {
    const int ro = 5;
    int *p = (int *)&ro;           /* casting away const and writing is UB */
    *p = 6;
    fprintf(stderr, "ro=%d *p=%d\n", ro, *p);   /* the compiler may print 5 and 6 for the same object */
    char *s = "literal";
    s[0] = 'L';                    /* UB: string literal lives in read-only memory */
    fprintf(stderr, "%s\n", s);
    return 0;
}
