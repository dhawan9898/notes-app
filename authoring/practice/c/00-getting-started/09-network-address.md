---
kind: code
title: Network address of an IPv4 prefix
module: Getting Started
difficulty: Medium
tags:
  - bits
  - networking
  - masks
starter: c/starters/start-here/ex9-network.c
examples:
  - input: 10.20.77.130 / 21
    output: 10.20.72.0
  - input: 172.16.5.4 / 12
    output: 172.16.0.0
  - input: 10.1.2.3 / 0
    output: 0.0.0.0
    why: A /0 mask is all zeros. A shift by 32 is undefined behaviour for a 32-bit value.
constraints:
  - '`0 <= prefix <= 32`.'
  - 'Do not shift a 32-bit value by 32 or more: handle `prefix == 0` separately or shift in 64 bits.'
hints:
  - 'The mask for /p has p ones followed by zeros: `~0u << (32 - p)`.'
  - 'Check /0 and /32: one of them makes the shift count 32.'
---

An IPv4 address is a 32-bit number and a prefix length `/p` says that the first `p` bits are the network part. Write `uint32_t network_of(uint32_t ip, int prefix)` that returns `ip` with the last `32 - prefix` host bits cleared. The macro `IP(a, b, c, d)` builds an address from its four bytes.

## Explanation

Mask for /21 is `0xFFFFFFFF << (32 - 21)`; `ip & mask` gives `10.20.72.0` (the third octet 77 rounds down to a multiple of 8).
