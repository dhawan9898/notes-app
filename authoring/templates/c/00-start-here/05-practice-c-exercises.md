---
title: "Practice: C Exercises"
track: c
module: Start Here
moduleOrder: 0
order: 5
level: beginner
summary: "Ten graded exercises on number systems, expressions, pointers, strings, bits, dynamic memory and reading a sanitizer report, with a verified answer program at the end."
prerequisites:
  - "C From Zero: Your First Programs"
  - Binary, Hexadecimal and Bits
  - Pointers for Beginners
sources:
  - label: King — C Programming, A Modern Approach, 2nd ed., programming projects of Ch. 5-13
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., exercises of Ch. 1-5
---

Do these in order. For each: try it on paper or in a file first, then compile with `gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined`, and only then look at the answers at the end. If you get stuck for more than ten minutes, read the matching section of the previous notes again. A real engineer's habit: *predict before you run*.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Number systems (paper).** Convert `1011 0110` (binary) to decimal and to hexadecimal. Convert decimal `200` to binary and hexadecimal.

**2. Predict the output (paper).** What does each expression evaluate to? `5 / 2`, `5 % 2`, `5 / 2.0`, `10 - 3 * 2`, `7 & 3`, `1 << 4`.

**3. Fix the swap.** This function does not change the caller's variables. Explain why, and rewrite it so that it works.

```c
void swap_ints(int a, int b) { int t = a; a = b; b = t; }
```

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex3-swap.c}}

</details>

**4. Maximum.** Write `int max_of(const int *v, int count)` returning the largest element of an array (assume `count > 0`).

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex4-max.c}}

</details>

**5. Reverse a string in place.** Write `void reverse(char *s)`; `"stressed"` becomes `"desserts"`. (Hint: a string ends at `'\0'`; use `strlen` and swap pairs from both ends.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex5-reverse.c}}

</details>

**6. Count the one-bits.** Write `int popcount(uint32_t x)` returning how many bits of `x` are 1. `popcount(255)` is 8.

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex6-popcount.c}}

</details>

**7. A growable array.** Using `malloc`/`realloc`/`free`, write a small structure holding an `int` array that doubles its capacity when full, with a `vec_push` function. Push ten squares and print the size and capacity. Check that you free the memory.

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex7-vec.c}}

</details>

**8. Find the bug.** This function should sum the first `n` elements. It sometimes returns a wrong value or crashes. Find the bug, then run it under AddressSanitizer and read the report.

```c
int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) total += v[i];
    return total;
}
```

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex8-sum.c}}

</details>

**9. Subnetting with a mask.** Given the address `10.20.77.130` and prefix length 21, compute the network address by building the mask and using `&`. (Check against the "do it in your head" method in *IPv4 Addressing, Subnetting, VLSM and CIDR*.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include c/starters/start-here/ex9-network.c}}

</details>

**10. Wrap-around.** What are `(uint8_t)-1`, `(uint8_t)-128`, `(int8_t)200` and `(uint8_t)(255 + 1)`? Explain each in terms of two's complement.

## Answers

Try before opening.

<details>
<summary>Show answers and a verified program</summary>

1. `1011 0110` = 128 + 32 + 16 + 4 + 2 = **182** = **0xB6** (split into nibbles `1011` = B, `0110` = 6). `200` = 128 + 64 + 8 = `1100 1000` = **0xC8**.
2. `5 / 2` = **2** (integer division); `5 % 2` = **1**; `5 / 2.0` = **2.5**; `10 - 3 * 2` = **4** (multiplication first); `7 & 3` = `0111 & 0011` = **3**; `1 << 4` = **16**.
3. The function receives *copies* of the two numbers; it swaps its copies and the caller's variables are untouched. Pass addresses: `void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }` and call `swap_ints(&x, &y)`.
4. Start with the first element as the best, scan the rest and replace the best when you see a larger one.
5. Swap `s[i]` with `s[n-1-i]` for `i` from 0 up to (not including) `n/2`. Stopping at the middle matters: going to `n` would swap everything back.
6. Test the lowest bit with `x & 1`, add it to a counter, shift right, repeat until `x` is 0 (there are faster methods: see *Bit Manipulation*).
7. Keep `data`, `size` and `cap`; when `size == cap`, `realloc` to double (starting from 4) and **check the result for `NULL` before overwriting your pointer**; doubling makes the average cost of a push constant (see *Dynamic Arrays* in the DSA track).
8. The loop condition `i <= n` runs `n + 1` times and reads one element past the end (`v[n]`): an **off-by-one** error. With `i < n` it is correct. AddressSanitizer reports `stack-buffer-overflow ... READ of size 4` and points at the line.
9. Mask for /21 is `0xFFFFFFFF << (32 - 21)`; `ip & mask` gives `10.20.72.0` (the third octet 77 rounds down to a multiple of 8).
10. `-1` in 8 bits is `1111 1111` = **255**; `-128` is `1000 0000` = **128**; `200` does not fit in a signed 8-bit type, so the conversion is *implementation-defined* (GCC keeps the bit pattern `1100 1000`, which as signed is **-56**); `255 + 1` is computed as an `int` (256) and converting to 8 bits keeps the low 8 bits, giving **0**.

The program below implements and checks all of the above (run under the sanitizers, so any memory mistake would be reported):

{{include c/beginner/answers.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o answers answers.c && ./answers}}

The buggy exercise 8 under AddressSanitizer:

{{include c/beginner/ex8_bug.c}}

{{run c/beginner :: gcc -std=c17 -g -fsanitize=address -o ex8 ex8_bug.c && (./ex8 2>&1 | grep -E "ERROR|READ of|#1 " | sed 's#/[^ ]*/##g; s/==[0-9]*==//; s/ on address.*//; s/ at 0x[0-9a-f]* thread T0//; s/0x[0-9a-f]* in //')}}

</details>

## Key takeaways

1. Predict, compile with warnings and sanitizers, then compare.
2. Most beginner bugs here are: copies instead of addresses, off-by-one loops, forgetting the string terminator, and unchecked allocation.
3. The sanitizer report names the bug, the size of the access and the line.

**Next:** continue with module 1, *The Compilation Pipeline and the ELF Object Model*.
