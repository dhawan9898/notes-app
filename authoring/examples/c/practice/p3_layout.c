#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct wasteful { char tag; double value; char flag; int count; short id; };      /* declared in a careless order */
struct compact  { double value; int count; short id; char tag; char flag; };      /* same members, largest first */
struct packed_hdr { uint8_t version; uint16_t length; uint32_t id; } __attribute__((packed));

#define SHOW(type, member) printf("  %-8s offset %2zu\n", #member, offsetof(type, member))

int main(void) {
    printf("struct wasteful: size %zu, alignment %zu\n", sizeof(struct wasteful), _Alignof(struct wasteful));
    SHOW(struct wasteful, tag); SHOW(struct wasteful, value); SHOW(struct wasteful, flag); SHOW(struct wasteful, count); SHOW(struct wasteful, id);
    printf("struct compact:  size %zu, alignment %zu\n", sizeof(struct compact), _Alignof(struct compact));
    SHOW(struct compact, value); SHOW(struct compact, count); SHOW(struct compact, id); SHOW(struct compact, tag); SHOW(struct compact, flag);
    printf("packed header:   size %zu (no padding, but unaligned fields are slower and can fault on some CPUs)\n", sizeof(struct packed_hdr));

    union { uint32_t as_int; float as_float; unsigned char bytes[4]; } u = {.as_float = 1.0f};
    printf("float 1.0 viewed as bytes: %02x %02x %02x %02x -> integer 0x%08x\n", u.bytes[0], u.bytes[1], u.bytes[2], u.bytes[3], u.as_int);

    struct { unsigned ready : 1; unsigned mode : 3; unsigned level : 4; } bits = {1, 5, 9};
    printf("bit-fields ready=%u mode=%u level=%u occupy %zu byte(s)\n", bits.ready, bits.mode, bits.level, sizeof bits);
    return 0;
}
