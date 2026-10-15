---
title: "C From Zero: Your First Programs"
track: c
module: Getting Started
moduleOrder: 0
order: 1
level: beginner
summary: "Write, compile and run your first C programs: variables and types, printf and input, if/for/while, functions, arrays and strings, and how to read compiler errors and warnings."
prerequisites:
  - A Linux (or macOS/WSL) terminal and gcc or clang installed
sources:
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 1 (A Tutorial Introduction) and Ch. 2-4"
  - label: "King — C Programming, A Modern Approach, 2nd ed., Ch. 2-10"
  - label: "ISO/IEC 9899:2018 §6.4, §6.5, §6.8 (lexical elements, expressions, statements)"
  - label: "GCC manual — Options to Request or Suppress Warnings"
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

```c
#include <stdio.h>

int main(void) {
    int apples = 3;                 /* a whole number */
    double price = 0.5;             /* a number with a fractional part */
    char grade = 'A';               /* a single character */

    double total = apples * price;  /* an expression: the compiler works out the type */
    printf("I bought %d apples at %.2f each, total %.2f, grade %c\n", apples, price, total, grade);

    printf("sizes in bytes: int=%zu double=%zu char=%zu\n", sizeof(int), sizeof(double), sizeof(char));
    printf("7 / 2 = %d, 7 %% 2 = %d, 7 / 2.0 = %.1f\n", 7 / 2, 7 % 2, 7 / 2.0);   /* integer division drops the fraction */
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o first first.c && ./first
I bought 3 apples at 0.50 each, total 1.50, grade A
sizes in bytes: int=4 double=8 char=1
7 / 2 = 3, 7 % 2 = 1, 7 / 2.0 = 3.5
```

- Common types: `int` (whole numbers, 4 bytes here), `double` (numbers with a fractional part), `char` (one character, 1 byte).
- `printf` fills in **format specifiers** from its extra arguments: `%d` for an `int`, `%f`/`%.2f` for a `double`, `%c` for a `char`, `%s` for a string, `%zu` for a size. The specifier must match the argument's type or the output is garbage (the compiler warns you).
- **Integer division drops the fraction**: `7 / 2` is `3`, not `3.5`. Make one side a `double` (`7 / 2.0`) to get `3.5`. `%` is the remainder (`7 % 2` is `1`).

## Making decisions and repeating things

```c
#include <stdio.h>

/* A function: a named, reusable piece of code that takes inputs and returns a result. */
static int square(int x) { return x * x; }

static int sum_array(const int values[], int count) {
    int total = 0;
    for (int i = 0; i < count; i++) {          /* i = 0, 1, 2, ... count-1 */
        total += values[i];
    }
    return total;
}

int main(void) {
    for (int n = 1; n <= 15; n++) {
        if (n % 15 == 0)      printf("FizzBuzz ");
        else if (n % 3 == 0)  printf("Fizz ");
        else if (n % 5 == 0)  printf("Buzz ");
        else                  printf("%d ", n);
    }
    printf("\n");

    int scores[] = { 90, 72, 85, 60 };
    int count = (int)(sizeof scores / sizeof scores[0]);     /* number of elements = total bytes / bytes per element */
    printf("sum = %d, average = %.1f, square(9) = %d\n", sum_array(scores, count), (double)sum_array(scores, count) / count, square(9));

    int countdown = 3;
    while (countdown > 0) { printf("%d... ", countdown); countdown--; }
    printf("liftoff\n");

    char name[] = "Ada";                       /* a string is an array of chars ending with a zero byte */
    printf("%s has %zu letters; first letter %c; bytes in the array: %zu\n", name, sizeof name - 1, name[0], sizeof name);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o control control.c && ./control
1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz 
sum = 307, average = 76.8, square(9) = 81
3... 2... 1... liftoff
Ada has 3 letters; first letter A; bytes in the array: 4
```

