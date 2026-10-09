#include <limits.h>
#include <stdio.h>
#include <stdint.h>
int main(void) {
    printf("char is %s, CHAR_BIT=%d\n", (char)-1 < 0 ? "signed" : "unsigned", CHAR_BIT);
    printf("sizeof short=%zu int=%zu long=%zu long long=%zu void*=%zu size_t=%zu\n",
           sizeof(short), sizeof(int), sizeof(long), sizeof(long long), sizeof(void *), sizeof(size_t));
    printf("-8 >> 1 = %d (implementation-defined for negative values)\n", -8 >> 1);
    printf("INT_MIN / -1 is undefined; (unsigned)-1 = %u\n", (unsigned)-1);
    uint32_t x = 0x01020304; unsigned char *b = (unsigned char *)&x;
    printf("byte order: %s-endian\n", b[0] == 4 ? "little" : "big");
    return 0;
}
