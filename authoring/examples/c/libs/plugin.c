#include <stdio.h>
int plugin_run(int x) { printf("plugin: got %d\n", x); return x * 2; }
