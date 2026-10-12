---
title: Binary, Hexadecimal and Bits
track: c
module: Start Here
moduleOrder: 0
order: 3
level: beginner
summary: "Why computers use binary, how to convert between binary, decimal and hexadecimal, bit operations, flags and masks (with an IPv4 netmask example), negative numbers, overflow and byte order, all with running code."
prerequisites:
  - "C From Zero: Your First Programs"
sources:
  - label: Warren — Hacker's Delight, 2nd ed., Ch. 1-2
  - label: Bryant & O'Hallaron — Computer Systems, A Programmer's Perspective, Ch. 2 (Representing and Manipulating Information)
  - label: ISO/IEC 9899:2018 §6.2.6 (Representations of types) and §6.5.7 (Bitwise shift operators)
  - label: RFC 4632 §3 — Classless Inter-domain Routing (prefix notation)
    url: https://www.rfc-editor.org/rfc/rfc4632
---

Everything inside a computer is stored as **bits**: a bit is a single `0` or `1` (an electrical switch that is off or on). Programmers need to read numbers in binary and hexadecimal because it explains addresses, permissions, network masks, file formats and most bugs involving "how big can this number get".

## Counting in binary

In decimal each digit position is worth ten times the one to its right (ones, tens, hundreds). In **binary** each position is worth **two** times the one to its right: 1, 2, 4, 8, 16, 32, 64, 128...

```text
position value:   128  64  32  16   8   4   2   1
binary 00101010:    0   0   1   0   1   0   1   0   -> 32 + 8 + 2 = 42
```

To convert decimal to binary, repeatedly find the largest position value that fits: 42 = 32 + 8 + 2, so set those three positions to 1. A **byte** is 8 bits and holds 0 to 255 (`2^8 − 1`). In networking a byte is often called an **octet**, which is why an IPv4 address (four octets) is written like `192.168.10.77`: each part is one byte, 0 to 255.

## Hexadecimal: binary shorthand

Long strings of 0s and 1s are hard to read, so people use **hexadecimal** (base 16). It has 16 digits: `0-9` then `A-F` (A=10 ... F=15). One hex digit is exactly **4 bits**, so a byte is exactly two hex digits: `1111 1111` is `FF`, `0010 1010` is `2A`. In C you write hex with the prefix `0x` (`0x2A`), and `printf` prints it with `%x`.

| Decimal | Binary | Hex |
|---|---|---|
| 0 | 0000 | 0 |
| 9 | 1001 | 9 |
| 10 | 1010 | A |
| 15 | 1111 | F |
| 16 | 0001 0000 | 10 |
| 255 | 1111 1111 | FF |

To convert hex to binary, replace each hex digit by its four bits: `0xC0A8` = `1100 0000 1010 1000`. Reading it as bytes, `C0 A8` is **192 . 168**, the start of the familiar private address range.

## Seeing it in code

{{include c/beginner/bits.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o bits bits.c && ./bits}}

### The bit operators

| Operator | Name | Rule | Typical use |
|---|---|---|---|
| `a & b` | AND | 1 only where both are 1 | **keep** some bits (masking); test a bit |
| `a \| b` | OR | 1 where either is 1 | **set** bits |
| `a ^ b` | XOR | 1 where they differ | flip bits |
| `~a` | NOT | flips every bit | build "all but" masks |
| `a << n` | shift left | moves bits left, fills zeros | multiply by `2^n`; build a bit at position `n` |
| `a >> n` | shift right | moves bits right | divide by `2^n`; extract a field |

Note the difference between `&` (bitwise, on numbers) and `&&` (logical, on true/false).

### Flags: many yes/no values in one number

Unix permissions are the classic example: read = 4, write = 2, execute = 1, and `rw-` is `4 | 2 = 6`. The program shows how to test (`perm & WRITE`), set (`perm |= EXEC`) and clear (`perm & ~WRITE`) a flag. The same pattern appears in file-open flags, socket options, hardware registers and TCP flags.

