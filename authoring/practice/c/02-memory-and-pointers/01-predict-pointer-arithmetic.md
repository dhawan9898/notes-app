---
kind: question
title: Predict pointer arithmetic and sizeof
module: Memory and Pointers
difficulty: Medium
---

Given `int a[5] = {10, 20, 30, 40, 50}; int *p = a + 1;` what are `*p`, `p[2]`, `*(a + 3)` and `&a[4] - p`? What are `sizeof a` and `sizeof p`, and what does `sizeof arr` give *inside* a function declared `f(int arr[])`? How many bytes separate `(char *)(p + 1)` and `(char *)p`?

## Explanation

```c
#include <stdio.h>

static size_t size_in_function(int arr[]) { return sizeof arr; }      /* arr is really an int * here */

int main(void) {
    int a[5] = {10, 20, 30, 40, 50};
    int *p = a + 1;
    printf("*p = %d, p[2] = %d, *(a + 3) = %d\n", *p, p[2], *(a + 3));
    printf("&a[4] - p = %td elements\n", &a[4] - p);
    printf("sizeof a = %zu, sizeof p = %zu, sizeof inside function = %zu\n", sizeof a, sizeof p, size_in_function(a));
    printf("(char *)(p + 1) - (char *)p = %td bytes\n", (char *)(p + 1) - (char *)p);
    printf("a == &a[0]: %d, (void *)&a == (void *)a: %d\n", a == &a[0], (void *)&a == (void *)a);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p2_ptr p2_ptr.c 2>&1 | grep warning | sed 's/^[^ ]* //'; ./p2_ptr; rm -f p2_ptr
warning: ‘sizeof’ on array function parameter ‘arr’ will return size of ‘int *’ [-Wsizeof-array-argument]
*p = 20, p[2] = 40, *(a + 3) = 40
&a[4] - p = 3 elements
sizeof a = 20, sizeof p = 8, sizeof inside function = 8
(char *)(p + 1) - (char *)p = 4 bytes
a == &a[0]: 1, (void *)&a == (void *)a: 1
```

`p` points at `a[1]`, so `*p` is 20 and `p[2]` is `a[3]` = 40. Pointer subtraction counts *elements*: `&a[4] - p` is 3. `sizeof a` is the whole array (20 bytes) but `sizeof p` is the pointer (8). An array parameter is really a pointer, so inside the function `sizeof arr` is also 8, and the compiler warns about it; always pass the length separately. Adding 1 to an `int *` moves 4 bytes.
