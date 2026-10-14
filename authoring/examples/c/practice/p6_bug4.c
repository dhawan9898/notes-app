#include <stdio.h>
#include <stdlib.h>

static void process(int fail) {
    char *buf = malloc(64);
    if (!buf) return;
    if (fail) {
        free(buf);
        free(buf);
        return;
    }
    free(buf);
}

int main(void) { process(0); process(1); return 0; }
