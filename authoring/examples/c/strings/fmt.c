#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    char secret[] = "TOPSECRET";
    (void)secret;
    if (argc > 1) printf(argv[1]);        /* BUG: the user controls the format string */
    putchar('\n');
    return 0;
}
