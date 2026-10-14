#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *scores = malloc(3 * sizeof *scores);
    scores[0] = 90; scores[1] = 80; scores[2] = 70;
    free(scores);
    printf("first score after free: %d\n", scores[0]);
    return 0;
}
