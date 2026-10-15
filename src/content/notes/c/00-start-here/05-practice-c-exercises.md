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

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Number systems (paper).** Convert `1011 0110` (binary) to decimal and to hexadecimal. Convert decimal `200` to binary and hexadecimal.

**2. Predict the output (paper).** What does each expression evaluate to? `5 / 2`, `5 % 2`, `5 / 2.0`, `10 - 3 * 2`, `7 & 3`, `1 << 4`.

**3. Fix the swap.** This function does not change the caller's variables. Explain why, and rewrite it so that it works.

```c
void swap_ints(int a, int b) { int t = a; a = b; b = t; }
```

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: make swap_ints exchange the caller's two variables.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* The tests call swap_ints(&a, &b). This version makes the same mistake as the by-value one in the exercise:
   it swaps its own copies (here, copies of the two pointers) and leaves the caller's ints alone. */
void swap_ints(int *a, int *b) {
    int *t = a;
    a = b;
    b = t;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a = 1, b = 2;
    swap_ints(&a, &b);
    check_int("a after swap_ints(&a, &b)", a, 2);
    check_int("b after swap_ints(&a, &b)", b, 1);
    int x = -7, y = 7;
    swap_ints(&x, &y);
    check_int("x after swap_ints(&x, &y)", x, 7);
    check_int("y after swap_ints(&x, &y)", y, -7);
    return summary();
}
```

</details>

**4. Maximum.** Write `int max_of(const int *v, int count)` returning the largest element of an array (assume `count > 0`).

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 4: write max_of, the largest element of an array (count > 0).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int max_of(const int *v, int count) {
    (void)v; (void)count;
    return 0;   /* replace with your loop */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a[] = {4, 9, -2, 7};
    int negatives[] = {-5, -1, -7};
    int one[] = {42};
    int last[] = {1, 2, 3, 100};
    check_int("max_of({4, 9, -2, 7})", max_of(a, 4), 9);
    check_int("max_of({-5, -1, -7}) (all negative)", max_of(negatives, 3), -1);
    check_int("max_of({42})", max_of(one, 1), 42);
    check_int("max_of({1, 2, 3, 100}) (largest is last)", max_of(last, 4), 100);
    return summary();
}
```

</details>

