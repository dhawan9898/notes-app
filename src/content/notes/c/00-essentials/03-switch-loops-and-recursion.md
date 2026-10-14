---
title: "More Control Flow: switch, Loop Control and Recursion"
track: c
module: C Essentials
moduleOrder: 0.5
order: 3
level: beginner
summary: "The rest of C's control flow that beginners need: switch and why a missing break falls through, do-while, break and continue, and recursion explained with a base case, a recursive case and what the call stack is doing, with compiled programs."
prerequisites:
  - "C From Zero: Your First Programs"
  - "Structs, Enums and Typedef"
sources:
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 3 (Control Flow) and §4.10 (Recursion)"
  - label: "King — C Programming, A Modern Approach, 2nd ed., Ch. 5-6 and §9.6 (Recursion)"
  - label: "ISO/IEC 9899:2018 §6.8.4.2 (The switch statement), §6.8.5 (Iteration statements), §6.8.6 (Jump statements)"
  - label: "GCC manual — Warning Options (-Wimplicit-fallthrough)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
---

*C From Zero* gave you `if`, `for` and `while`. This note adds the remaining tools you will see in almost every real C program: `switch`, `do`-`while`, `break`/`continue`, and the idea of a function that calls itself.

## switch: choosing among many fixed values

`switch` compares one integer (or `char`, or enum) against a list of `case` labels and jumps to the match. It is tidier than a long chain of `if`/`else if` when you test one value against constants.

```c
#include <stdio.h>

static const char *day_kind(int d) {          /* 1 = Monday ... 7 = Sunday */
    switch (d) {
        case 6:
        case 7:                               /* two labels, one body: weekend */
            return "weekend";
        case 1: case 2: case 3: case 4: case 5:
            return "weekday";
        default:                              /* anything else */
            return "not a day";
    }
}

int main(void) {
    for (int d = 0; d <= 7; d += 3) printf("day %d is %s\n", d, day_kind(d));

    int n = 0, sum = 0;
    do {                                      /* body runs first, condition is checked after */
        n++;
        if (n % 2 == 0) continue;             /* skip the rest of this round */
        if (n > 9) break;                     /* leave the loop completely */
        sum += n;
    } while (n < 100);
    printf("sum of odd numbers up to 9 = %d (loop stopped at n = %d)\n", sum, n);

    int grade = 2;
    switch (grade) {                          /* the classic bug: no break means "fall through" */
        case 1: printf("one ");
        case 2: printf("two ");
        case 3: printf("three ");
        default: printf("(fell through to the end)\n");
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o control1 control1.c 2>&1 | grep -E "warning" | head -3; ./control1
control1.c:29:17: warning: this statement may fall through [-Wimplicit-fallthrough=]
control1.c:30:17: warning: this statement may fall through [-Wimplicit-fallthrough=]
control1.c:31:17: warning: this statement may fall through [-Wimplicit-fallthrough=]
day 0 is not a day
day 3 is weekday
day 6 is weekend
sum of odd numbers up to 9 = 25 (loop stopped at n = 11)
two three (fell through to the end)
```

Read the three parts:

1. **Grouped labels.** `case 6:` followed directly by `case 7:` shares one body. Use this to say "6 or 7".
2. **`default`** catches every value no label matched. Always include it unless you are switching over an enum and want the compiler to warn about unhandled values.
3. **Fall-through.** Once a case matches, execution continues *downwards through every following case* until a `break`, a `return` or the closing brace. In the last block `grade` is 2, so it prints `two`, `three` and the default text. That is almost never what you want, and `-Wextra` warns about it, as the output above shows. Put a `break;` at the end of every case (or `return`, as `day_kind` does). When fall-through is intentional, write a comment and use `[[fallthrough]];` (C23) or `__attribute__((fallthrough));` (GCC/Clang) so the warning knows.

`switch` works only on integer-like values: you cannot switch on a string or a `double`; use `if` with `strcmp` for strings.

## do-while, break and continue

A `while` loop checks its condition **before** each round, so its body may run zero times. A `do { ... } while (cond);` loop checks **after**, so the body always runs at least once; it suits "ask, then validate" menus and retry loops. Note the semicolon after the closing `)`.

