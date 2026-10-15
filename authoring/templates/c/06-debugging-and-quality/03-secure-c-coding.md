---
title: Secure C Coding
track: c
module: Debugging and Quality
moduleOrder: 6
order: 3
level: expert
summary: The vulnerability classes that dominate C code and the concrete defenses against each — input validation, safe arithmetic and buffers, command and path injection (with demos), TOCTOU races, privilege dropping, randomness and constant-time comparison, plus the process and tooling around them.
prerequisites:
  - Strings and Buffer Safety
  - Build Systems, Compiler Flags and Hardening Options
  - Static Analysis, Fuzzing and Testing C Code
sources:
  - label: SEI CERT C Coding Standard
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: MITRE CWE Top 25 Most Dangerous Software Weaknesses; CWE-787, CWE-125, CWE-416, CWE-190, CWE-78, CWE-22, CWE-367
    url: https://cwe.mitre.org/top25/
  - label: ISO/IEC TS 17961 C Secure Coding Rules; MISRA C:2012/2023; OpenSSF Secure Coding Guide for C and C++
    url: https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
  - label: "Seacord — Effective C (No Starch Press, 2020); Secure Coding in C and C++ (Addison-Wesley)"
  - label: man 2 openat2 (RESOLVE_BENEATH), man 3 getrandom, man 3 explicit_bzero, man 7 landlock
    url: https://man7.org/linux/man-pages/man2/openat2.2.html
  - label: Microsoft Security Response Center / Google Chromium — ~70% of vulnerabilities are memory safety issues
    url: https://www.chromium.org/Home/chromium-security/memory-safety/
---

Studies of large C and C++ code bases (Chromium, Android, Windows, Linux) consistently attribute roughly **70 % of serious security vulnerabilities to memory-safety errors**. C gives no safety net; secure C is a discipline of *making invalid states unrepresentable, validating at boundaries and failing closed*. This note collects the patterns that matter most.

## The usual suspects (CWE)

| Class | CWE | Typical cause | Primary defenses |
|---|---|---|---|
| Out-of-bounds write / read | 787 / 125 | unchecked lengths, off-by-one, `strcpy`, signed/unsigned confusion | bounds checks on every copy, length-tracking APIs, ASan/fuzzing, `_FORTIFY_SOURCE` |
| Use after free / double free | 416 / 415 | ownership confusion, dangling pointers | single owner, null after free, arenas, ASan |
| Integer overflow / wraparound | 190 | size arithmetic before `malloc`/index | checked arithmetic, `calloc`, `size_t` discipline |
| NULL dereference | 476 | unchecked returns | check every fallible call |
| Uninitialized memory use / leak | 457 / 200 | stack/heap garbage, padding | initialize, `calloc`, zero structs before export |
| Format string | 134 | user data as format | constant format strings, `-Wformat-security` |
| OS command injection | 78 | `system()`/`popen()` with user data | `execve`-style APIs, allow-lists |
| Path traversal | 22 | `../`, symlinks | canonicalize + containment, `openat2(RESOLVE_BENEATH)` |
| Race conditions (TOCTOU) | 362/367 | check then use | operate on file descriptors, atomic open flags |
| Hard-coded/weak secrets, weak RNG | 798/338 | `rand()`, constants | `getrandom`, secret stores |
| Privilege errors | 250/269 | running as root unnecessarily | drop privileges early, least privilege, sandboxing |
| Timing side channels | 208 | early-exit comparisons | constant-time primitives |

## 1. Treat all input as hostile, at the boundary

- Identify **trust boundaries**: network, files, environment variables, command-line arguments, IPC, device data, even other processes' shared memory (and the *kernel's* view of user pointers).
- Define a strict input grammar and validate **length, range, format, and encoding** before use; reject, don't "sanitize". Prefer allow-lists to deny-lists.
- Parse with explicit bounds: every `(pointer, length)` pair should be checked against the actual remaining size before it is read. The fuzzed parser in the testing note shows how one missing comparison (`len` vs. destination size) becomes a stack overflow.
- **Length-prefixed fields**: check `len <= remaining` first using subtraction (`len > size - pos`), never `pos + len > size` (overflow).
- Limit resource use: maximum sizes, nesting depth (recursion → stack overflow), counts (hash-flooding DoS), time.
- Canonicalize once, then validate the canonical form, and use that form for the operation (otherwise attackers use equivalent encodings).
- Don't trust `Content-Length`, `st_size`, or numbers in the file for allocation without caps.

