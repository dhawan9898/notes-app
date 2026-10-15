---
kind: question
title: Short answers on buffering and durability
module: Systems-Level C
difficulty: Hard
---

(a) Why does a program that prints with `printf` and then crashes sometimes lose its last lines, while `write(1, ...)` does not? (b) What does `fsync(fd)` guarantee that `fflush(fp)` does not? (c) When is `read()` returning 0 *not* an error?

## Explanation

(a) `printf` writes into a **user-space buffer** that is flushed at exit, on newline for a terminal, or when full; a crash skips the flush. `write` goes straight to the kernel. (Use `fflush`, `setvbuf`, or write demos to `stderr`.) (b) `fflush` only moves data from your process's buffer to the kernel; `fsync` asks the kernel to push it to **stable storage**, which is what makes data survive a power failure (directories need their own `fsync` too). (c) At **end of file**: `read` returns 0 when the other end of a pipe or socket is closed or the file offset is at the end. Only `-1` signals an error.
