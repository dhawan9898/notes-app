#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf on_error;

static void parse_level(int depth) {
    if (depth == 3) longjmp(on_error, 42);          /* unwinds 3 frames at once; NO destructors/cleanup run */
    if (depth < 10) parse_level(depth + 1);
}

int main(void) {
    int plain_counter = 0;
    volatile int safe_counter = 0;                  /* locals modified after setjmp and read after longjmp must be volatile */
    char *buf = NULL;

    int code = setjmp(on_error);                    /* returns 0 first, then the longjmp value */
    if (code == 0) {
        buf = malloc(100);                          /* will leak: longjmp skips free */
        plain_counter = 1;
        safe_counter = 1;
        printf("first pass: calling parse_level\n");
        parse_level(0);
        puts("not reached");
    } else {
        printf("recovered with code %d, safe_counter=%d (plain_counter=%d is indeterminate in theory)\n", code, safe_counter, plain_counter);
        free(buf);                                  /* we kept the pointer, so we can clean up here */
    }
    return 0;
}
