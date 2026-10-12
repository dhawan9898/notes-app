#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exercise 3: swap that works */
static void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }

/* Exercise 4: largest element of an array (count > 0) */
static int max_of(const int *v, int count) { int best = v[0]; for (int i = 1; i < count; i++) if (v[i] > best) best = v[i]; return best; }

/* Exercise 5: reverse a string in place */
static void reverse(char *s) { size_t n = strlen(s); for (size_t i = 0; i < n / 2; i++) { char t = s[i]; s[i] = s[n - 1 - i]; s[n - 1 - i] = t; } }

/* Exercise 6: count the 1 bits of a number */
static int popcount(uint32_t x) { int c = 0; while (x) { c += x & 1; x >>= 1; } return c; }

/* Exercise 7: a growable array of ints built on malloc/realloc */
typedef struct { int *data; size_t size, cap; } IntVec;
static int vec_push(IntVec *v, int x) {
    if (v->size == v->cap) { size_t ncap = v->cap ? v->cap * 2 : 4; int *nd = realloc(v->data, ncap * sizeof *nd); if (!nd) return 0; v->data = nd; v->cap = ncap; }
    v->data[v->size++] = x; return 1;
}

/* Exercise 8: the fixed version of an off-by-one loop */
static int sum_first_n(const int *v, int n) { int total = 0; for (int i = 0; i < n; i++) total += v[i]; return total; }   /* the buggy version used i <= n */

/* Exercise 9: network address of ip/prefix, as four numbers */
static void network_of(uint32_t ip, int prefix, unsigned out[4]) {
    uint32_t mask = prefix == 0 ? 0 : 0xFFFFFFFFu << (32 - prefix), net = ip & mask;
    for (int i = 0; i < 4; i++) out[i] = (net >> (24 - 8 * i)) & 255;
}

int main(void) {
    printf("Ex 1: 0b1011 0110 = %d = 0x%X;  200 = 0b", 0xB6, 0xB6); for (int i = 7; i >= 0; i--) putchar(200 >> i & 1 ? '1' : '0'); printf(" = 0x%X\n", 200);
    printf("Ex 2: 5/2 = %d, 5%%2 = %d, 5/2.0 = %.1f, 10 - 3 * 2 = %d, 7 & 3 = %d, 1 << 4 = %d\n", 5 / 2, 5 % 2, 5 / 2.0, 10 - 3 * 2, 7 & 3, 1 << 4);
    int a = 1, b = 2; swap_ints(&a, &b); assert(a == 2 && b == 1); printf("Ex 3: after swap_ints: a=%d b=%d\n", a, b);
    int v[] = { 4, 9, -2, 7 }; assert(max_of(v, 4) == 9); printf("Ex 4: max_of = %d\n", max_of(v, 4));
    char s[] = "stressed"; reverse(s); printf("Ex 5: reverse(\"stressed\") = %s\n", s);
    assert(popcount(0) == 0 && popcount(255) == 8 && popcount(0x80000001u) == 2); printf("Ex 6: popcount(255) = %d, popcount(0x80000001) = %d\n", popcount(255), popcount(0x80000001u));
    IntVec vec = { 0 }; for (int i = 0; i < 10; i++) vec_push(&vec, i * i); printf("Ex 7: pushed 10 squares, size=%zu capacity=%zu last=%d\n", vec.size, vec.cap, vec.data[9]); free(vec.data);
    int w[] = { 1, 2, 3, 4 }; printf("Ex 8: sum_first_n = %d\n", sum_first_n(w, 4));
    unsigned net[4]; network_of((10u << 24) | (20u << 16) | (77u << 8) | 130u, 21, net); printf("Ex 9: 10.20.77.130/21 -> network %u.%u.%u.%u\n", net[0], net[1], net[2], net[3]);
    printf("Ex 10: -1 as uint8_t = %u, -128 as uint8_t = %u, (int8_t)200 = %d, (uint8_t)(255 + 1) = %u\n", (uint8_t)-1, (uint8_t)-128, (int8_t)200, (uint8_t)(255 + 1));
    return 0;
}
