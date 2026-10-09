#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef union { float f; uint32_t u; } FloatBits;

/* A tagged union (sum type): the tag says which member is valid. */
typedef struct {
    enum { T_INT, T_DOUBLE, T_STR } tag;
    union { long i; double d; const char *s; } as;
} Value;

static void print(const Value *v) {
    switch (v->tag) {
    case T_INT:    printf("int %ld\n", v->as.i); break;
    case T_DOUBLE: printf("double %g\n", v->as.d); break;
    case T_STR:    printf("string \"%s\"\n", v->as.s); break;
    }
}

struct flags {                       /* bit-fields: layout is implementation-defined */
    unsigned ready : 1;
    unsigned mode  : 3;
    unsigned count : 12;
};

int main(void) {
    FloatBits fb = { .f = 1.0f };
    printf("1.0f as bits via union: 0x%08x (type punning through a union is defined in C, not C++)\n", fb.u);
    uint32_t viaMemcpy; float one = 1.0f; memcpy(&viaMemcpy, &one, sizeof one);
    printf("same via memcpy:        0x%08x\n", viaMemcpy);

    Value vs[] = { { T_INT, { .i = 42 } }, { T_DOUBLE, { .d = 2.5 } }, { T_STR, { .s = "text" } } };
    for (size_t i = 0; i < 3; i++) print(&vs[i]);
    printf("sizeof(Value) = %zu (tag + largest member + padding)\n", sizeof(Value));

    struct flags f = { .ready = 1, .mode = 5, .count = 4000 };
    uint32_t raw = 0; memcpy(&raw, &f, sizeof f);
    printf("sizeof(struct flags)=%zu, raw bytes as integer = 0x%08x  (bit order is compiler/ABI specific)\n", sizeof f, raw);
    return 0;
}
