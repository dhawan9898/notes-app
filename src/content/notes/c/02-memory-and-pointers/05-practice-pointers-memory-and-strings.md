---
title: "Practice: Pointers, Memory and Strings"
track: c
module: Memory and Pointers
moduleOrder: 2
order: 5
level: intermediate
summary: "Six exercises on the memory module: predicting pointer arithmetic and sizeof, fixing a dangling pointer three ways, finding and fixing a leak with LeakSanitizer, measuring a stack frame, and building a growable string with realloc, with verified answers."
prerequisites:
  - "Pointers, Arrays and Decay"
  - "Stack Frames and the x86-64 System V Calling Convention"
  - "Dynamic Memory: malloc Internals, Classic Bugs and Custom Allocators"
  - "Strings and Buffer Safety"
sources:
  - label: "ISO/IEC 9899:2018 §6.5.6 (Additive operators), §6.7.6.3 (Function declarators), §7.22.3 (Memory management)"
  - label: "SEI CERT C Coding Standard — MEM30-C, MEM31-C, DCL30-C"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+08.+Memory+Management+%28MEM%29
  - label: "AddressSanitizer and LeakSanitizer documentation"
    url: https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer
---

These exercises ask you to predict, then to find real bugs with sanitizers. Compile with `gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined` unless an exercise says otherwise, and read every warning.

## Exercises

**1. Predict.** Given `int a[5] = {10, 20, 30, 40, 50}; int *p = a + 1;` what are `*p`, `p[2]`, `*(a + 3)` and `&a[4] - p`? What are `sizeof a` and `sizeof p`, and what does `sizeof arr` give *inside* a function declared `f(int arr[])`? How many bytes separate `(char *)(p + 1)` and `(char *)p`?

**2. Dangling pointer.** The function below returns the address of a local array. Explain what the compiler and the run time do, then fix it **two different ways** (one where the caller provides the buffer, one that allocates).

```c
char *make_greeting_bad(const char *name) {
    char buf[32];
    snprintf(buf, sizeof buf, "Hello, %s", name);
    return buf;
}
```

**3. Find the leak.** Run the program below under AddressSanitizer (LeakSanitizer is included). Read the report and fix `student_free`.

```c
static void student_free(Student *s) { free(s->name); }
```

(`Student` holds a heap-allocated `name`, a heap-allocated `scores` array and is itself heap-allocated.)

**4. Measure a stack frame.** Write a recursive function with a 200-byte local array that records the address of that array at each depth, and use the difference between depth 0 and depth 4 to estimate the size of one frame. Which way does the stack grow?

**5. A growable string.** Write `Builder` (`char *data; size_t len, cap;`) with `sb_init` and `sb_append(Builder *, const char *)` that grows by doubling with `realloc`. It must not leak, must not lose the old buffer when `realloc` fails, and must keep the string NUL-terminated.

**6. Short answer.** (a) Why is `char *s = "abc"; s[0] = 'x';` a bug while `char s[] = "abc"; s[0] = 'x';` is fine? (b) Why does `strncpy(dst, src, sizeof dst)` not guarantee a valid string? (c) What is the difference between `free(p)` followed by `p = NULL` and just `free(p)`?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

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

**2.** The compiler warns "function returns address of local variable". The array lives in the function's stack frame, which is gone after the return; using the pointer is undefined behaviour. In practice GCC even replaces the returned address with `NULL`, so the program below crashes in `strlen`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* BUG: returns the address of a local array. */
static char *make_greeting_bad(const char *name) {
    char buf[32];
    snprintf(buf, sizeof buf, "Hello, %s", name);
    return buf;
}

/* Fix 1: the caller supplies the buffer. */
static void make_greeting_buf(const char *name, char *out, size_t cap) { snprintf(out, cap, "Hello, %s", name); }

/* Fix 2: allocate; the caller must free. */
static char *make_greeting_heap(const char *name) {
    size_t n = strlen("Hello, ") + strlen(name) + 1;
    char *s = malloc(n);
    if (s) snprintf(s, n, "Hello, %s", name);
    return s;
}

int main(void) {
    char mine[32];
    make_greeting_buf("Ada", mine, sizeof mine);
    printf("%s\n", mine);
    char *h = make_greeting_heap("Grace");
    if (h) printf("%s\n", h);
    free(h);
#ifdef SHOW_BUG
    char *bad = make_greeting_bad("Linus");
    printf("%s\n", bad);                              /* undefined behaviour: the stack frame is gone */
#else
    (void)make_greeting_bad;
#endif
    return 0;
}
```

```text
$ gcc -std=c17 -g -fsanitize=address -DSHOW_BUG -o bug p2_dangling.c 2>&1 | grep warning | sed "s/^[^ ]* //"; ./bug 2>&1 | grep -E "ERROR|SUMMARY" | sed -e "s/==[0-9]*==//" -e "s/ (pc .*//"; gcc -std=c17 -Wall -Wextra -g -fsanitize=address -o good p2_dangling.c 2>/dev/null && ./good; rm -f bug good
warning: function returns address of local variable [-Wreturn-local-addr]
ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
SUMMARY: AddressSanitizer: SEGV ../../../../src/libsanitizer/sanitizer_common/sanitizer_libc.cpp:176 in __sanitizer::internal_strlen(char const*)
Hello, Ada
Hello, Grace
```

Fix 1 (`make_greeting_buf`): the caller owns the buffer and passes its size. Fix 2 (`make_greeting_heap`): allocate with `malloc`, document that the caller must `free`, and handle `NULL`. A third option is a `static` buffer, which is not re-entrant or thread-safe and is usually a poor choice.

**3.** The buggy version (frees only the name):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; int *scores; int n; } Student;

static Student *student_new(const char *name, int n) {
    Student *s = malloc(sizeof *s);
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = calloc((size_t)n, sizeof(int));
    s->n = n;
    return s;
}

static void student_free(Student *s) { free(s->name); }      /* forgets the other two allocations */

int main(void) {
    Student *s = student_new("Ada", 3);
    printf("%s has %d scores\n", s->name, s->n);
    student_free(s);
    return 0;
}
```

