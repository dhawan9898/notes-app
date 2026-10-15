---
kind: question
title: Why doubling?
module: Linear Structures
difficulty: Medium
---

A dynamic array starts with capacity 1. Count how many element **copies** `N` pushes cost if it grows (a) by doubling and (b) by a fixed +10. Predict the ratio for `N = 1,000,000` before running anything, then state the amortised cost per push for each strategy.

## Explanation

```c
#include <stdio.h>

/* Count how many element copies N pushes cost when the array grows by doubling versus by a fixed +10. */
static long copies_for(long pushes, int doubling) {
    long cap = 1, size = 0, copies = 0;
    for (long i = 0; i < pushes; i++) {
        if (size == cap) {                        /* full: allocate a bigger array and copy everything over */
            copies += size;
            cap = doubling ? cap * 2 : cap + 10;
        }
        size++;
    }
    return copies;
}

int main(void) {
    printf("%10s %16s %16s\n", "pushes", "copies (double)", "copies (+10)");
    for (long n = 1000; n <= 1000000; n *= 10)
        printf("%10ld %16ld %16ld\n", n, copies_for(n, 1), copies_for(n, 0));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p2_growth p2_growth.c && ./p2_growth; rm -f p2_growth
    pushes  copies (double)     copies (+10)
      1000             1023            49600
     10000            16383          4996000
    100000           131071        499960000
   1000000          1048575      49999600000
```

Doubling copies about **N** elements in total (1,048,575 for a million pushes: 1 + 2 + 4 + ... + 2^19), so the amortised cost is about 1 copy per push: **O(1)**. Growing by a constant copies about N²/20 elements in total (50 billion for a million pushes), so each push costs O(N) amortised, roughly 48,000 times worse at this size. The deeper reason is that doubling makes the *copies a geometric series* whose sum is bounded by twice the last term; a fixed increment makes it an arithmetic series that grows quadratically. Real libraries use factors between 1.5 and 2.
