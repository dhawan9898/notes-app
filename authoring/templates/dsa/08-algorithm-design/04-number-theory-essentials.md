---
title: Number Theory Essentials
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 4
level: advanced
summary: "Euclid's algorithm and Bezout coefficients, modular exponentiation and inverses, overflow-safe 128-bit mulmod, bit-array and segmented sieves, deterministic Miller-Rabin for all 64-bit integers, fast-doubling Fibonacci, CRT, binomials mod p and a toy RSA — each checked, with the two overflow bugs the tests caught."
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 2, §4.5 (Rational arithmetic, Euclid's algorithm) and §4.5.4 (Factoring into primes; primality tests)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 31 (Number-Theoretic Algorithms)
  - label: Miller — Riemann's Hypothesis and Tests for Primality (J. Computer and System Sciences 13, 1976)
  - label: Rabin — Probabilistic Algorithm for Testing Primality (J. Number Theory 12, 1980)
  - label: Sorenson & Webster — Strong Pseudoprimes to Twelve Prime Bases (Mathematics of Computation 86, 2017)
  - label: Rivest, Shamir, Adleman — A Method for Obtaining Digital Signatures and Public-Key Cryptosystems (CACM 21(2), 1978)
  - label: Hardy & Wright — An Introduction to the Theory of Numbers, 6th ed.
  - label: Warren — Hacker's Delight, 2nd ed. (Chapter 10, integer division by constants; Chapter 11, number systems)
---

Integer arithmetic with a modulus is the substrate of hashing, random number generators, cyclic buffers, error-correcting codes, and all of public-key cryptography. The algorithms are short — most are under ten lines — but each hides a trap: overflow in the intermediate product, a negative remainder, a probabilistic test that the wrong number fools. This note builds the standard toolbox in C, verifies every piece, and records two overflow bugs that the test suite caught in the *first* version of this code.

{{include dsa/numtheory/nt.h}}

## Greatest common divisor and Bezout's identity

**Euclid's algorithm** computes `gcd(a, b)` by replacing `(a, b)` with `(b, a mod b)` until `b = 0`. It takes `O(log min(a, b))` steps; the worst case is consecutive Fibonacci numbers (Lamé's theorem, 1844 — the first analysis of an algorithm's running time). The **extended** algorithm also returns integers `x, y` with `a·x + b·y = gcd(a, b)` (Bezout's identity), by back-substituting through the recursion. These coefficients are what you need for:

- **Modular inverses.** `a⁻¹ mod m` exists iff `gcd(a, m) = 1`, and it is the `x` in `a·x + m·y = 1`. This works for *any* modulus, prime or not.
- **Fermat's little theorem** gives an alternative when `p` is prime: `a⁻¹ ≡ a^(p−2) (mod p)` — one modular exponentiation.
- **Linear Diophantine equations** `a·x + b·y = c` (solvable iff `gcd(a, b) | c`) and the **Chinese remainder theorem**.

**The Chinese remainder theorem** says that for coprime moduli `m₁`, `m₂` the pair of congruences `x ≡ a₁ (m₁)`, `x ≡ a₂ (m₂)` has exactly one solution modulo `m₁m₂`; the code builds it from one inverse. CRT is how big-number RSA decryption is sped up by about 4× (work modulo each prime separately), and how residue-number-system arithmetic splits a large computation into small independent ones.

## Fast modular exponentiation and the overflow trap

`b^e mod m` by repeated squaring takes `O(log e)` multiplications: square the base each step and multiply it into the result when the corresponding bit of `e` is set. Everything else — Fermat inverses, Diffie–Hellman, RSA, primality tests — builds on it.

The trap is that the intermediate product `a·b` of two residues below a 62-bit modulus is up to 124 bits and overflows 64-bit arithmetic *silently* — `uint64_t` wraps, and the answer is plausible and wrong. GCC and Clang provide `unsigned __int128` on 64-bit targets; a single `(u128)a * b % m` is exact. (Without 128-bit support use `a·b mod m` by shift-and-add, or Montgomery multiplication, which also avoids the slow 128-bit division and is what real big-integer libraries use.)

## Primes: sieves

The **sieve of Eratosthenes** marks multiples of each prime starting at its square, in `O(n log log n)` time. Two standard improvements, both implemented below:

