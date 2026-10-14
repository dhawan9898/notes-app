---
title: "Arrays, Strings, Command-Line Arguments and Files"
track: c
module: C Essentials
moduleOrder: 0.5
order: 4
level: beginner
summary: "Everyday C data handling for beginners: two-dimensional arrays, the string.h basics (strlen, strcmp, snprintf), reading a line safely, command-line arguments with argc and argv, and reading and writing text files with fopen, fprintf and fscanf, all compiled and run."
prerequisites:
  - "C From Zero: Your First Programs"
  - Pointers for Beginners
  - "Structs, Enums and Typedef"
sources:
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 1.9, 5.7 (multi-dimensional arrays), 5.10 (command-line arguments), 7 and 8 (input/output)"
  - label: "King — C Programming, A Modern Approach, 2nd ed., Ch. 8, 13 and 22"
  - label: "ISO/IEC 9899:2018 §7.21 (Input/output), §7.24 (String handling), §5.1.2.2.1 (Program startup)"
  - label: "man 3 fopen, man 3 snprintf, man 3 strtol"
    url: https://man7.org/linux/man-pages/man3/fopen.3.html
---

*C From Zero* introduced one-dimensional arrays and strings. This note covers what comes next in nearly every small program: tables of values, working with text, taking input from the command line, and saving and loading data in files. Safe-by-default habits matter here, because C does not check array bounds or string lengths for you.

## Two-dimensional arrays

An array of arrays is a table: `int grid[3][4]` is 3 rows of 4 ints. You index it `grid[row][column]`, and two nested loops visit every cell.

```c
#include <stdio.h>

#define ROWS 3
#define COLS 4

int main(void) {
    int grid[ROWS][COLS];                         /* 3 rows of 4 ints, stored row after row in memory */
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = r * 10 + c;              /* grid[row][column] */

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) printf("%3d", grid[r][c]);
        printf("\n");
    }
    printf("sizeof grid = %zu bytes = %d rows x %d cols x %zu bytes\n", sizeof grid, ROWS, COLS, sizeof grid[0][0]);

    int *flat = &grid[0][0];                      /* the same memory seen as one long row */
    printf("flat[5] = %d is grid[1][1] = %d\n", flat[5], grid[1][1]);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o arrays2d arrays2d.c && ./arrays2d
  0  1  2  3
 10 11 12 13
 20 21 22 23
sizeof grid = 48 bytes = 3 rows x 4 cols x 4 bytes
flat[5] = 11 is grid[1][1] = 11
```

In memory there is no table: the rows are laid out one after another in a single run of 48 bytes, so `flat[5]` and `grid[1][1]` are the same cell. That is why loops should run with the **row index outermost**: walking along a row visits neighbouring bytes, which the CPU cache loves (see *Performance-Oriented C*). Use named constants (`ROWS`, `COLS`) instead of repeating 3 and 4, so a size change happens in one place. When you pass a 2-D array to a function, the column count must be part of the parameter type (`int g[][COLS]`).

## Strings: the five operations you need

A C string is a `char` array ending with `'\0'`. There is no string type, so the library functions in `<string.h>` and `<stdio.h>` do the work:

```c
#include <stdio.h>
#include <ctype.h>
#include <string.h>

int main(void) {
    char name[16] = "Ada";                        /* room for 15 letters plus the final '\0' */
    printf("\"%s\" has length %zu but occupies %zu bytes\n", name, strlen(name), sizeof name);

    if (strcmp(name, "Ada") == 0) printf("equal: strcmp returns 0 when strings match\n");

    char greeting[32];
    snprintf(greeting, sizeof greeting, "Hello, %s!", name);   /* bounded: cannot write past 32 bytes */
    printf("%s\n", greeting);

    const char *long_text = "much too long";
    char small[6];
    int needed = snprintf(small, sizeof small, "%s", long_text);
    printf("small holds \"%s\" (cut off); the full text needed %d characters\n", small, needed);

    for (size_t i = 0; name[i] != '\0'; i++) name[i] = (char)toupper((unsigned char)name[i]);
    printf("upper-cased in place: %s\n", name);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o strings1 strings1.c 2>&1 | grep warning; ./strings1
strings1.c:17:49: warning: ‘%s’ directive output truncated writing 13 bytes into a region of size 6 [-Wformat-truncation=]
"Ada" has length 3 but occupies 16 bytes
equal: strcmp returns 0 when strings match
Hello, Ada!
small holds "much " (cut off); the full text needed 13 characters
upper-cased in place: ADA
```

- `strlen(s)` counts characters up to (not including) the `'\0'`; `sizeof name` is the whole array, 16 bytes. They are different questions.
- **Compare with `strcmp`, never `==`.** `==` compares two addresses, not the characters, and GCC warns "comparison with string literal results in unspecified behavior". `strcmp` returns 0 when equal, negative or positive otherwise.
- **Build text with `snprintf(buf, sizeof buf, ...)`.** It never writes past the size you give it, and it returns how many characters the full result *needed*, so you can detect truncation: the `small` buffer got `"much "` while the return value says 13 were wanted. GCC even spots the truncation at compile time, as the warning line shows.
- **Avoid `strcpy`, `strcat` and `gets`.** They do not know the destination size and are the classic source of buffer overflows; the notes *Strings and Buffer Safety* and *Secure C Coding* show the attacks.
- Characters are small integers, so `toupper` from `<ctype.h>` converts a letter; cast to `unsigned char` first, as shown, because negative values are undefined.

## Reading a whole line safely

