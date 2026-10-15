---
kind: code
title: Network byte order codec
module: Systems-Level C
difficulty: Medium
tags:
  - endianness
  - bits
  - networking
starter: c/starters/bits/ex3-big-endian.c
examples:
  - input: put_u16(buf, 0xABCD)
    output: buf = AB CD
  - input: get_u32({C0, A8, 01, 02})
    output: '0xC0A80102'
    why: This is 192.168.1.2 as it travels on the wire.
constraints:
  - No casts of the buffer to `uint32_t *` (alignment and aliasing problems).
  - 'Promote the bytes to `uint32_t` before shifting: `p[0] << 24` on a promoted `int` can overflow.'
hints:
  - '`p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;`'
  - Build the value with `(uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | ...`.
---

Network protocols put the most significant byte first (**big-endian**). Implement `put_u16` and `put_u32`, which store a value into a byte buffer in that order, and `get_u32`, which reads it back. Use only shifts and masks: the result must not depend on the machine’s own byte order, and the buffer may be unaligned.

## Explanation

Shifting out each byte gives the same result on every host because shifts operate on *values*, not on memory layout. `memcpy` into a `uint32_t` (rather than casting the buffer pointer) avoids alignment and aliasing problems, and `ntohl` then converts from the wire order to the host order. Never write a struct to the network with `fwrite`: padding and byte order differ between machines.
