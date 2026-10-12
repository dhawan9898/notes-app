---
title: "Practice: C Exercises"
track: c
module: Start Here
moduleOrder: 0
order: 5
level: beginner
summary: "Ten graded exercises on number systems, expressions, pointers, strings, bits, dynamic memory and reading a sanitizer report, with a verified answer program at the end."
prerequisites:
  - "C From Zero: Your First Programs"
  - Binary, Hexadecimal and Bits
  - Pointers for Beginners
sources:
  - label: "King — C Programming, A Modern Approach, 2nd ed., programming projects of Ch. 5-13"
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., exercises of Ch. 1-5"
---

Do these in order. For each: try it on paper or in a file first, then compile with `gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined`, and only then look at the answers at the end. If you get stuck for more than ten minutes, read the matching section of the previous notes again. A real engineer's habit: *predict before you run*.

## Exercises

**1. Number systems (paper).** Convert `1011 0110` (binary) to decimal and to hexadecimal. Convert decimal `200` to binary and hexadecimal.

**2. Predict the output (paper).** What does each expression evaluate to? `5 / 2`, `5 % 2`, `5 / 2.0`, `10 - 3 * 2`, `7 & 3`, `1 << 4`.

**3. Fix the swap.** This function does not change the caller's variables. Explain why, and rewrite it so that it works.

```c
void swap_ints(int a, int b) { int t = a; a = b; b = t; }
```

**4. Maximum.** Write `int max_of(const int *v, int count)` returning the largest element of an array (assume `count > 0`).

**5. Reverse a string in place.** Write `void reverse(char *s)`; `"stressed"` becomes `"desserts"`. (Hint: a string ends at `'\0'`; use `strlen` and swap pairs from both ends.)

**6. Count the one-bits.** Write `int popcount(uint32_t x)` returning how many bits of `x` are 1. `popcount(255)` is 8.

**7. A growable array.** Using `malloc`/`realloc`/`free`, write a small structure holding an `int` array that doubles its capacity when full, with a `vec_push` function. Push ten squares and print the size and capacity. Check that you free the memory.

**8. Find the bug.** This function should sum the first `n` elements. It sometimes returns a wrong value or crashes. Find the bug, then run it under AddressSanitizer and read the report.

```c
int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) total += v[i];
    return total;
}
```

**9. Subnetting with a mask.** Given the address `10.20.77.130` and prefix length 21, compute the network address by building the mask and using `&`. (Check against the "do it in your head" method in *IPv4 Addressing, Subnetting, VLSM and CIDR*.)

**10. Wrap-around.** What are `(uint8_t)-1`, `(uint8_t)-128`, `(int8_t)200` and `(uint8_t)(255 + 1)`? Explain each in terms of two's complement.

## Answers

Try before opening.

<details>
<summary>Show answers and a verified program</summary>

1. `1011 0110` = 128 + 32 + 16 + 4 + 2 = **182** = **0xB6** (split into nibbles `1011` = B, `0110` = 6). `200` = 128 + 64 + 8 = `1100 1000` = **0xC8**.
2. `5 / 2` = **2** (integer division); `5 % 2` = **1**; `5 / 2.0` = **2.5**; `10 - 3 * 2` = **4** (multiplication first); `7 & 3` = `0111 & 0011` = **3**; `1 << 4` = **16**.
3. The function receives *copies* of the two numbers; it swaps its copies and the caller's variables are untouched. Pass addresses: `void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }` and call `swap_ints(&x, &y)`.
4. Start with the first element as the best, scan the rest and replace the best when you see a larger one.
5. Swap `s[i]` with `s[n-1-i]` for `i` from 0 up to (not including) `n/2`. Stopping at the middle matters: going to `n` would swap everything back.
6. Test the lowest bit with `x & 1`, add it to a counter, shift right, repeat until `x` is 0 (there are faster methods: see *Bit Manipulation*).
7. Keep `data`, `size` and `cap`; when `size == cap`, `realloc` to double (starting from 4) and **check the result for `NULL` before overwriting your pointer**; doubling makes the average cost of a push constant (see *Dynamic Arrays* in the DSA track).
8. The loop condition `i <= n` runs `n + 1` times and reads one element past the end (`v[n]`): an **off-by-one** error. With `i < n` it is correct. AddressSanitizer reports `stack-buffer-overflow ... READ of size 4` and points at the line.
9. Mask for /21 is `0xFFFFFFFF << (32 - 21)`; `ip & mask` gives `10.20.72.0` (the third octet 77 rounds down to a multiple of 8).
10. `-1` in 8 bits is `1111 1111` = **255**; `-128` is `1000 0000` = **128**; `200` does not fit in a signed 8-bit type, so the conversion is *implementation-defined* (GCC keeps the bit pattern `1100 1000`, which as signed is **-56**); `255 + 1` is computed as an `int` (256) and converting to 8 bits keeps the low 8 bits, giving **0**.

The program below implements and checks all of the above (run under the sanitizers, so any memory mistake would be reported):

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o answers answers.c && ./answers
Ex 1: 0b1011 0110 = 182 = 0xB6;  200 = 0b11001000 = 0xC8
Ex 2: 5/2 = 2, 5%2 = 1, 5/2.0 = 2.5, 10 - 3 * 2 = 4, 7 & 3 = 3, 1 << 4 = 16
Ex 3: after swap_ints: a=2 b=1
Ex 4: max_of = 9
Ex 5: reverse("stressed") = desserts
Ex 6: popcount(255) = 8, popcount(0x80000001) = 2
Ex 7: pushed 10 squares, size=10 capacity=16 last=81
Ex 8: sum_first_n = 10
Ex 9: 10.20.77.130/21 -> network 10.20.72.0
Ex 10: -1 as uint8_t = 255, -128 as uint8_t = 128, (int8_t)200 = -56, (uint8_t)(255 + 1) = 0
```

The buggy exercise 8 under AddressSanitizer:

```c
#include <stdio.h>

int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++)       /* bug: runs n+1 times */
        total += v[i];
    return total;
}

int main(void) {
    int w[] = { 1, 2, 3, 4 };
    printf("%d\n", sum_first_n(w, 4));
    return 0;
}
```

```text
$ gcc -std=c17 -g -fsanitize=address -o ex8 ex8_bug.c && (./ex8 2>&1 | grep -E "ERROR|READ of|#1 " | sed 's#/[^ ]*/##g; s/==[0-9]*==//; s/ on address.*//; s/ at 0x[0-9a-f]* thread T0//; s/0x[0-9a-f]* in //')
ERROR: AddressSanitizer: stack-buffer-overflow
READ of size 4
    #1 main ex8_bug.c:12
```

</details>

## Key takeaways

1. Predict, compile with warnings and sanitizers, then compare.
2. Most beginner bugs here are: copies instead of addresses, off-by-one loops, forgetting the string terminator, and unchecked allocation.
3. The sanitizer report names the bug, the size of the access and the line.

**Next:** continue with module 1, *The Compilation Pipeline and the ELF Object Model*.
