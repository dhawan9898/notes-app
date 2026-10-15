// Strict aliasing in action: the optimizer assumes an int * and a float * never refer to the same object.
#include <stdio.h>
#include <string.h>

__attribute__((noinline)) int store_and_read(int *ip, float *fp) {
    *ip = 1;
    *fp = 2.0f;          // may this store change *ip? Under C11 6.5p7 it may not, so ...
    return *ip;          // ... the compiler is allowed to return the constant 1 without reloading
}

// Defined alternative: reinterpret bytes with memcpy (compilers turn it into a register move).
static unsigned bits_of(float f) {
    unsigned u;
    memcpy(&u, &f, sizeof u);
    return u;
}

int main(void) {
    union { int i; float f; } u;
    int r = store_and_read(&u.i, &u.f);    // both pointers refer to the same 4 bytes: undefined behavior
    printf("store_and_read returned %d (0x%08x)\n", r, (unsigned)r);
    printf("memory holds            0x%08x (bit pattern of 2.0f)\n", bits_of(u.f));
    return 0;
}
