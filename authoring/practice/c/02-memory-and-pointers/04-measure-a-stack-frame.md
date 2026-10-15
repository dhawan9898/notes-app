---
kind: question
title: Measure a stack frame
module: Memory and Pointers
difficulty: Medium
---

Write a recursive function with a 200-byte local array that records the address of that array at each depth, and use the difference between depth 0 and depth 4 to estimate the size of one frame. Which way does the stack grow?

## Explanation

```c
#include <stdio.h>
#include <stdint.h>

static uintptr_t first;

static void recurse(int depth) {
    volatile char pad[200];                       /* give each frame a known amount of local data */
    pad[0] = (char)depth;
    uintptr_t here = (uintptr_t)&pad[0];
    if (depth == 0) first = here;
    if (depth == 4) {
        printf("stack grew by %lu bytes over 4 calls (about %lu bytes per frame)\n",
               (unsigned long)(first - here), (unsigned long)((first - here) / 4));
        return;
    }
    recurse(depth + 1);
    pad[1] = pad[0];                              /* keep the call from becoming a loop */
}

int main(void) { recurse(0); return 0; }
```

```text
$ gcc -std=c17 -Wall -Wextra -O0 -o p2_frames p2_frames.c && ./p2_frames; rm -f p2_frames
stack grew by 960 bytes over 4 calls (about 240 bytes per frame)
```

Each frame is the 200-byte array plus the saved return address, saved frame pointer, the `depth` argument and alignment, so a little over 200 bytes (the exact figure depends on compiler and flags). The first address is **higher** than the later ones, so the stack grows toward **lower addresses**. (Do not compile this one with a sanitizer: AddressSanitizer moves locals off the real stack.)
