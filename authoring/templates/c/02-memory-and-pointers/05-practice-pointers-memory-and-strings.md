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
  - label: ISO/IEC 9899:2018 §6.5.6 (Additive operators), §6.7.6.3 (Function declarators), §7.22.3 (Memory management)
  - label: "SEI CERT C Coding Standard — MEM30-C, MEM31-C, DCL30-C"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+08.+Memory+Management+%28MEM%29
  - label: AddressSanitizer and LeakSanitizer documentation
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

{{include c/starters/pointers/ex2-greeting.c}}

</details>

**3. Find the leak.** Run the program below under AddressSanitizer (LeakSanitizer is included). Read the report and fix `student_free`.

```c
static void student_free(Student *s) { free(s->name); }
```

(`Student` holds a heap-allocated `name`, a heap-allocated `scores` array and is itself heap-allocated.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/pointers/ex3-leak.c}}

</details>

**4. Measure a stack frame.** Write a recursive function with a 200-byte local array that records the address of that array at each depth, and use the difference between depth 0 and depth 4 to estimate the size of one frame. Which way does the stack grow?

**5. A growable string.** Write `Builder` (`char *data; size_t len, cap;`) with `sb_init` and `sb_append(Builder *, const char *)` that grows by doubling with `realloc`. It must not leak, must not lose the old buffer when `realloc` fails, and must keep the string NUL-terminated.

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/pointers/ex5-builder.c}}

</details>

**6. Short answer.** (a) Why is `char *s = "abc"; s[0] = 'x';` a bug while `char s[] = "abc"; s[0] = 'x';` is fine? (b) Why does `strncpy(dst, src, sizeof dst)` not guarantee a valid string? (c) What is the difference between `free(p)` followed by `p = NULL` and just `free(p)`?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include c/practice/p2_ptr.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o p2_ptr p2_ptr.c 2>&1 | grep warning | sed 's/^[^ ]* //'; ./p2_ptr; rm -f p2_ptr}}

`p` points at `a[1]`, so `*p` is 20 and `p[2]` is `a[3]` = 40. Pointer subtraction counts *elements*: `&a[4] - p` is 3. `sizeof a` is the whole array (20 bytes) but `sizeof p` is the pointer (8). An array parameter is really a pointer, so inside the function `sizeof arr` is also 8, and the compiler warns about it; always pass the length separately. Adding 1 to an `int *` moves 4 bytes.

**2.** The compiler warns "function returns address of local variable". The array lives in the function's stack frame, which is gone after the return; using the pointer is undefined behaviour. In practice GCC even replaces the returned address with `NULL`, so the program below crashes in `strlen`:

{{include c/practice/p2_dangling.c}}

{{run c/practice :: gcc -std=c17 -g -fsanitize=address -DSHOW_BUG -o bug p2_dangling.c 2>&1 | grep warning | sed "s/^[^ ]* //"; ./bug 2>&1 | grep -E "ERROR|SUMMARY" | sed -e "s/==[0-9]*==//" -e "s/ (pc .*//"; gcc -std=c17 -Wall -Wextra -g -fsanitize=address -o good p2_dangling.c 2>/dev/null && ./good; rm -f bug good}}

Fix 1 (`make_greeting_buf`): the caller owns the buffer and passes its size. Fix 2 (`make_greeting_heap`): allocate with `malloc`, document that the caller must `free`, and handle `NULL`. A third option is a `static` buffer, which is not re-entrant or thread-safe and is usually a poor choice.

**3.** The buggy version (frees only the name):

{{include c/practice/p2_leak_bug.c}}

{{run c/practice :: gcc -std=c17 -g -fsanitize=address -o p2_leak_bug p2_leak_bug.c && ./p2_leak_bug 2>&1 | grep -E "ERROR|leak of|SUMMARY" | sed -e 's/==[0-9]*==//' -e 's#/[^ ]*/##'; rm -f p2_leak_bug}}

Two allocations are lost: the `Student` struct itself (24 bytes, the *direct* leak) and the `scores` array it points to (12 bytes, an *indirect* leak, because the only pointer to it was inside the leaked struct). The corrected program frees all three, **children before the parent**:

{{include c/practice/p2_leak.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -g -fsanitize=address -o p2_leak p2_leak.c && ./p2_leak; echo "exit status $?"; rm -f p2_leak}}

A clean run with exit status 0 means LeakSanitizer found nothing.

**4.**

{{include c/practice/p2_frames.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -O0 -o p2_frames p2_frames.c && ./p2_frames; rm -f p2_frames}}

Each frame is the 200-byte array plus the saved return address, saved frame pointer, the `depth` argument and alignment, so a little over 200 bytes (the exact figure depends on compiler and flags). The first address is **higher** than the later ones, so the stack grows toward **lower addresses**. (Do not compile this one with a sanitizer: AddressSanitizer moves locals off the real stack.)

**5.**

{{include c/practice/p2_builder.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o p2_builder p2_builder.c && ./p2_builder; rm -f p2_builder}}

Key points: grow geometrically (`cap *= 2`) so appending is amortised O(1) per byte; assign `realloc`'s result to a *temporary* so a failure does not overwrite and leak `data`; copy `n + 1` bytes to carry the terminator; free once at the end. The 35-character result forced the capacity from 16 up to 64.

**6.** (a) A string literal lives in read-only memory; `char *s` just points at it, so writing crashes (undefined behaviour). `char s[] = "abc"` creates a modifiable **copy** in the array. Use `const char *` for literals so the compiler catches this. (b) `strncpy` does **not** add a terminating NUL when the source is as long as or longer than the limit; and it pads short strings with zeros (wasteful). Prefer `snprintf(dst, sizeof dst, "%s", src)`, which always terminates and reports truncation. (c) After `free(p)` the pointer still holds the stale address; using it is a use-after-free and freeing it again is a double free. Setting `p = NULL` makes a second `free(p)` harmless and a later dereference crash immediately and predictably, but it does not fix other copies of the pointer.

</details>

## Key takeaways

1. Pointer arithmetic counts elements, not bytes; an array parameter is a pointer, so `sizeof` inside a function is not the array size.
2. Never return the address of a local variable; make the caller supply the buffer or allocate and document ownership.
3. Free in reverse order of ownership (children before parent); LeakSanitizer reports direct and indirect leaks.
4. Stacks grow toward lower addresses on x86-64; frame size is measurable, and deep recursion is bounded by it.
5. Grow buffers geometrically, keep `realloc`'s result in a temporary, and prefer `snprintf` over `strncpy`.