### Masks: the netmask example

An IPv4 address is one 32-bit number. A **netmask** such as `255.255.255.0` is 24 one-bits followed by 8 zero-bits (`/24` just counts the ones). ANDing the address with the mask keeps the **network part** and zeroes the host part: `192.168.10.77 & 255.255.255.0 = 192.168.10.0`. The program prints exactly that, and the part cleared by the mask (`ip & ~mask`) is the host number, 77. Once you see subnetting as "AND with a mask" it stops being memorisation (see *IPv4 Addressing, Subnetting, VLSM and CIDR*).

## Negative numbers: two's complement

Computers store negative integers in **two's complement**: to negate, flip all bits and add 1. For 8 bits, `-1` is `1111 1111` and `-5` is `1111 1011`. The same bit pattern means `251` if you read it as unsigned and `-5` as signed; the type decides. A signed 8-bit number ranges from `-128` to `127`; an unsigned one from `0` to `255`. In general a `n`-bit unsigned number holds `0 … 2^n − 1`.

## Overflow: numbers have a fixed size

A variable has a fixed number of bits, so it cannot grow forever. An unsigned 8-bit value wraps around: `250 + 8` becomes `2` (the demonstration above). C defines this wrap-around for **unsigned** types, but **signed overflow is undefined behavior**: the compiler may assume it never happens (see *Types, Integer Promotion, Conversions and Overflow*). Use a wide enough type, and use the fixed-size types from `<stdint.h>` (`uint8_t`, `int32_t`, `uint64_t`) when the exact size matters, as it does for network and file formats.

## Byte order (endianness)

A number larger than one byte must be stored as several bytes, in some order. **Little-endian** machines (x86, most ARM setups) store the *least significant* byte first; **big-endian** stores the most significant first. The program prints the bytes of `0x12345678` as `78 56 34 12` on this machine: little-endian. Network protocols are defined in **big-endian ("network byte order")**, so programs convert with `htons`/`htonl` when building packets, and you must keep this in mind when reading a packet capture in hex.

## Quick reference: how big is it?

| Bits | Unsigned range | Signed range | Common type |
|---|---|---|---|
| 8 | 0 … 255 | −128 … 127 | `uint8_t`, `char` |
| 16 | 0 … 65,535 | −32,768 … 32,767 | `uint16_t` (a port number) |
| 32 | 0 … 4,294,967,295 | ≈ ±2.1 billion | `uint32_t` (an IPv4 address) |
| 64 | 0 … ≈ 1.8 × 10^19 | ≈ ±9.2 × 10^18 | `uint64_t`, `size_t` |

Powers of two worth memorising: `2^10 = 1,024`, `2^16 = 65,536`, `2^20 ≈ 1 million`, `2^32 ≈ 4.3 billion`.

## Pitfalls

- Confusing `&` with `&&` and `|` with `||`.
- Shifting by an amount that is at least the width of the type (undefined behavior), or shifting a negative signed value.
- Assuming `int` is a particular size; use `<stdint.h>` types for formats.
- Reading hex bytes in the wrong order on a little-endian machine.
- Forgetting that `0x80` is *negative* when stored in a signed 8-bit type.
- Using a signed type for a value that is a set of bit flags; use unsigned.

## Key takeaways

1. Binary positions are powers of two; one hex digit is four bits, so a byte is two hex digits.
2. `&` keeps bits, `|` sets bits, `^` flips bits, shifts multiply and divide by powers of two.
3. A netmask is a pattern of ones; ANDing with it extracts the network part of an address.
4. Two's complement stores negatives; the type decides whether a pattern is read as signed or unsigned.
5. Fixed-size integers overflow (unsigned wraps; signed is undefined); byte order matters whenever bytes cross a machine or network boundary.

**Next:** *Pointers for Beginners*; later in the track, *Bit Manipulation* covers advanced tricks.
