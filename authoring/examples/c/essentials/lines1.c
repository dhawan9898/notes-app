#include <stdio.h>
#include <string.h>

int main(void) {
    char line[64];
    printf("Type a sentence: ");
    if (!fgets(line, sizeof line, stdin)) return 1;   /* reads at most 63 chars, keeps the newline */
    line[strcspn(line, "\n")] = '\0';                  /* cut the newline off */
    int words = 0, in_word = 0;
    for (const char *p = line; *p; p++) {
        if (*p == ' ') in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    printf("\"%s\" has %zu characters and %d words\n", line, strlen(line), words);
    return 0;
}
