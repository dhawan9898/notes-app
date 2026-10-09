#include <stdio.h>
#include "greet.h"

int call_count = 0;              /* initialized data? no: zero -> .bss */
static int secret = 42;          /* internal linkage: local symbol */
const char *banner = GREETING;   /* pointer in .data, string in .rodata */

int add(int a, int b) {
    call_count++;
    return a + b + secret;
}

int main(void) {
    printf("%s %d\n", banner, add(1, 2));
    return 0;
}