`scanf("%s")` stops at the first space and can overflow a buffer. `fgets` reads a full line and takes a size:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    char line[64];
    printf("Type a sentence: ");
    if (!fgets(line, sizeof line, stdin)) return 1;   /* reads at most 63 chars, keeps the newline */
    line[strcspn(line, "\n")] = '\0';                  /* cut the newline off */
    int words = 0, in_word = 0;
    for (const char *p = line; *p; p++) {
        if (*p == ' ') in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    printf("\"%s\" has %zu characters and %d words\n", line, strlen(line), words);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o lines1 lines1.c && echo "the quick brown fox" | ./lines1; rm -f lines1
Type a sentence: "the quick brown fox" has 19 characters and 4 words
```

`fgets(line, sizeof line, stdin)` reads at most `sizeof line - 1` characters, always terminates the string, and **keeps the newline**. `strcspn(line, "\n")` finds the newline's position so you can overwrite it with `'\0'`. It returns `NULL` at end of input, so always test it. (The output above comes from piping text in; run the program interactively and type your own sentence.)

## Command-line arguments: argc and argv

`main` can receive the words typed after the program name: `int main(int argc, char *argv[])`. `argc` is the count including the program name, and `argv[i]` are the words as strings. Numbers arrive as *text*, so convert with `strtol`, which also lets you detect bad input (unlike `atoi`, which silently returns 0):

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {               /* argc = how many words, argv = the words themselves */
    printf("program name: %s, arguments given: %d\n", argv[0], argc - 1);
    long total = 0;
    for (int i = 1; i < argc; i++) {
        char *end;
        long v = strtol(argv[i], &end, 10);       /* text -> number, and a way to detect junk */
        if (*end != '\0') { fprintf(stderr, "'%s' is not a whole number\n", argv[i]); return 1; }
        total += v;
    }
    printf("sum = %ld\n", total);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o args1 args1.c && ./args1 4 5 6; ./args1 4 x; echo "exit status: $?"; rm -f args1
program name: ./args1, arguments given: 3
sum = 15
program name: ./args1, arguments given: 2
exit status: 1
'x' is not a whole number
```

`strtol(text, &end, 10)` stores in `end` where parsing stopped; if `*end` is not `'\0'`, part of the text was not a number. Error messages go to `stderr` (`fprintf(stderr, ...)`), and `return 1` tells the shell the program failed: the shell variable `$?` shows the exit status, and `0` always means success.

## Files: fopen, fprintf, fscanf, fclose

A `FILE *` is a handle to an open file. The cycle is always **open, check, use, close**:

```c
#include <stdio.h>

int main(void) {
    FILE *out = fopen("scores.txt", "w");         /* "w": create or overwrite for writing */
    if (!out) { perror("scores.txt"); return 1; } /* ALWAYS check: the file may be unwritable */
    fprintf(out, "ada 91\nlinus 78\ngrace 99\n");
    fclose(out);                                  /* flushes buffered data to disk */

    FILE *in = fopen("scores.txt", "r");
    if (!in) { perror("scores.txt"); return 1; }
    char name[32];
    int score, count = 0, sum = 0;
    while (fscanf(in, "%31s %d", name, &score) == 2) {   /* returns how many items it read */
        printf("%-6s %3d\n", name, score);
        sum += score; count++;
    }
    fclose(in);
    printf("%d scores, average %.1f\n", count, (double)sum / count);

    FILE *missing = fopen("no-such-file.txt", "r");
    fflush(stdout);                               /* keep stdout and stderr lines in order */
    if (!missing) perror("no-such-file.txt");     /* prints the reason, e.g. "No such file or directory" */
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o files1 files1.c && ./files1; rm -f files1 scores.txt
ada     91
linus   78
grace   99
3 scores, average 89.3
no-such-file.txt: No such file or directory
```

- `fopen(path, "w")` creates or empties a file for writing, `"r"` reads, `"a"` appends. It returns `NULL` on failure, so **always check**; `perror` prints your label plus the operating system's reason ("No such file or directory", "Permission denied").
- `fprintf` is `printf` aimed at a file; `fscanf` is `scanf` from a file. `fscanf` returns the number of items it successfully read, which is how the `while` loop knows when the file ends or the data is malformed. The `%31s` width keeps a long word from overflowing the 32-byte `name` buffer.
- `fclose` flushes buffered data and releases the handle. Forgetting it can lose the last few bytes and leaks a descriptor.
- For binary data use `"rb"`/`"wb"` with `fread`/`fwrite`. The note *File I/O in C: stdio vs POSIX, Buffering, Short Reads and Durability* explains buffering and what survives a crash.

## Pitfalls

- **Off-by-one on strings:** a 16-byte array holds 15 characters plus the terminator.
- **Not checking `fopen`, `fgets` or `fscanf` return values** turns a missing file into a crash or an infinite loop.
- **`scanf("%s")` without a width** and `gets` allow buffer overflows.
- **`argv[1]` without checking `argc`** crashes when the user supplies no arguments.
- **Comparing strings with `==`** or assigning with `=` (`name = "Bob"` is an error for an array).

## Key takeaways

1. A 2-D array is one block of memory read row by row; keep the row index in the outer loop and name the sizes.
2. Compare strings with `strcmp`, build them with `snprintf`, read lines with `fgets`, and avoid `strcpy`, `strcat` and `gets`.
3. `argc`/`argv` deliver arguments as text; convert with `strtol` and check the result.
4. For files, open, check for `NULL`, use, then close; use `perror` to explain failures and `0`/non-zero exit codes to report success.