- `if / else if / else` runs one branch. Conditions use `==` (equal), `!=`, `<`, `>`, `<=`, `>=`, and `&&` (and), `||` (or), `!` (not). **A single `=` assigns; `==` compares.** Mixing them up is the most famous beginner bug.
- `for (start; condition; step)` repeats while the condition holds. `while (condition)` is the same idea without the built-in counter.
- In C, "true" is any non-zero value and "false" is `0`.
- A **function** is named, reusable code: `int square(int x)` takes an `int` and returns an `int`. Functions let you break a problem into small parts you can test separately. `static` on a function means "private to this file".
- An **array** (`int scores[] = {...}`) holds several values of the same type in a row, numbered from **0**. The last valid index is `count - 1`. C does **not** check that an index is in range (see *Pointers for Beginners* and *Strings and Buffer Safety*).
- A **string** is an array of `char` ending with a zero byte (`'\0'`). `"Ada"` needs 4 bytes: three letters and the terminator, which is why the program prints 4 for the array and 3 for the letters.

## Reading input safely

```c
#include <stdio.h>
#include <stdlib.h>

/* Read a line safely with fgets (it never writes past the buffer), then convert it. Never use gets(); avoid scanf("%s"). */
int main(void) {
    char line[64];
    printf("Enter a whole number: ");
    if (fgets(line, sizeof line, stdin) == NULL) { printf("no input\n"); return 1; }

    char *end;
    long value = strtol(line, &end, 10);          /* end points at the first character that was NOT part of the number */
    if (end == line) { printf("that was not a number: %s", line); return 1; }
    printf("you typed %ld; doubled it is %ld\n", value, value * 2);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o input input.c && echo 21 | ./input && echo abc | ./input
Enter a whole number: you typed 21; doubled it is 42
Enter a whole number: that was not a number: abc
```

(Here `echo 21 |` feeds the program a line of input, standing in for you typing it.) Rules to remember:

- Read a line with `fgets(buffer, sizeof buffer, stdin)`: it stops before overflowing the buffer. **Never use `gets`** (it was removed from the language because it cannot be used safely) and avoid `scanf("%s")` without a width limit.
- Convert text to a number with `strtol` and check the `end` pointer to see whether any digits were found. Never assume the user typed what you asked for.

## Reading compiler messages

The compiler is trying to help. Messages come in two kinds: an **error** stops compilation (the code is not valid C), a **warning** means the code compiles but is probably wrong. Read the *first* message first; later ones are often consequences of it.

```c
#include <stdio.h>

int main(void) {
    int count = 5
    printf("%d\n", count);
    printf("%d\n", total);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra broken.c -o broken 2>&1 | sed 's#.*/##'
broken.c: In function ‘main’:
broken.c:5:5: error: expected ‘,’ or ‘;’ before ‘printf’
    5 |     printf("%d\n", count);
      |     ^~~~~~
broken.c:6:20: error: ‘total’ undeclared (first use in this function)
    6 |     printf("%d\n", total);
      |                    ^~~~~
broken.c:6:20: note: each undeclared identifier is reported only once for each function it appears in
broken.c:4:9: warning: unused variable ‘count’ [-Wunused-variable]
    4 |     int count = 5
      |         ^~~~~
```

The message gives the file, line and column (`broken.c:5:5`), the kind (`error`), and what it expected. Line 5 is reported because the compiler only noticed the missing `;` when it reached `printf`: **the real mistake is often on the line before the one reported**. The second error is a *typo*: `total` was never declared. Fix errors from the top.

Warnings catch bugs the language allows:

```c
#include <stdio.h>

int main(void) {
    int unused;
    int x;
    printf("x is %d\n", x);          /* x was never given a value */
    printf("%d\n", "hello");         /* wrong format for a string */
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra warn.c -o warn 2>&1 | grep -E "warning|note"
warn.c:7:14: warning: format ‘%d’ expects argument of type ‘int’, but argument 2 has type ‘char *’ [-Wformat=]
warn.c:4:9: warning: unused variable ‘unused’ [-Wunused-variable]
warn.c:6:5: warning: ‘x’ is used uninitialized [-Wuninitialized]
warn.c:5:9: note: ‘x’ was declared here
```

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
