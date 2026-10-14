#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {               /* argc = how many words, argv = the words themselves */
    printf("program name: %s, arguments given: %d\n", argv[0], argc - 1);
    long total = 0;
    for (int i = 1; i < argc; i++) {
        char *end;
        long v = strtol(argv[i], &end, 10);       /* text -> number, and a way to detect junk */
        if (*end != '\0') { fprintf(stderr, "'%s' is not a whole number\n", argv[i]); return 1; }
        total += v;
    }
    printf("sum = %ld\n", total);
    return 0;
}
