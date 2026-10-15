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

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Predict.** Given `int a[5] = {10, 20, 30, 40, 50}; int *p = a + 1;` what are `*p`, `p[2]`, `*(a + 3)` and `&a[4] - p`? What are `sizeof a` and `sizeof p`, and what does `sizeof arr` give *inside* a function declared `f(int arr[])`? How many bytes separate `(char *)(p + 1)` and `(char *)p`?

**2. Dangling pointer.** The function below returns the address of a local array. Explain what the compiler and the run time do, then fix it **two different ways** (one where the caller provides the buffer, one that allocates).

```c
char *make_greeting_bad(const char *name) {
    char buf[32];
    snprintf(buf, sizeof buf, "Hello, %s", name);
    return buf;
}
```

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: two correct replacements for a function that returned the address of a local array.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Fix 1: the caller supplies the buffer and its size; write "Hello, <name>" into it (truncate if it does not fit). */
void make_greeting_buf(const char *name, char *out, size_t cap) {
    (void)name; (void)out; (void)cap;
}

/* Fix 2: return a newly allocated "Hello, <name>" that the caller frees, or NULL if memory runs out. */
char *make_greeting_heap(const char *name) {
    (void)name;
    return NULL;
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
    char mine[32] = "";
    make_greeting_buf("Ada", mine, sizeof mine);
    check_str("make_greeting_buf(\"Ada\", 32-byte buffer)", mine, "Hello, Ada");
    char small[8] = "";
    make_greeting_buf("Grace", small, sizeof small);
    check_str("make_greeting_buf(\"Grace\", 8-byte buffer) truncates", small, "Hello, ");
    char *h = make_greeting_heap("Linus");
    check_str("make_greeting_heap(\"Linus\")", h, "Hello, Linus");
    free(h);
    char *long_name = make_greeting_heap("Margaret Hamilton, Apollo flight software");
    check_str("make_greeting_heap(long name) is not truncated", long_name, "Hello, Margaret Hamilton, Apollo flight software");
    free(long_name);
    return summary();
}
```

</details>

**3. Find the leak.** Run the program below under AddressSanitizer (LeakSanitizer is included). Read the report and fix `student_free`.

```c
static void student_free(Student *s) { free(s->name); }
```

(`Student` holds a heap-allocated `name`, a heap-allocated `scores` array and is itself heap-allocated.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: student_free must release everything student_new allocated.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests count allocations: malloc, calloc and free below go through these counters. */
static int live_blocks;
static void *counted_malloc(size_t n) { void *p = malloc(n); if (p) live_blocks++; return p; }
static void *counted_calloc(size_t k, size_t n) { void *p = calloc(k, n); if (p) live_blocks++; return p; }
static void counted_free(void *p) { if (p) live_blocks--; free(p); }
#define malloc(n) counted_malloc(n)
#define calloc(k, n) counted_calloc(k, n)
#define free(p) counted_free(p)

typedef struct { char *name; int *scores; int n; } Student;

static Student *student_new(const char *name, int n) {
    Student *s = malloc(sizeof *s);
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = calloc((size_t)n, sizeof(int));
    s->n = n;
    return s;
}

/* ===== Your code: edit below this line ===== */
void student_free(Student *s) {
    free(s->name);
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
    Student *s = student_new("Ada", 3);
    check_int("blocks allocated by student_new", live_blocks, 3);
    student_free(s);
    check_int("blocks still allocated after student_free", live_blocks, 0);
    Student *t = student_new("Grace", 0);
    student_free(t);
    check_int("blocks still allocated after a second student", live_blocks, 0);
    return summary();
}
```

</details>

**4. Measure a stack frame.** Write a recursive function with a 200-byte local array that records the address of that array at each depth, and use the difference between depth 0 and depth 4 to estimate the size of one frame. Which way does the stack grow?

**5. A growable string.** Write `Builder` (`char *data; size_t len, cap;`) with `sb_init` and `sb_append(Builder *, const char *)` that grows by doubling with `realloc`. It must not leak, must not lose the old buffer when `realloc` fails, and must keep the string NUL-terminated.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 5: a growable string that doubles its capacity with realloc.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The tests can make the next realloc fail, to check that a failed append keeps the old string. */
static int fail_next_realloc;
void *test_realloc(void *p, size_t n) {
    if (fail_next_realloc) { fail_next_realloc = 0; return NULL; }
    return realloc(p, n);
}
#define realloc(p, n) test_realloc(p, n)

typedef struct { char *data; size_t len, cap; } Builder;

/* ===== Your code: edit below this line ===== */
/* Start with a small buffer holding "" (len 0). Return 1 on success, 0 if memory runs out. */
int sb_init(Builder *b) {
    b->data = NULL;
    b->len = b->cap = 0;
    return 0;
}

/* Append s, doubling cap until it fits. On failure return 0 and leave b exactly as it was. */
int sb_append(Builder *b, const char *s) {
    (void)b; (void)s;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

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
    Builder b;
    check_int("sb_init succeeds", sb_init(&b), 1);
    if (b.data) {
        check_str("empty builder", b.data, "");
        int ok = 1;
        for (int i = 1; i <= 5; i++) {
            char piece[16];
            snprintf(piece, sizeof piece, "item-%d;", i);
            ok &= sb_append(&b, piece);
        }
        check_int("five appends succeed", ok, 1);
        check_str("contents", b.data, "item-1;item-2;item-3;item-4;item-5;");
        check_int("len", (long long)b.len, 35);
        check_int("cap leaves room for the terminating NUL", b.cap > b.len, 1);
        fail_next_realloc = 1;
        check_int("append when realloc fails returns", sb_append(&b, "this one needs to grow the buffer a lot further than it is now"), 0);
        check_str("  and keeps the old string", b.data, "item-1;item-2;item-3;item-4;item-5;");
        check_int("  and the old len", (long long)b.len, 35);
        fail_next_realloc = 0;
        free(b.data);
    }
    return summary();
}
```

</details>

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
