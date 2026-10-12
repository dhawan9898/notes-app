---
title: "C From Zero: Your First Programs"
track: c
module: Start Here
moduleOrder: 0
order: 2
level: beginner
summary: "Write, compile and run your first C programs: variables and types, printf and input, if/for/while, functions, arrays and strings, and how to read compiler errors and warnings."
prerequisites:
  - A Linux (or macOS/WSL) terminal and gcc or clang installed
sources:
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 1 (A Tutorial Introduction) and Ch. 2-4
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 2-10
  - label: ISO/IEC 9899:2018 §6.4, §6.5, §6.8 (lexical elements, expressions, statements)
  - label: GCC manual — Options to Request or Suppress Warnings
    url: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
---

A **program** is a list of instructions for a computer. C is a *compiled* language: you write text in a **source file**, a program called the **compiler** translates it into a machine-code file, and then you run that file. The cycle is always: edit → compile → run → read what happened.

## Your first program

Save this as `hello.c`:

```c
#include <stdio.h>

int main(void) {
    printf("Hello, world\n");
    return 0;
}
```

Compile and run it:

```bash
gcc -std=c17 -Wall -Wextra -o hello hello.c
./hello
```

What each part means:

- `#include <stdio.h>` pulls in the declarations for the standard input/output functions (`printf`, ...).
- `int main(void)` is where every C program starts. `int` means it returns a whole number to the operating system.
- `printf("...\n")` prints text; `\n` is a newline.
- `return 0;` tells the operating system the program succeeded. Any other number means failure.
- `gcc` is the compiler; `-o hello` names the output file; `-Wall -Wextra` turn on the compiler's helpful warnings (always use them); `./hello` runs the result (`./` means "in this directory").

## Variables, types and printing

A **variable** is a named box in memory holding a value. Its **type** says what kind of value and how many bytes it occupies.

{{include c/beginner/first.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o first first.c && ./first}}

- Common types: `int` (whole numbers, 4 bytes here), `double` (numbers with a fractional part), `char` (one character, 1 byte).
- `printf` fills in **format specifiers** from its extra arguments: `%d` for an `int`, `%f`/`%.2f` for a `double`, `%c` for a `char`, `%s` for a string, `%zu` for a size. The specifier must match the argument's type or the output is garbage (the compiler warns you).
- **Integer division drops the fraction**: `7 / 2` is `3`, not `3.5`. Make one side a `double` (`7 / 2.0`) to get `3.5`. `%` is the remainder (`7 % 2` is `1`).

## Making decisions and repeating things

{{include c/beginner/control.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o control control.c && ./control}}

- `if / else if / else` runs one branch. Conditions use `==` (equal), `!=`, `<`, `>`, `<=`, `>=`, and `&&` (and), `||` (or), `!` (not). **A single `=` assigns; `==` compares.** Mixing them up is the most famous beginner bug.
- `for (start; condition; step)` repeats while the condition holds. `while (condition)` is the same idea without the built-in counter.
- In C, "true" is any non-zero value and "false" is `0`.
- A **function** is named, reusable code: `int square(int x)` takes an `int` and returns an `int`. Functions let you break a problem into small parts you can test separately. `static` on a function means "private to this file".
- An **array** (`int scores[] = {...}`) holds several values of the same type in a row, numbered from **0**. The last valid index is `count - 1`. C does **not** check that an index is in range (see *Pointers for Beginners* and *Strings and Buffer Safety*).
- A **string** is an array of `char` ending with a zero byte (`'\0'`). `"Ada"` needs 4 bytes: three letters and the terminator, which is why the program prints 4 for the array and 3 for the letters.

## Reading input safely

{{include c/beginner/input.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o input input.c && echo 21 | ./input && echo abc | ./input}}

(Here `echo 21 |` feeds the program a line of input, standing in for you typing it.) Rules to remember:

- Read a line with `fgets(buffer, sizeof buffer, stdin)`: it stops before overflowing the buffer. **Never use `gets`** (it was removed from the language because it cannot be used safely) and avoid `scanf("%s")` without a width limit.
- Convert text to a number with `strtol` and check the `end` pointer to see whether any digits were found. Never assume the user typed what you asked for.

## Reading compiler messages

The compiler is trying to help. Messages come in two kinds: an **error** stops compilation (the code is not valid C), a **warning** means the code compiles but is probably wrong. Read the *first* message first; later ones are often consequences of it.

{{include c/beginner/broken.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra broken.c -o broken 2>&1 | sed 's#.*/##'}}

The message gives the file, line and column (`broken.c:5:5`), the kind (`error`), and what it expected. Line 5 is reported because the compiler only noticed the missing `;` when it reached `printf`: **the real mistake is often on the line before the one reported**. The second error is a *typo*: `total` was never declared. Fix errors from the top.

Warnings catch bugs the language allows:

{{include c/beginner/warn.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra warn.c -o warn 2>&1 | grep -E "warning|note"}}

`x` was used before being given a value (its contents are whatever garbage was in that memory), and `%d` was given a string. Treat every warning as a bug until proven otherwise; many teams compile with `-Werror` so warnings fail the build.

## Pitfalls

- `=` versus `==`.
- Forgetting the `;` or a closing `}`; the error appears lines later.
- Using a variable before assigning it.
- Off-by-one: an array of `n` elements has indices `0` to `n-1`.
- Integer division and overflow: `int` holds about ±2.1 billion; larger results wrap around.
- Ignoring warnings.
- `printf` format that does not match the argument type.

## Key takeaways

1. Edit, compile with `-Wall -Wextra`, run; read errors from the top and warnings as bugs.
2. Variables have types; the type decides size and what operations mean; integer division drops the fraction.
3. `if`, `for`, `while` and functions are all the control flow you need to start.
4. Arrays are numbered from 0 and C never checks bounds; strings are `char` arrays ending in a zero byte.
5. Read input with `fgets` and `strtol`, and check the result.

**Next:** *Binary, Hexadecimal and Bits* and *Pointers for Beginners*.