Two statements change a loop's normal rhythm:

- `continue;` skips the rest of the current round and goes to the next check (in a `for`, to the update step).
- `break;` leaves the innermost loop (or `switch`) immediately.

In the program, even numbers are skipped with `continue`, and at `n > 9` the loop `break`s, so the sum is 1+3+5+7+9 = 25 and `n` ends at 11. Use these sparingly: if a loop body is full of `continue` and `break`, restructuring the condition is usually clearer. `break` only exits **one** level, so leaving two nested loops needs a flag variable (or moving the loops into a function and using `return`).

## Recursion: a function that calls itself

Some problems are naturally described as "do a small step, then solve the same problem on something smaller". Factorial is the classic: `5! = 5 × 4!`, and `4! = 4 × 3!`, down to `1! = 1`. A function that solves this by calling itself is **recursive**. Every correct recursive function has two parts:

- a **base case** that returns an answer directly without recursing (`n <= 1`), which stops the chain;
- a **recursive case** that calls itself with a *smaller* argument, moving toward the base case.

```c
#include <stdio.h>

static unsigned long factorial(unsigned n) {
    if (n <= 1) return 1;                     /* base case: stops the recursion */
    return n * factorial(n - 1);              /* recursive case: a smaller copy of the same problem */
}

static void countdown(int n) {
    if (n == 0) { printf("liftoff\n"); return; }
    printf("%d ", n);
    countdown(n - 1);
}

static int sum_digits(int n) {                /* 1234 -> 4 + sum_digits(123) */
    if (n < 10) return n;
    return n % 10 + sum_digits(n / 10);
}

int main(void) {
    printf("5! = %lu, 10! = %lu\n", factorial(5), factorial(10));
    countdown(5);
    printf("digits of 98765 add up to %d\n", sum_digits(98765));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o recur1 recur1.c && ./recur1
5! = 120, 10! = 3628800
5 4 3 2 1 liftoff
digits of 98765 add up to 35
```

How it works: each call gets its own copy of its parameters and local variables on the **call stack** (a region of memory that grows with each call and shrinks on each return). `factorial(5)` calls `factorial(4)`, which calls `factorial(3)`, and so on until `factorial(1)` returns 1; then each waiting call multiplies and returns in reverse order. `countdown` prints *before* recursing, so numbers appear in order; had it printed after the recursive call, they would appear in reverse (try it). *Stack Frames and the x86-64 System V Calling Convention* shows what one stack frame contains.

Recursion is not magic and not always the best tool:

- **Forget the base case or never move toward it**, and the function calls itself forever until the stack runs out and the program crashes with a *stack overflow* (a segmentation fault).
- **Depth costs memory.** A loop uses constant space; a recursion 100,000 deep uses 100,000 stack frames. `factorial(25)` also overflows `unsigned long`, an example of the integer limits described in *Binary, Hexadecimal and Bits*.
- Some recursions repeat work enormously (naive Fibonacci recomputes the same values exponentially many times). The DSA notes *Thinking in Data Structures* and *Dynamic Programming* show the fix, called memoization.
- Use recursion where the data is itself recursive (folders in folders, trees) or the problem splits naturally into smaller copies; use a loop when a simple counter will do.

## Pitfalls

- **Missing `break`** in a `case`: falls through silently.
- **Declaring a variable directly after a `case` label** is not allowed in older C; wrap the body in braces `{ }`.
- **`break` inside `switch` inside a loop** leaves only the `switch`, not the loop.
- **Infinite `do-while`/`while`** when the condition variable is never updated.
- **Recursion without a reachable base case**, or with very deep recursion on large inputs.

## Key takeaways

1. `switch` jumps to a matching `case`, continues until `break`/`return`, and needs `default`; compile with `-Wextra` to catch accidental fall-through.
2. `do-while` always runs its body once; `continue` skips to the next round and `break` leaves the innermost loop or switch.
3. A recursive function needs a base case and a recursive case that makes the problem smaller; each call uses stack memory.
4. Prefer a loop for simple counting and recursion for naturally nested or divide-and-conquer problems.
