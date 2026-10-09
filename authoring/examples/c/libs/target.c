#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    for (int i = 0; i < 5; i++) { char *p = malloc(100); strcpy(p, "x"); free(p); }
    puts("done");
    return 0;
}
