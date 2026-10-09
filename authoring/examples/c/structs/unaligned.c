#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned char buf[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint32_t v;
    memcpy(&v, buf + 1, sizeof v);             /* defined: compiles to a single (unaligned-capable) load on x86 */
    printf("memcpy read: 0x%08x\n", v);
    v = *(uint32_t *)(buf + 1);                /* UB: misaligned + aliasing; may fault on ARM/SPARC or be miscompiled */
    printf("cast read:   0x%08x\n", v);
    return 0;
}
