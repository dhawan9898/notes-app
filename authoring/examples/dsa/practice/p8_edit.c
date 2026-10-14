#include <stdio.h>
#include <string.h>

static int min3(int a, int b, int c) { int m = a < b ? a : b; return m < c ? m : c; }

/* Edit distance: fewest insertions, deletions and substitutions turning a into b. d[i][j] = distance of the prefixes a[0..i), b[0..j). */
static int edit_distance(const char *a, const char *b) {
    int n = (int)strlen(a), m = (int)strlen(b);
    int d[32][32];
    for (int i = 0; i <= n; i++) d[i][0] = i;                    /* delete all of a's prefix */
    for (int j = 0; j <= m; j++) d[0][j] = j;                    /* insert all of b's prefix */
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            d[i][j] = min3(d[i - 1][j] + 1,                      /* delete a[i-1] */
                           d[i][j - 1] + 1,                      /* insert b[j-1] */
                           d[i - 1][j - 1] + (a[i - 1] != b[j - 1]));   /* substitute (cost 0 if equal) */
    return d[n][m];
}

int main(void) {
    const char *pairs[][2] = {{"kitten", "sitting"}, {"flaw", "lawn"}, {"", "abc"}, {"same", "same"}, {"intention", "execution"}};
    for (int i = 0; i < 5; i++) printf("edit_distance(\"%s\", \"%s\") = %d\n", pairs[i][0], pairs[i][1], edit_distance(pairs[i][0], pairs[i][1]));
    return 0;
}
