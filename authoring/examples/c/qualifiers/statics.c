#include <stdio.h>
static int next_id(void) { static int id = 100; return id++; }      /* persists across calls, initialized once */
_Thread_local int per_thread = 7;                                    /* one copy per thread */
int main(void) {
    int a = next_id(), b = next_id(), c = next_id();                 /* separate statements: argument order is unspecified */
    printf("%d %d %d\n", a, b, c);
    printf("per_thread=%d\n", per_thread);
    return 0;
}
