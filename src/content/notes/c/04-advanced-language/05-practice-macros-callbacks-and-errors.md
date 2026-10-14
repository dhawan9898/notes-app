---
title: "Practice: Macros, Callbacks and Error Handling"
track: c
module: Advanced Language
moduleOrder: 4
order: 5
level: advanced
summary: "Five exercises on the advanced-language module: macro traps (precedence and double evaluation), a function-pointer dispatch table and qsort comparators, error handling with errno and a single cleanup path, and a type-checked variadic logger, with verified answers."
prerequisites:
  - "The Preprocessor and Macros"
  - "Function Pointers, Callbacks, Opaque Types and Intrusive Containers"
  - "Error Handling, Variadic Functions and setjmp/longjmp"
sources:
  - label: "ISO/IEC 9899:2018 §6.10.3 (Macro replacement), §7.16 (Variable arguments), §7.5 (Errors)"
  - label: "SEI CERT C Coding Standard — PRE31-C, PRE00-C, ERR30-C, FIO42-C"
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: "GCC manual — Function Attributes (format)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
---

Macros and callbacks are where C gives you power and no safety net. Predict first, then run. Compile with `gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined`.

## Exercises

**1. Macro traps.** Given `#define SQUARE_BAD(x) x * x`, what is `SQUARE_BAD(1 + 2)`? Fix the macro. Then, for `#define MAX_BAD(a, b) ((a) > (b) ? (a) : (b))`, what happens with `MAX_BAD(next(), 0)` where `next()` increments a global counter and returns it? Replace the macro with something that evaluates each argument once.

**2. A dispatch table.** Write a table of `{name, function pointer}` entries for `add`, `sub` and `mul` (each `int (*)(int, int)`) and call each through the table with arguments 6 and 3, without a `switch`.

**3. Sorting with callbacks.** Sort an array of `struct {const char *name; int age;}` first by age, then by name, using `qsort`. Write both comparators. Why must a comparator never be written as `return a->age - b->age;`?

**4. One cleanup path.** Write `char *slurp(const char *path, size_t *len)` that reads a whole file into a NUL-terminated buffer. It must return `NULL` with `errno` set on every failure, free everything it allocated and close the file on **every** error path, using a single cleanup label. Why save `errno` before cleaning up?

**5. A checked logger.** Write `log_msg(const char *level, const char *fmt, ...)` that prints `[level] message` by forwarding to `vfprintf`. Which GCC attribute makes the compiler check the format string against the arguments?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

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

**2.**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int (*op)(int, int); } Command;      /* a dispatch table */

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }
static int sub(int a, int b) { return a - b; }

typedef struct { const char *name; int age; } Person;

static int by_age(const void *x, const void *y) {
    const Person *a = x, *b = y;
    return (a->age > b->age) - (a->age < b->age);
}
static int by_name(const void *x, const void *y) {
    return strcmp(((const Person *)x)->name, ((const Person *)y)->name);
}

int main(void) {
    const Command table[] = {{"add", add}, {"sub", sub}, {"mul", mul}};
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        printf("%s(6, 3) = %d\n", table[i].name, table[i].op(6, 3));

    Person people[] = {{"Linus", 55}, {"Ada", 36}, {"Grace", 85}, {"Dennis", 70}};
    size_t n = sizeof people / sizeof people[0];
    qsort(people, n, sizeof people[0], by_age);
    printf("by age:  "); for (size_t i = 0; i < n; i++) printf("%s(%d) ", people[i].name, people[i].age); printf("\n");
    qsort(people, n, sizeof people[0], by_name);
    printf("by name: "); for (size_t i = 0; i < n; i++) printf("%s ", people[i].name); printf("\n");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_fnptr p4_fnptr.c && ./p4_fnptr; rm -f p4_fnptr
add(6, 3) = 9
sub(6, 3) = 3
mul(6, 3) = 18
by age:  Ada(36) Linus(55) Dennis(70) Grace(85) 
by name: Ada Dennis Grace Linus 
```

The table pairs a name with a function pointer; `table[i].op(6, 3)` calls through it. Adding a command means adding one table row, not editing a `switch`. The type `int (*op)(int, int)` reads "pointer to a function taking two ints and returning int".

**3.** The same program sorts with both comparators (the by-age line, then the by-name line above). `qsort` calls the comparator with `const void *`, so the first line of each comparator casts to the real type. The comparator must return negative, zero or positive; `(a > b) - (a < b)` does that for integers without overflow, whereas `a->age - b->age` can overflow for large values (and for unsigned types never goes negative). Use `strcmp` for strings.

**4.**

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a whole file into memory. Returns NULL and sets errno on failure; single exit point cleans up. */
static char *slurp(const char *path, size_t *len_out) {
    char *buf = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;                                   /* errno already set by fopen */
    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    long size = ftell(f);
    if (size < 0) goto fail;
    rewind(f);
    buf = malloc((size_t)size + 1);
    if (!buf) goto fail;
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) { errno = EIO; goto fail; }
    buf[size] = '\0';
    fclose(f);
    *len_out = (size_t)size;
    return buf;
fail:
    {
        int saved = errno;                                 /* fclose/free may change errno */
        free(buf);
        fclose(f);
        errno = saved;
    }
    return NULL;
}

int main(void) {
    FILE *t = fopen("sample.txt", "w");
    if (!t) { perror("sample.txt"); return 1; }
    fputs("hello from a file\n", t);
    fclose(t);

    size_t len;
    char *text = slurp("sample.txt", &len);
    if (text) { printf("read %zu bytes: %s", len, text); free(text); }

    text = slurp("does-not-exist.txt", &len);
    if (!text) printf("slurp failed: %s (errno %d)\n", strerror(errno), errno);
    remove("sample.txt");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o p4_errors p4_errors.c && ./p4_errors; rm -f p4_errors sample.txt
read 18 bytes: hello from a file
slurp failed: No such file or directory (errno 2)
```

The `goto fail` pattern keeps one cleanup block instead of repeating `free`/`fclose` at every error. Each failure sets `errno` (library calls do it automatically; the short read sets `EIO` by hand). `errno` is saved because `free` and `fclose` are allowed to change it, and the caller needs the *original* reason: the second call shows "No such file or directory (errno 2)". The AddressSanitizer build confirms there are no leaks or invalid accesses on either path. Note that `goto` for cleanup is accepted style in C; jumping *backwards* or into scopes is what makes code unreadable.

**5.**

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

</details>

## Key takeaways

1. Macros substitute text: parenthesise parameters and body, and remember they can evaluate an argument more than once; prefer `static inline` functions.
2. Function pointers plus a table replace a `switch` and make behaviour data-driven; `qsort` comparators must return a sign without overflowing.
3. Use one cleanup path (`goto fail`) for functions that acquire several resources, and save `errno` before cleanup calls.
4. Mark printf-like wrappers with `__attribute__((format(...)))` so format mistakes are caught at compile time.
