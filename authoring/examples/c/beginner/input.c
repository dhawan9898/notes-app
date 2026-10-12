#include <stdio.h>
#include <stdlib.h>

/* Read a line safely with fgets (it never writes past the buffer), then convert it. Never use gets(); avoid scanf("%s"). */
int main(void) {
    char line[64];
    printf("Enter a whole number: ");
    if (fgets(line, sizeof line, stdin) == NULL) { printf("no input\n"); return 1; }

    char *end;
    long value = strtol(line, &end, 10);          /* end points at the first character that was NOT part of the number */
    if (end == line) { printf("that was not a number: %s", line); return 1; }
    printf("you typed %ld; doubled it is %ld\n", value, value * 2);
    return 0;
}
