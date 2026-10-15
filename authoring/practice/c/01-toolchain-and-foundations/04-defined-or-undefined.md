---
kind: question
title: Defined or undefined?
module: Toolchain and Language Foundations
difficulty: Medium
---

For each, say whether the behaviour is defined, implementation-defined or undefined, and why: (a) `unsigned u = UINT_MAX; u++;` (b) `int i = INT_MAX; i++;` (c) `int n = 1 << 31;` (d) `int a[4]; a[4] = 0;` (e) `(int)3.99` (f) `char c = 300;` Run (b) under UBSan and read the report.

## Explanation

- (a) **Defined**: unsigned arithmetic wraps modulo 2^N, so `u` becomes 0.
- (b) **Undefined**: signed overflow. The compiler may assume it never happens. Run it:

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int big = INT_MAX;
    int wrapped = (int)((unsigned)big + 1u);            /* defined: do the arithmetic unsigned, convert back (implementation-defined) */
    printf("computed through unsigned: %d\n", wrapped);
    int big2 = INT_MAX;
    printf("about to overflow a signed int...\n");
    fflush(stdout);
    big2++;                                              /* undefined behaviour: UBSan reports it */
    printf("%d\n", big2);
    return 0;
}
```

```text
$ gcc -std=c17 -g -fsanitize=undefined -o ub p1_ub.c && ./ub 2>&1 | sed 's#/[^ ]*/##'; rm -f ub
computed through unsigned: -2147483648
about to overflow a signed int...
p1_ub.c:11:9: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
-2147483648
```

UBSan names the exact line and operands. The safe route is to do the arithmetic in `unsigned` and convert back (value implementation-defined) or to use the checked helpers from exercise 3.
- (c) **Undefined** in C17 for a 32-bit `int`: shifting a 1 into the sign bit overflows. Use `1u << 31`.
- (d) **Undefined**: writing one past the end of an array (AddressSanitizer reports a stack-buffer-overflow).
- (e) **Defined**: conversion from floating point truncates toward zero, giving `3`, provided the value fits.
- (f) **Implementation-defined**: 300 does not fit a `char`; GCC wraps to 44. It compiles silently, but `-Woverflow` warns.
