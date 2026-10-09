#include <stdio.h>
#include <string.h>

/* Deliberately unsafe: copies without checking the length. */
static void copy(const char *s) {
    char buf[16];
    strcpy(buf, s);
    printf("copied: %s\n", buf);
}

int main(int argc, char **argv) {
    copy(argc > 1 ? argv[1] : "short");
    puts("returned normally");
    return 0;
}
