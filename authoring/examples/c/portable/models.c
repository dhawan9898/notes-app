#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

int main(void) {
    printf("%-12s %s\n", "type", "bytes on this platform  (ILP32 / LP64 Linux / LLP64 Windows)");
    printf("%-12s %zu   (4 / 4 / 4)\n", "int", sizeof(int));
    printf("%-12s %zu   (4 / 8 / 4)   <- differs\n", "long", sizeof(long));
    printf("%-12s %zu   (8 / 8 / 8)\n", "long long", sizeof(long long));
    printf("%-12s %zu   (4 / 8 / 8)   <- differs\n", "void *", sizeof(void *));
    printf("%-12s %zu   (4 / 8 / 8)   <- differs\n", "size_t", sizeof(size_t));
    printf("%-12s %zu   (4 / 4 / 2)   <- differs\n", "wchar_t", sizeof(wchar_t));
    printf("%-12s %zu   (4 / 8 / 8)\n", "time_t", sizeof(time_t));
    printf("char signed? %s    byte order: %s-endian\n", CHAR_MIN < 0 ? "yes" : "no", *(const uint16_t *)"\x01\x00" == 1 ? "little" : "big");
    return 0;
}