1. **Odd numbers only, one bit each**: half the numbers and one-eighth of the space of a byte array — 62.5 MB for all numbers up to 10⁹.
2. **Segmented sieve**: sieve a window of `[lo, hi)` at a time using the primes up to `√hi`. The working set fits in the L1/L2 cache instead of streaming through all of memory, and it needs only `O(√n + block)` memory, which makes ranges like `[10¹², 10¹²+10⁷]` feasible. (In the measurement below the byte-per-number segmented sieve is only about as fast as the bit-packed plain sieve at 10⁹ — the bit-packing already cut memory traffic by 16× — so its real advantage here is memory, and speed for larger limits.)

For a single number, trial division up to `√n` costs `O(√n)` — hopeless for 64-bit inputs. That calls for a primality *test*.

## Miller–Rabin primality

For an odd `n`, write `n − 1 = 2^r · d` with `d` odd. If `n` is prime then for any base `a` either `a^d ≡ 1` or `a^(2^j·d) ≡ −1 (mod n)` for some `0 ≤ j < r`, because the only square roots of 1 modulo a prime are ±1. A composite `n` fails this test for at least 3/4 of all bases, so testing `k` random bases leaves error probability at most `4^−k`. The older **Fermat test** (`a^(n−1) ≡ 1`) is fooled by **Carmichael numbers** — composite `n` for which *every* coprime base passes; the smallest, 561 = 3·11·17, is shown below passing for all 319 coprime bases. Miller–Rabin has no analogue of Carmichael numbers: its worst case is a quarter of the bases, not all of them.

For a *bounded* range the test becomes **deterministic** with a fixed base set: the first twelve primes (2…37) are sufficient for every `n < 3.3·10²⁴` (Sorenson & Webster), hence for all 64-bit integers. The code uses exactly those bases. It is the right primality test for any integer that fits in a machine word: about 0.5 µs per number here.

## Fast Fibonacci and linear recurrences

`F(n)` for huge `n` by the **fast-doubling identities** `F(2k) = F(k)(2F(k+1) − F(k))` and `F(2k+1) = F(k)² + F(k+1)²` needs `O(log n)` steps — about 60 for `n = 10¹⁸`. The general method is **matrix exponentiation**: any linear recurrence of order `k` is a `k×k` matrix power, solved in `O(k³ log n)`.

## Binomial coefficients modulo a prime

Counting problems ask for `C(n, k) mod p` for many `(n, k)`. Precompute factorials and inverse factorials modulo `p` once (`O(n)`, with a single Fermat inverse and a downward sweep for the rest), then each query is two multiplications. The code verifies the table against Pascal's triangle.

## Verification

{{include dsa/numtheory/arith.c}}

{{run dsa/numtheory :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined arith.c -o arith && ./arith}}

The checks run thousands of random cases against naive implementations: `gcd` against trial division, Bezout's identity for the extended algorithm (including negative inputs), `powmod` against repeated multiplication, inverses by both methods, fast-doubling Fibonacci against the iterative definition, CRT against the congruences, and `C(n,k) mod p` against Pascal's triangle. The rest of the output:

- **Overflow demonstration.** For a modulus around 2⁶² the product `(m−3)·(m−5)` is `15` modulo `m`. With plain 64-bit arithmetic the answer is wrong; with the 128-bit intermediate it is `15`.
- **Fermat fooled, Miller–Rabin not.** 561 passes the Fermat test for all 319 coprime bases yet Miller–Rabin correctly reports composite.
- **Known values.** `F(90) = 2 880 067 194 370 816 120` fits in 64 bits, and `F(10¹⁸) mod (10⁹+7) = 209 783 453` takes about sixty squarings.
- **A toy RSA round trip.** Two 32-bit primes found with Miller–Rabin give a 63-bit modulus; `e = 65537`, `d = e⁻¹ mod φ(n)`, and encrypting then decrypting recovers the message. This is **not secure** — a 63-bit modulus is factored instantly — and it uses no padding; real RSA uses ≥ 2048-bit moduli, OAEP/PSS padding and constant-time big-integer arithmetic. It is here to show where the pieces (primality testing, extended Euclid, modular exponentiation) fit; see the *IPsec and IKEv2* and *SSH, PAM, sudo and auditd* notes for how deployed protocols use Diffie–Hellman, RSA and elliptic-curve variants.

### Two bugs the tests caught

