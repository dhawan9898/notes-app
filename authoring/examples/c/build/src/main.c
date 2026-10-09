#include <stdio.h>
#include <string.h>
#include "util.h"
int main(int argc, char **argv) {
    char buf[32];
    if (argc > 1) { strncpy(buf, argv[1], sizeof buf - 1); buf[sizeof buf - 1] = 0; } else strcpy(buf, "default");
    printf("%s -> %d\n", buf, clamp(argc * 50, 0, 100));
    return 0;
}
