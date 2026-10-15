---
kind: code
title: A bit toolkit for uint32_t
module: Systems-Level C
difficulty: Medium
tags:
  - bits
  - masks
  - rotate
starter: c/starters/bits/ex1-bit-toolkit.c
examples:
  - input: popcount32(0xF0F0)
    output: '8'
  - input: rotl32(0x80000001, 1)
    output: '0x00000003'
  - input: byte_swap(0x11223344)
    output: '0x44332211'
  - input: is_pow2(0)
    output: 0 (false)
    why: 0 is not a power of two even though `0 & (0 - 1) == 0`.
constraints:
  - Bit indexes `i` are 0 to 31; rotate counts `r` are 0 to 31.
  - '`rotl32(x, 0)` must return `x`: `x >> 32` is undefined.'
hints:
  - 'A power of two has exactly one set bit: `x != 0 && (x & (x - 1)) == 0`.'
  - 'Rotate: `(x << r) | (x >> ((32 - r) & 31))`.'
  - Use `1u << i`, not `1 << i`, so bit 31 is not a signed shift.
---

Implement seven small bit operations on `uint32_t`: `popcount32`, `is_pow2`, `rotl32` (rotate left), `set_bit`, `clear_bit`, `toggle_bit` and `byte_swap`.

All must be free of undefined behaviour: in particular a rotate by 0 and shifts by large counts need care.

## Explanation

`x &= x - 1` clears the lowest set bit, so `popcount32` loops once per 1 bit (Kernighan's method; the hardware instruction is available as `__builtin_popcount`). A power of two has exactly one set bit, so `x & (x - 1)` is zero, and the `x != 0` guard handles 0. `rotl32` masks the count to 0..31 and special-cases 0 so no shift by 32 occurs. Single-bit edits use `|`, `& ~` and `^` with `1u << i` (the `u` matters: `1 << 31` on a signed `int` is undefined).
