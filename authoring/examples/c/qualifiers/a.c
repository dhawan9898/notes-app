#include <stdio.h>
int counter;                      /* tentative definition in a.c */
static int hidden = 1;            /* internal linkage */
void bump(void);
int main(void) { bump(); bump(); printf("counter=%d hidden=%d\n", counter, hidden); return 0; }
