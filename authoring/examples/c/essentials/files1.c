#include <stdio.h>

int main(void) {
    FILE *out = fopen("scores.txt", "w");         /* "w": create or overwrite for writing */
    if (!out) { perror("scores.txt"); return 1; } /* ALWAYS check: the file may be unwritable */
    fprintf(out, "ada 91\nlinus 78\ngrace 99\n");
    fclose(out);                                  /* flushes buffered data to disk */

    FILE *in = fopen("scores.txt", "r");
    if (!in) { perror("scores.txt"); return 1; }
    char name[32];
    int score, count = 0, sum = 0;
    while (fscanf(in, "%31s %d", name, &score) == 2) {   /* returns how many items it read */
        printf("%-6s %3d\n", name, score);
        sum += score; count++;
    }
    fclose(in);
    printf("%d scores, average %.1f\n", count, (double)sum / count);

    FILE *missing = fopen("no-such-file.txt", "r");
    fflush(stdout);                               /* keep stdout and stderr lines in order */
    if (!missing) perror("no-such-file.txt");     /* prints the reason, e.g. "No such file or directory" */
    return 0;
}
