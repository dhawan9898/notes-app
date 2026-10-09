#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>

struct bad  { char a; double b; char c; int d; short e; };          /* declaration order as written */
struct good { double b; int d; short e; char a; char c; };          /* members sorted by decreasing alignment */
struct __attribute__((packed)) packed { char a; double b; char c; int d; short e; };

struct hdr {                       /* a wire-format-like header: avoid relying on struct layout for that! */
    uint8_t  version;
    uint16_t length;
    uint32_t id;
};

struct flex { size_t n; int items[]; };                              /* flexible array member */

#define SHOW(T, ...) do { printf("%-14s size=%2zu align=%zu\n", #T, sizeof(struct T), alignof(struct T)); __VA_ARGS__ } while (0)
#define OFF(T, m) printf("    %-4s offset=%2zu size=%zu\n", #m, offsetof(struct T, m), sizeof(((struct T *)0)->m))

int main(void) {
    SHOW(bad,    OFF(bad, a); OFF(bad, b); OFF(bad, c); OFF(bad, d); OFF(bad, e););
    SHOW(good,   OFF(good, b); OFF(good, d); OFF(good, e); OFF(good, a); OFF(good, c););
    SHOW(packed, OFF(packed, a); OFF(packed, b); OFF(packed, c); OFF(packed, d); OFF(packed, e););
    SHOW(hdr,    OFF(hdr, version); OFF(hdr, length); OFF(hdr, id););
    printf("flex: sizeof(struct flex)=%zu (items[] adds 0), allocate n items with sizeof + n*sizeof(int)\n", sizeof(struct flex));
    return 0;
}