1. **`ext_gcd` was right but `inv_euclid` overflowed.** The first version computed `((x % m) + m) % m` to make the coefficient non-negative. For `m` near 2⁶³ the intermediate `x % m + m` exceeds `2⁶³` — **signed overflow, undefined behavior**, which UBSan flagged at once during the RSA round trip (and the result was negative, tripping the `d > 0` assertion). The corrected form is `x %= m; if (x < 0) x += m;`, which never exceeds `m`. The same pattern, `(a % m + m) % m`, is the universal "make it non-negative" idiom and is only safe when `2m` fits in the type.
2. **The simple modular multiplication overflowed** for 62-bit moduli (shown above), which is why `mulmod` exists.

In both cases a test that exercises values *near the type's limits* found what small tests could not. Number-theory code should always be tested at the boundary of its integer type.

## Sieve and primality measurements

{{include dsa/numtheory/sieve.c}}

{{run dsa/numtheory :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined sieve.c -o sieve_asan -lm && ./sieve_asan | head -9 && gcc -std=gnu11 -O2 -Wall -Wextra sieve.c -o sieve -lm && ./sieve | tail -3}}

The first block is the sanitizer build: both sieves reproduce the known prime-counting values π(x) (25, 1229, 78 498, 664 579, 5 761 455 for x = 10², 10⁴, 10⁶, 10⁷, 10⁸), and Miller–Rabin agrees with the sieve on **every** integer from 0 to two million; the classic traps (561, the strong pseudoprime 3 215 031 751, 2⁶⁴−1) are classified correctly, and the Mersenne prime 2⁶¹−1 and the largest 64-bit prime 2⁶⁴−59 are accepted. The last lines are from the optimized build: counting primes below 10⁹ (π = 50 847 534; a few seconds for either sieve) and testing numbers just above 10¹⁸, where a prime occurs once in about `ln(10¹⁸) ≈ 41` numbers — exactly the density the run finds.

## Beyond this note

- **Factoring**: trial division, Pollard's rho (`O(n^¼)`, practical to ~10¹⁸), the quadratic sieve and the number field sieve (the algorithm behind factoring records). The difficulty of factoring large semiprimes is what RSA's security rests on.
- **Euler's totient** `φ(n)` and the **Möbius function**, computable for all `n ≤ N` in one sieve pass; used in counting problems and Euler's theorem `a^φ(m) ≡ 1`.
- **Discrete logarithm**: baby-step giant-step in `O(√m)`; the hard problem underlying Diffie–Hellman.
- **Montgomery multiplication** and Barrett reduction for fast modular multiplication, and **constant-time** implementations for cryptography (branches and table lookups on secret data leak through timing).
- **Number-theoretic transform (NTT)**: an FFT in modular arithmetic, for exact polynomial and big-integer multiplication in `O(n log n)`.

## Pitfalls

- **Overflow in `a*b % m`** for `m > 2³²` (use `unsigned __int128`, Montgomery, or `fmod`-style tricks); **overflow in `(a % m + m) % m`** for `m > 2⁶²`.
- **C's `%` is the remainder, not the mathematical modulus**: `-7 % 5 == -2`. Normalise with `r < 0 ? r + m : r`.
- **Signed shifts and sizes**: use `1ULL << k` for masks of 32 or more bits.
- **Using the Fermat test** (or a few random Miller–Rabin rounds) where an adversary picks the number: Carmichael numbers and crafted pseudoprimes exist; use the deterministic base set (or at least a strong random test) for untrusted input.
- **Modulus not prime**: Fermat inverses and Fermat's little theorem require a prime modulus; use extended Euclid otherwise.
- **Rolling your own cryptography**: the toy RSA above has no padding and a tiny modulus; never ship it. Use a vetted library (OpenSSL, libsodium) with constant-time implementations.
- **Zero and one**: `gcd(0, 0)`, `powmod` with modulus 1 (everything is 0), and `miller_rabin(1)` are the edge cases that unit tests should cover.

## Key takeaways

1. Euclid's algorithm is `O(log n)`; its extended form gives modular inverses for any coprime modulus and underlies CRT.
2. Modular exponentiation takes `O(log e)` multiplications — but multiplication of 62-bit residues needs a 128-bit intermediate.
3. The Fermat test is fooled by Carmichael numbers; Miller–Rabin with the first twelve prime bases is a *deterministic* primality test for all 64-bit integers at ~0.5 µs per test.
4. Segmented, bit-packed sieves enumerate primes to 10⁹ in seconds with a cache-sized working set.
5. Precomputed factorial and inverse-factorial tables turn binomials modulo a prime into two multiplications.
6. Test number-theoretic code near the limits of the integer type: both bugs in this note appeared only for values close to 2⁶³.