## 2. Arithmetic and allocation

```c
/* WRONG */  buf = malloc(count * sizeof *buf);                   /* count * size may wrap → small buffer, large copy */
/* RIGHT */  buf = calloc(count, sizeof *buf);                    /* or reallocarray(NULL, count, sizeof *buf) */
/* RIGHT */  if (len > size - pos) return ERR_TRUNCATED;          /* subtraction form cannot overflow when pos <= size */
```

Use `size_t` for sizes, check conversions between signed and unsigned (`-Wsign-conversion`), use `__builtin_mul_overflow`/`ckd_mul`, avoid `int` for lengths, and watch `uint8_t`/`uint16_t` promotions (types note). Cap attacker-controlled sizes before multiplying. Zero memory that may leak (`calloc`, `memset` before copying structs across a trust boundary).

## 3. Buffers and strings

Follow the strings note: length-tracking buffers, `snprintf`, no `gets/strcpy/strcat/sprintf`, bounded `memcpy` with verified sizes, explicit NUL handling, constant format strings. Never write `buf[len]` without proving `len < sizeof buf`. Prefer APIs that take a destination size. Consider fat-pointer helper types (`struct slice { uint8_t *p; size_t n; }`) with checked accessors; the macros are tedious, but they localize bounds checks.

## 4. Command injection: never build shell command lines from data

`system()`, `popen()`, and `sh -c` interpret metacharacters (`; | & $ ( ) < > \` newline`). Compare:

{{include c/secure/inject.c}}

{{run c/secure :: gcc -Wall -Wextra inject.c -o inject && ./inject 2>&1}}

With `system`, the attacker's `; echo INJECTED-COMMAND-RAN` became a **second command that executed**. With `execlp`/`execve`, the same string was just a (nonexistent) file name. Rules:

- Avoid running programs at all when a library call or syscall exists.
- Otherwise `fork` + `execve`/`posix_spawn` with an argv array; use `--` to end option parsing so values like `-rf` aren't read as options (option injection); use absolute program paths and a minimal, explicit environment (`execve` with a sanitized `envp`; never trust `PATH`, `IFS`, `LD_PRELOAD`, `LD_LIBRARY_PATH` when privileged: setuid programs ignore the `LD_*` ones).
- If a shell is unavoidable, quote with a vetted routine or pass data via environment variables / positional parameters (`sh -c 'cmd "$1"' sh "$value"`), and validate with an allow-list.
- The same pattern applies to SQL (parameterized queries), LDAP, XML/XPath, log lines (CRLF injection), and format strings.

## 5. Paths and files

### Path traversal

User-supplied file names must not escape the intended directory. A string check for `..` is not enough: absolute paths, symlinks, and alternate encodings bypass it. Resolve the path and verify containment:

{{include c/secure/paths.c}}

{{run c/secure :: gcc -Wall -Wextra paths.c -o paths && ./paths}}

The naive `".."` test **allowed** `escape/passwd`, a path that goes through a symlink inside the base directory pointing at `/etc` and so reads `/etc/passwd`. The `realpath`-and-prefix check denied it, including the sibling-directory trap (`/tmp/secure_demo_root2` shares the prefix string of `/tmp/secure_demo_root`, hence the check that the next character is `/` or the end).

`realpath` followed by `open` still has a race (below). The race-free tools on modern Linux are **`openat2(dirfd, path, &how{ .resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS })`** (kernel ≥ 5.6) and `O_NOFOLLOW`/`openat` relative to a trusted directory fd, which make the kernel refuse escapes atomically. Also consider chroot/namespaces/Landlock for defense in depth.

