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
  - label: ISO/IEC 9899:2018 §6.10.3 (Macro replacement), §7.16 (Variable arguments), §7.5 (Errors)
  - label: "SEI CERT C Coding Standard — PRE31-C, PRE00-C, ERR30-C, FIO42-C"
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: GCC manual — Function Attributes (format)
    url: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
---

Macros and callbacks are where C gives you power and no safety net. Predict first, then run. Compile with `gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined`.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Macro traps.** Given `#define SQUARE_BAD(x) x * x`, what is `SQUARE_BAD(1 + 2)`? Fix the macro. Then, for `#define MAX_BAD(a, b) ((a) > (b) ? (a) : (b))`, what happens with `MAX_BAD(next(), 0)` where `next()` increments a global counter and returns it? Replace the macro with something that evaluates each argument once.

**2. A dispatch table.** Write a table of `{name, function pointer}` entries for `add`, `sub` and `mul` (each `int (*)(int, int)`) and call each through the table with arguments 6 and 3, without a `switch`.

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/callbacks/ex2-dispatch.c}}

</details>

**3. Sorting with callbacks.** Sort an array of `struct {const char *name; int age;}` first by age, then by name, using `qsort`. Write both comparators. Why must a comparator never be written as `return a->age - b->age;`?

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/callbacks/ex3-comparators.c}}

</details>

**4. One cleanup path.** Write `char *slurp(const char *path, size_t *len)` that reads a whole file into a NUL-terminated buffer. It must return `NULL` with `errno` set on every failure, free everything it allocated and close the file on **every** error path, using a single cleanup label. Why save `errno` before cleaning up?

**5. A checked logger.** Write `log_msg(const char *level, const char *fmt, ...)` that prints `[level] message` by forwarding to `vfprintf`. Which GCC attribute makes the compiler check the format string against the arguments?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include c/practice/p4_macros.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o p4_macros p4_macros.c && ./p4_macros; rm -f p4_macros}}

The preprocessor substitutes **text**: `SQUARE_BAD(1 + 2)` becomes `1 + 2 * 1 + 2` = 5, not 9. Parenthesise every parameter *and* the whole body: `((x) * (x))`. That fixes precedence but not **double evaluation**: `MAX_BAD(next(), 0)` expands `a` twice, so `next()` runs twice (the counter reached 2, and the printed result 2 is not what a single call returns). Use a `static inline` function (one evaluation per argument, type-checked), or a GNU statement-expression macro with local temporaries when you need generic code.

**2.**

{{include c/practice/p4_fnptr.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o p4_fnptr p4_fnptr.c && ./p4_fnptr; rm -f p4_fnptr}}

The table pairs a name with a function pointer; `table[i].op(6, 3)` calls through it. Adding a command means adding one table row, not editing a `switch`. The type `int (*op)(int, int)` reads "pointer to a function taking two ints and returning int".

**3.** The same program sorts with both comparators (the by-age line, then the by-name line above). `qsort` calls the comparator with `const void *`, so the first line of each comparator casts to the real type. The comparator must return negative, zero or positive; `(a > b) - (a < b)` does that for integers without overflow, whereas `a->age - b->age` can overflow for large values (and for unsigned types never goes negative). Use `strcmp` for strings.

**4.**

{{include c/practice/p4_errors.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o p4_errors p4_errors.c && ./p4_errors; rm -f p4_errors sample.txt}}

The `goto fail` pattern keeps one cleanup block instead of repeating `free`/`fclose` at every error. Each failure sets `errno` (library calls do it automatically; the short read sets `EIO` by hand). `errno` is saved because `free` and `fclose` are allowed to change it, and the caller needs the *original* reason: the second call shows "No such file or directory (errno 2)". The AddressSanitizer build confirms there are no leaks or invalid accesses on either path. Note that `goto` for cleanup is accepted style in C; jumping *backwards* or into scopes is what makes code unreadable.

**5.**

{{include c/practice/p4_variadic.c}}

{{run c/practice :: gcc -std=c17 -Wall -Wextra -o p4_variadic p4_variadic.c && ./p4_variadic; rm -f p4_variadic}}

`va_start`/`va_end` bracket the argument list and `vfprintf` consumes it (the `v` variants take a `va_list`). `__attribute__((format(printf, 2, 3)))` tells GCC and Clang that argument 2 is a printf-style format and arguments from 3 on are its values, so `log_msg("info", "%d", "text")` produces a compile-time warning instead of a run-time crash. Without the attribute a variadic function is completely unchecked.

</details>

## Key takeaways

1. Macros substitute text: parenthesise parameters and body, and remember they can evaluate an argument more than once; prefer `static inline` functions.
2. Function pointers plus a table replace a `switch` and make behaviour data-driven; `qsort` comparators must return a sign without overflowing.
3. Use one cleanup path (`goto fail`) for functions that acquire several resources, and save `errno` before cleanup calls.
4. Mark printf-like wrappers with `__attribute__((format(...)))` so format mistakes are caught at compile time.
