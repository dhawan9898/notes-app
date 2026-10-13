// Section garbage collection: unused functions and data disappear when each lives in its own section.
#include <stdio.h>
int used(int x) { return x * 3 + 1; }
int unused_a(int x) { int s = 0; for (int i = 0; i < x; i++) s += i * i; return s; }
int unused_b(int x) { return unused_a(x) ^ 0x5a5a; }
const char table_unused[2048] = {1};
int main(void) { printf("%d\n", used(14)); return 0; }