### TOCTOU (time-of-check to time-of-use)

```c
if (access(path, W_OK) == 0) {       /* check */
    fd = open(path, O_WRONLY);       /* use: an attacker can swap `path` for a symlink in between */
}
```

The file system can change between the two calls. Fix by making the check and the use **one atomic operation on one object**: just `open()` and handle `EACCES`; use `O_NOFOLLOW`, `O_EXCL | O_CREAT` for creation, `openat`, and then `fstat(fd, ...)` on the *descriptor* to examine what you actually opened. For temporary files use `mkstemp()` (random name, `O_EXCL`, mode `0600`), never `tmpnam`/`mktemp`/predictable names in `/tmp`. Set `umask` deliberately (`umask(077)` for private files), and create files with explicit modes. Use `renameat2(RENAME_NOREPLACE)` and write-temp-then-rename for atomic replacement (permissions!).

Other race classes: signal handlers (async-signal-safety), `fork`+threads, shared memory, and `chdir`/relative path assumptions. Prefer descriptors over paths and PIDs over names; use `pidfd_open` instead of PID reuse-prone `kill`.

## 6. Privileges

1. **Don't run as root** unless required; use capabilities or a dedicated user.
2. Do privileged setup (bind port < 1024, open files), then **drop privileges permanently** and in the right order: `setgroups(0, NULL)` (clear supplementary groups), `setresgid(g, g, g)`, then `setresuid(u, u, u)`; verify it cannot be regained (`setuid(0)` must fail) and that `getuid/geteuid` are what you expect. Check every return value: a failed drop that continues running as root is a classic vulnerability (CWE-273).
3. `PR_SET_NO_NEW_PRIVS`, `seccomp` filters, `Landlock` (self-restriction of filesystem/network access), namespaces, and systemd sandboxing shrink what an exploited process can do.
4. Privilege separation (OpenSSH model): split a daemon into a small privileged monitor and a sandboxed unprivileged worker that handles untrusted input; communicate over a narrow socket protocol.
5. Setuid binaries: sanitize environment, clear dangerous variables, use absolute paths, minimize code, avoid `system`; keep them rare.

## 7. Randomness, secrets and comparison

- **Never use `rand()`/`random()`/`time(NULL)` seeds for security.** A deterministic generator seeded with a guessable value is reproducible by an attacker:

{{include c/secure/rng.c}}

{{run c/secure :: gcc -Wall -Wextra rng.c -o rng && ./rng}}

  Both runs of `rand()` produced identical numbers. Use `getrandom(2)` (or `/dev/urandom`, `arc4random_buf` on BSD/glibc ≥ 2.36, `BCryptGenRandom` on Windows) for keys, tokens, nonces, salts, and session IDs. Check return values (short reads).
- **Compare secrets in constant time.** `memcmp`/`strcmp` return at the first difference, leaking how many leading bytes matched through timing, which allows byte-by-byte guessing of MACs/tokens over a network. Use a constant-time routine (libsodium `sodium_memcmp`, OpenSSL `CRYPTO_memcmp`, `timingsafe_bcmp`):

{{include c/secure/ctcmp.c}}

