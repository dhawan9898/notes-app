---
kind: question
title: Short answers on strings and free
module: Memory and Pointers
difficulty: Medium
---

(a) Why is `char *s = "abc"; s[0] = 'x';` a bug while `char s[] = "abc"; s[0] = 'x';` is fine? (b) Why does `strncpy(dst, src, sizeof dst)` not guarantee a valid string? (c) What is the difference between `free(p)` followed by `p = NULL` and just `free(p)`?

## Explanation

(a) A string literal lives in read-only memory; `char *s` just points at it, so writing crashes (undefined behaviour). `char s[] = "abc"` creates a modifiable **copy** in the array. Use `const char *` for literals so the compiler catches this. (b) `strncpy` does **not** add a terminating NUL when the source is as long as or longer than the limit; and it pads short strings with zeros (wasteful). Prefer `snprintf(dst, sizeof dst, "%s", src)`, which always terminates and reports truncation. (c) After `free(p)` the pointer still holds the stale address; using it is a use-after-free and freeing it again is a double free. Setting `p = NULL` makes a second `free(p)` harmless and a later dereference crash immediately and predictably, but it does not fix other copies of the pointer.