**5. Reverse a string in place.** Write `void reverse(char *s)`; `"stressed"` becomes `"desserts"`. (Hint: a string ends at `'\0'`; use `strlen` and swap pairs from both ends.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 5: reverse a string in place.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
void reverse(char *s) {
    (void)s;    /* swap pairs of characters from both ends */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_str(const char *what, const char *got, const char *want) {
    checks++;
    if (got && strcmp(got, want) == 0) printf("PASS %s = \"%s\"\n", what, got);
    else { failed++; printf("FAIL %s: expected \"%s\", got %s%s%s\n", what, want, got ? "\"" : "", got ? got : "NULL", got ? "\"" : ""); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    char s1[] = "stressed", s2[] = "ab", s3[] = "a", s4[] = "", s5[] = "racecar", s6[] = "hello world";
    reverse(s1); check_str("reverse(\"stressed\")", s1, "desserts");
    reverse(s2); check_str("reverse(\"ab\")", s2, "ba");
    reverse(s3); check_str("reverse(\"a\")", s3, "a");
    reverse(s4); check_str("reverse(\"\")", s4, "");
    reverse(s5); check_str("reverse(\"racecar\")", s5, "racecar");
    reverse(s6); check_str("reverse(\"hello world\")", s6, "dlrow olleh");
    return summary();
}
```

</details>

**6. Count the one-bits.** Write `int popcount(uint32_t x)` returning how many bits of `x` are 1. `popcount(255)` is 8.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 6: count the 1 bits of a 32-bit number.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int popcount(uint32_t x) {
    (void)x;
    return 0;   /* look at the lowest bit with x & 1, then shift */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("popcount(0)", popcount(0), 0);
    check_int("popcount(1)", popcount(1), 1);
    check_int("popcount(255)", popcount(255), 8);
    check_int("popcount(0x80000001)", popcount(0x80000001u), 2);
    check_int("popcount(0xFFFFFFFF)", popcount(0xFFFFFFFFu), 32);
    return summary();
}
```

</details>

**7. A growable array.** Using `malloc`/`realloc`/`free`, write a small structure holding an `int` array that doubles its capacity when full, with a `vec_push` function. Push ten squares and print the size and capacity. Check that you free the memory.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 7: a growable array of ints that doubles its capacity when full.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int *data;
    size_t size;    /* elements in use */
    size_t cap;     /* elements allocated */
} IntVec;

/* ===== Your code: edit below this line ===== */
/* Append x; grow data with realloc (doubling cap) when size == cap. Return 1 on success, 0 if memory runs out. */
int vec_push(IntVec *v, int x) {
    (void)v; (void)x;
    return 0;
}

/* Release the memory and reset the vector to empty. */
void vec_free(IntVec *v) {
    (void)v;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    IntVec v = {0};
    int ok = 1;
    for (int i = 0; i < 10; i++) ok &= vec_push(&v, i * i);
    check_int("every vec_push returned 1", ok, 1);
    check_int("size after 10 pushes", (long long)v.size, 10);
    check_int("capacity is at least the size", v.cap >= v.size && v.data != NULL, 1);
    if (v.data && v.size == 10) {
        check_int("data[0]", v.data[0], 0);
        check_int("data[9]", v.data[9], 81);
    }
    vec_free(&v);
    check_int("data is NULL after vec_free", v.data == NULL, 1);
    check_int("size is 0 after vec_free", (long long)v.size, 0);
    return summary();
}
```

</details>

**8. Find the bug.** This function should sum the first `n` elements. It sometimes returns a wrong value or crashes. Find the bug, then run it under AddressSanitizer and read the report.

```c
int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) total += v[i];
    return total;
}
```

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 8: find and fix the bug in sum_first_n.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) total += v[i];
    return total;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int v[] = {1, 2, 3, 4, 99};     /* the 99 is not part of the first 4 elements */
    check_int("sum_first_n(v, 4)", sum_first_n(v, 4), 10);
    check_int("sum_first_n(v, 1)", sum_first_n(v, 1), 1);
    check_int("sum_first_n(v, 0)", sum_first_n(v, 0), 0);
    check_int("sum_first_n(v, 5)", sum_first_n(v, 5), 109);
    return summary();
}
```

</details>

**9. Subnetting with a mask.** Given the address `10.20.77.130` and prefix length 21, compute the network address by building the mask and using `&`. (Check against the "do it in your head" method in *IPv4 Addressing, Subnetting, VLSM and CIDR*.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 9: network address of an IPv4 address and prefix length, using a mask and &.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IP(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

/* ===== Your code: edit below this line ===== */
/* Return ip with the host bits (the last 32 - prefix bits) cleared. prefix is 0 to 32. */
uint32_t network_of(uint32_t ip, int prefix) {
    (void)prefix;
    return ip;      /* build the mask, then return ip & mask */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("10.20.77.130/21 -> 10.20.72.0", network_of(IP(10, 20, 77, 130), 21), IP(10, 20, 72, 0));
    check_int("192.168.1.77/24 -> 192.168.1.0", network_of(IP(192, 168, 1, 77), 24), IP(192, 168, 1, 0));
    check_int("172.16.5.4/12 -> 172.16.0.0", network_of(IP(172, 16, 5, 4), 12), IP(172, 16, 0, 0));
    check_int("10.1.2.3/32 -> 10.1.2.3", network_of(IP(10, 1, 2, 3), 32), IP(10, 1, 2, 3));
    check_int("10.1.2.3/0 -> 0.0.0.0", network_of(IP(10, 1, 2, 3), 0), 0);
    return summary();
}
```

</details>

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