{{run c/secure :: gcc -Wall -Wextra ctcmp.c -o ctcmp && ./ctcmp}}

  (Compilers may "optimize" naive constant-time code; real libraries use `volatile` reads, assembly or compiler barriers, and are audited. Don't write your own crypto.)
- **Wipe secrets**: `explicit_bzero`/`memset_explicit`/`OPENSSL_cleanse` (plain `memset` before `free` is a dead store the optimizer may delete). Use `mlock` for key material to avoid swapping, `MADV_DONTDUMP` to keep it out of core dumps, and don't put secrets in `argv`, environment, logs, URLs or exception messages.
- **Cryptography**: use vetted libraries (libsodium, OpenSSL/BoringSSL, mbedTLS) with high-level APIs (AEAD like ChaCha20-Poly1305/AES-GCM); never ECB, unauthenticated CBC, home-made MACs, MD5/SHA-1 for security, or reused nonces. Verify TLS certificates and hostnames, validate signatures before parsing the signed content, and prefer password hashing with Argon2id/scrypt/bcrypt.

## 8. Memory management discipline

- One clear **owner** per allocation; document borrowing; free in the same layer that allocates; null the pointer after free when it persists.
- Prefer **arena/region** allocation for request-scoped data (one free instead of hundreds), eliminating a class of leaks and use-after-free in error paths.
- Never return pointers to locals; avoid `alloca`/VLAs; check `realloc` results through a temporary.
- Initialize everything (`= {0}`; C23 `{}`; `-ftrivial-auto-var-init=zero`), and zero padding of structs sent over trust boundaries.
- Defensive APIs: `reallocarray`, `strlcpy`, `snprintf`, `memcpy_s`-style wrappers that take both sizes.
- Hardened allocators (`scudo`, `hardened_malloc`, glibc `GLIBC_TUNABLES=glibc.malloc.check`) and `MALLOC_PERTURB_` in tests.

## 9. Concurrency and signals

Avoid data races (undefined behavior, exploitable), use atomics/mutexes, keep signal handlers minimal and async-signal-safe, don't use non-reentrant functions (`strtok`, `localtime`, `getpwnam`) in threads, and beware of `fork` in multithreaded processes. Time-of-check problems appear in shared state as well (checks of a flag then use of the data it guards).

## 10. Defense in depth and tooling

No single defense suffices; assume bugs exist and make exploitation hard and damage small:

1. **Compile hardened** (PIE, full RELRO, canary, FORTIFY, CET, `-ftrivial-auto-var-init`); see the build note.
2. **Sanitize and fuzz** every parser; static analysis in CI; strict warnings.
3. **Sandbox**: seccomp-bpf filters (allow-list syscalls), Landlock, namespaces, containers with dropped capabilities, separate processes for risky parsers (browsers, `ffmpeg` in a sandbox).
4. **Minimize attack surface**: delete unused code/features, don't link what you don't use, disable dangerous protocol features.
5. **Review**: checklist (see below), threat model before coding, code review focused on boundaries and ownership, `git blame` for old, risky code.
6. **Patch quickly**: track dependencies (SBOM, `OSV-scanner`, `Dependabot`), have an update path and vulnerability disclosure policy (`SECURITY.md`).
7. **Prefer memory-safe languages for new components** (Rust, Go) at risky boundaries, and keep C for the parts that need it behind small, well-fuzzed interfaces (Rust-in-kernel, `rustls`, `sudo-rs`, `ripgrep`).

### Code-review checklist for C

- Every `malloc`/`realloc`/`calloc` result and size computation checked? Every `free` once on every path?
- Every array index/pointer arithmetic proven in range? Every length from input validated with overflow-safe arithmetic?
- Any `strcpy`/`sprintf`/`gets`/`system`/`rand`/`strtok`/`atoi`? `printf` with non-constant format?
- Are errors from I/O, `close`, `fclose`, and syscalls handled? `EINTR`, short reads/writes?
- Signed/unsigned and narrowing conversions explicit? Shifts in range?
- Uninitialized variables/padding? Locals escaping scope?
- Paths as descriptors, no TOCTOU? Temporary files via `mkstemp`?
- Threads: shared data protected? Lock ordering?
- Secrets: random source, constant-time compare, wiped, not logged?
- Privileges dropped and verified? Least capability?

## Key takeaways

- Most C vulnerabilities are memory-safety bugs from missing bounds checks, overflow in size arithmetic, and lifetime errors; design to make them impossible or detectable.
- Validate at trust boundaries with overflow-safe checks; never build shell commands or paths from data (use `execve`, canonicalize, `openat2`).
- Remove races by operating on file descriptors; drop privileges correctly and verify; use `getrandom`, constant-time comparison, `explicit_bzero`.
- Layer mitigations (hardened builds, sanitizers, fuzzing, sandboxing) and consider memory-safe languages for new high-risk components.
