#include <stdio.h>
#include <stdlib.h>

int initialised_global = 42;           /* .data: has a starting value */
int zeroed_global;                     /* .bss: zero at start, takes no space in the file */
const char message[] = "read-only";    /* .rodata: constants */

int main(void) {
    int on_stack = 1;
    int *on_heap = malloc(sizeof *on_heap);
    if (!on_heap) return 1;

    printf("code   (main)             %p\n", (void *)main);
    printf("rodata (message)          %p\n", (const void *)message);
    printf("data   (initialised)      %p\n", (void *)&initialised_global);
    printf("bss    (zeroed)           %p\n", (void *)&zeroed_global);
    printf("heap   (malloc)           %p\n", (void *)on_heap);
    printf("stack  (local variable)   %p\n", (void *)&on_stack);
    free(on_heap);
    return 0;
}