```text
$ gcc -std=c17 -g -fsanitize=address -o p2_leak_bug p2_leak_bug.c && ./p2_leak_bug 2>&1 | grep -E "ERROR|leak of|SUMMARY" | sed -e 's/==[0-9]*==//' -e 's#/[^ ]*/##'; rm -f p2_leak_bug
ERROR: LeakSanitizer: detected memory leaks
Direct leak of 24 byte(s) in 1 object(s) allocated from:
Indirect leak of 12 byte(s) in 1 object(s) allocated from:
SUMMARY: AddressSanitizer: 36 byte(s) leaked in 2 allocation(s).
```

Two allocations are lost: the `Student` struct itself (24 bytes, the *direct* leak) and the `scores` array it points to (12 bytes, an *indirect* leak, because the only pointer to it was inside the leaked struct). The corrected program frees all three, **children before the parent**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; int *scores; int n; } Student;

static Student *student_new(const char *name, int n) {
    Student *s = malloc(sizeof *s);
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = calloc((size_t)n, sizeof(int));
    s->n = n;
    return s;
}

static void student_free(Student *s) {
    free(s->name);                    /* BUG in the first version: forgot s->scores and s itself */
    free(s->scores);
    free(s);
}

int main(void) {
    Student *s = student_new("Ada", 3);
    s->scores[0] = 90;
    printf("%s has %d scores, first = %d\n", s->name, s->n, s->scores[0]);
    student_free(s);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address -o p2_leak p2_leak.c && ./p2_leak; echo "exit status $?"; rm -f p2_leak
Ada has 3 scores, first = 90
exit status 0
```

A clean run with exit status 0 means LeakSanitizer found nothing.

**4.**

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

**5.**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t len, cap; } Builder;

static int sb_init(Builder *b) { b->cap = 16; b->len = 0; b->data = malloc(b->cap); if (!b->data) return 0; b->data[0] = '\0'; return 1; }

static int sb_append(Builder *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        while (b->len + n + 1 > cap) cap *= 2;                      /* grow geometrically: amortised O(1) per byte */
        char *bigger = realloc(b->data, cap);
        if (!bigger) return 0;                                      /* keep the old buffer valid on failure */
        b->data = bigger; b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
    return 1;
}

int main(void) {
    Builder b;
    if (!sb_init(&b)) return 1;
    for (int i = 1; i <= 5; i++) {
        char piece[32];
        snprintf(piece, sizeof piece, "item-%d;", i);
        if (!sb_append(&b, piece)) return 1;
    }
    printf("\"%s\" length %zu capacity %zu\n", b.data, b.len, b.cap);
    free(b.data);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o p2_builder p2_builder.c && ./p2_builder; rm -f p2_builder
"item-1;item-2;item-3;item-4;item-5;" length 35 capacity 64
```

Key points: grow geometrically (`cap *= 2`) so appending is amortised O(1) per byte; assign `realloc`'s result to a *temporary* so a failure does not overwrite and leak `data`; copy `n + 1` bytes to carry the terminator; free once at the end. The 35-character result forced the capacity from 16 up to 64.

**6.** (a) A string literal lives in read-only memory; `char *s` just points at it, so writing crashes (undefined behaviour). `char s[] = "abc"` creates a modifiable **copy** in the array. Use `const char *` for literals so the compiler catches this. (b) `strncpy` does **not** add a terminating NUL when the source is as long as or longer than the limit; and it pads short strings with zeros (wasteful). Prefer `snprintf(dst, sizeof dst, "%s", src)`, which always terminates and reports truncation. (c) After `free(p)` the pointer still holds the stale address; using it is a use-after-free and freeing it again is a double free. Setting `p = NULL` makes a second `free(p)` harmless and a later dereference crash immediately and predictably, but it does not fix other copies of the pointer.

</details>

## Key takeaways

1. Pointer arithmetic counts elements, not bytes; an array parameter is a pointer, so `sizeof` inside a function is not the array size.
2. Never return the address of a local variable; make the caller supply the buffer or allocate and document ownership.
3. Free in reverse order of ownership (children before parent); LeakSanitizer reports direct and indirect leaks.
4. Stacks grow toward lower addresses on x86-64; frame size is measurable, and deep recursion is bounded by it.
5. Grow buffers geometrically, keep `realloc`'s result in a temporary, and prefer `snprintf` over `strncpy`.
