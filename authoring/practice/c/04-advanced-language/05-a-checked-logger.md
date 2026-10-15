---
kind: question
title: A checked logger
module: Advanced Language
difficulty: Hard
---

Write `log_msg(const char *level, const char *fmt, ...)` that prints `[level] message` by forwarding to `vfprintf`. Which GCC attribute makes the compiler check the format string against the arguments?

## Explanation

```c
#include <stdarg.h>
#include <stdio.h>

static void log_msg(const char *level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void log_msg(const char *level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%s] ", level);
    vfprintf(stdout, fmt, ap);                 /* forward the arguments to the v-version */
    fputc('\n', stdout);
    va_end(ap);
}

int main(void) {
    log_msg("info", "started with %d workers", 4);
    log_msg("warn", "disk %s is %d%% full", "/data", 91);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_variadic p4_variadic.c && ./p4_variadic; rm -f p4_variadic
[info] started with 4 workers
[warn] disk /data is 91% full
```

`va_start`/`va_end` bracket the argument list and `vfprintf` consumes it (the `v` variants take a `va_list`). `__attribute__((format(printf, 2, 3)))` tells GCC and Clang that argument 2 is a printf-style format and arguments from 3 on are its values, so `log_msg("info", "%d", "text")` produces a compile-time warning instead of a run-time crash. Without the attribute a variadic function is completely unchecked.
