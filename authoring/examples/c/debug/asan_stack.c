#include <stdio.h>
#include <string.h>

int global_table[4] = {1, 2, 3, 4};

static int sum_table(int n) {           /* off-by-one on a global: reads global_table[4] */
    int s = 0;
    for (int i = 0; i <= n; i++) s += global_table[i];
    return s;
}

static void fill(const char *src) {
    char name[8];
    char tag[8] = "TAG";
    strcpy(name, src);                  /* stack buffer overflow when src is longer than 7 chars */
    printf("name=%s tag=%s\n", name, tag);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0] == 'g') printf("sum=%d\n", sum_table(4));
    else fill(argc > 1 ? argv[1] : "ok");
    return 0;
}
