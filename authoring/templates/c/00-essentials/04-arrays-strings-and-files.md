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
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 1.9, 5.7 (multi-dimensional arrays), 5.10 (command-line arguments), 7 and 8 (input/output)
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 8, 13 and 22
  - label: ISO/IEC 9899:2018 §7.21 (Input/output), §7.24 (String handling), §5.1.2.2.1 (Program startup)
  - label: "man 3 fopen, man 3 snprintf, man 3 strtol"
    url: https://man7.org/linux/man-pages/man3/fopen.3.html
---

*C From Zero* introduced one-dimensional arrays and strings. This note covers what comes next in nearly every small program: tables of values, working with text, taking input from the command line, and saving and loading data in files. Safe-by-default habits matter here, because C does not check array bounds or string lengths for you.

## Two-dimensional arrays

An array of arrays is a table: `int grid[3][4]` is 3 rows of 4 ints. You index it `grid[row][column]`, and two nested loops visit every cell.

{{include c/essentials/arrays2d.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o arrays2d arrays2d.c && ./arrays2d}}

In memory there is no table: the rows are laid out one after another in a single run of 48 bytes, so `flat[5]` and `grid[1][1]` are the same cell. That is why loops should run with the **row index outermost**: walking along a row visits neighbouring bytes, which the CPU cache loves (see *Performance-Oriented C*). Use named constants (`ROWS`, `COLS`) instead of repeating 3 and 4, so a size change happens in one place. When you pass a 2-D array to a function, the column count must be part of the parameter type (`int g[][COLS]`).

## Strings: the five operations you need

A C string is a `char` array ending with `'\0'`. There is no string type, so the library functions in `<string.h>` and `<stdio.h>` do the work:

{{include c/essentials/strings1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o strings1 strings1.c 2>&1 | grep warning; ./strings1}}

- `strlen(s)` counts characters up to (not including) the `'\0'`; `sizeof name` is the whole array, 16 bytes. They are different questions.
- **Compare with `strcmp`, never `==`.** `==` compares two addresses, not the characters, and GCC warns "comparison with string literal results in unspecified behavior". `strcmp` returns 0 when equal, negative or positive otherwise.
- **Build text with `snprintf(buf, sizeof buf, ...)`.** It never writes past the size you give it, and it returns how many characters the full result *needed*, so you can detect truncation: the `small` buffer got `"much "` while the return value says 13 were wanted. GCC even spots the truncation at compile time, as the warning line shows.
- **Avoid `strcpy`, `strcat` and `gets`.** They do not know the destination size and are the classic source of buffer overflows; the notes *Strings and Buffer Safety* and *Secure C Coding* show the attacks.
- Characters are small integers, so `toupper` from `<ctype.h>` converts a letter; cast to `unsigned char` first, as shown, because negative values are undefined.

## Reading a whole line safely

`scanf("%s")` stops at the first space and can overflow a buffer. `fgets` reads a full line and takes a size:

{{include c/essentials/lines1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o lines1 lines1.c && echo "the quick brown fox" | ./lines1; rm -f lines1}}

`fgets(line, sizeof line, stdin)` reads at most `sizeof line - 1` characters, always terminates the string, and **keeps the newline**. `strcspn(line, "\n")` finds the newline's position so you can overwrite it with `'\0'`. It returns `NULL` at end of input, so always test it. (The output above comes from piping text in; run the program interactively and type your own sentence.)

## Command-line arguments: argc and argv

`main` can receive the words typed after the program name: `int main(int argc, char *argv[])`. `argc` is the count including the program name, and `argv[i]` are the words as strings. Numbers arrive as *text*, so convert with `strtol`, which also lets you detect bad input (unlike `atoi`, which silently returns 0):

{{include c/essentials/args1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o args1 args1.c && ./args1 4 5 6; ./args1 4 x; echo "exit status: $?"; rm -f args1}}

`strtol(text, &end, 10)` stores in `end` where parsing stopped; if `*end` is not `'\0'`, part of the text was not a number. Error messages go to `stderr` (`fprintf(stderr, ...)`), and `return 1` tells the shell the program failed: the shell variable `$?` shows the exit status, and `0` always means success.

## Files: fopen, fprintf, fscanf, fclose

A `FILE *` is a handle to an open file. The cycle is always **open, check, use, close**:

{{include c/essentials/files1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o files1 files1.c && ./files1; rm -f files1 scores.txt}}

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
