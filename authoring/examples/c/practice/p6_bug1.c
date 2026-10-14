#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *name = malloc(5);
    strcpy(name, "Alice");
    printf("%s\n", name);
    free(name);
    return 0;
}
