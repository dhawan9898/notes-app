---
kind: question
title: Wrap-around
module: Getting Started
difficulty: Easy
---

What are `(uint8_t)-1`, `(uint8_t)-128`, `(int8_t)200` and `(uint8_t)(255 + 1)`? Explain each in terms of two's complement.

## Explanation

`-1` in 8 bits is `1111 1111` = **255**; `-128` is `1000 0000` = **128**; `200` does not fit in a signed 8-bit type, so the conversion is *implementation-defined* (GCC keeps the bit pattern `1100 1000`, which as signed is **-56**); `255 + 1` is computed as an `int` (256) and converting to 8 bits keeps the low 8 bits, giving **0**.
