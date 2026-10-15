---
kind: question
title: Macro traps
module: Advanced Language
difficulty: Hard
---

Given `#define SQUARE_BAD(x) x * x`, what is `SQUARE_BAD(1 + 2)`? Fix the macro. Then, for `#define MAX_BAD(a, b) ((a) > (b) ? (a) : (b))`, what happens with `MAX_BAD(next(), 0)` where `next()` increments a global counter and returns it? Replace the macro with something that evaluates each argument once.

## Explanation

```c
#include <stdio.h>

#define SQUARE_BAD(x) x * x
#define SQUARE_OK(x) ((x) * (x))
#define MAX_BAD(a, b) ((a) > (b) ? (a) : (b))          /* parenthesised, but evaluates an argument twice */

static inline int max_int(int a, int b) { return a > b ? a : b; }   /* a function evaluates each argument once */

static int calls;
static int next(void) { return ++calls; }

int main(void) {
    printf("SQUARE_BAD(1 + 2) = %d (expands to 1 + 2 * 1 + 2)\n", SQUARE_BAD(1 + 2));
    printf("SQUARE_OK(1 + 2)  = %d\n", SQUARE_OK(1 + 2));
    int m = MAX_BAD(next(), 0);
    printf("MAX_BAD(next(), 0) = %d but next() ran %d times\n", m, calls);
    calls = 0;
    m = max_int(next(), 0);
    printf("max_int(next(), 0) = %d and next() ran %d time\n", m, calls);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_macros p4_macros.c && ./p4_macros; rm -f p4_macros
SQUARE_BAD(1 + 2) = 5 (expands to 1 + 2 * 1 + 2)
SQUARE_OK(1 + 2)  = 9
MAX_BAD(next(), 0) = 2 but next() ran 2 times
max_int(next(), 0) = 1 and next() ran 1 time
```

The preprocessor substitutes **text**: `SQUARE_BAD(1 + 2)` becomes `1 + 2 * 1 + 2` = 5, not 9. Parenthesise every parameter *and* the whole body: `((x) * (x))`. That fixes precedence but not **double evaluation**: `MAX_BAD(next(), 0)` expands `a` twice, so `next()` runs twice (the counter reached 2, and the printed result 2 is not what a single call returns). Use a `static inline` function (one evaluation per argument, type-checked), or a GNU statement-expression macro with local temporaries when you need generic code.
