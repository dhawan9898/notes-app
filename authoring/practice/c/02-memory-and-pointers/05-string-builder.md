---
kind: code
title: Growable string builder
module: Memory and Pointers
difficulty: Hard
tags:
  - realloc
  - strings
  - error handling
starter: c/starters/pointers/ex5-builder.c
examples:
  - input: sb_init(&b); sb_append(&b, "item-1;"); sb_append(&b, "item-2;");
    output: b.data = "item-1;item-2;", b.len = 14
  - input: realloc fails during sb_append
    output: returns 0; b.data and b.len unchanged
constraints:
  - '`sb_init` and `sb_append` return 1 on success and 0 on failure.'
  - 'There is always room for the terminating NUL: `cap > len` after every successful call.'
  - Never assign the result of `realloc` straight to `b->data`.
hints:
  - Compute `need = b->len + strlen(s) + 1` before touching the buffer.
  - Grow into a local `char *p = realloc(...)`, check it, then update `data` and `cap`.
  - Use `memcpy` to append and write the NUL yourself.
---

A `Builder` has `data` (a NUL-terminated string), `len` (its length) and `cap` (bytes allocated). Implement `sb_init` and `sb_append`.

`sb_append` doubles `cap` until the new text and its terminating NUL fit. If `realloc` fails, it returns 0 and leaves the builder **exactly as it was**: the old string must survive.

## Explanation

Key points: grow geometrically (`cap *= 2`) so appending is amortised O(1) per byte; assign `realloc`'s result to a *temporary* so a failure does not overwrite and leak `data`; copy `n + 1` bytes to carry the terminator; free once at the end. The 35-character result forced the capacity from 16 up to 64.
