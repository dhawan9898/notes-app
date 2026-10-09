---
title: "File I/O in C: stdio vs POSIX, Buffering, Short Reads and Durability"
track: c
module: Systems-Level C
moduleOrder: 7
order: 2
level: intermediate
summary: The two I/O layers (FILE* streams and file descriptors), how buffering changes syscall counts by 500×, robust read/write loops for short transfers and EINTR, atomic file replacement, mmap and directory traversal, with measured numbers.
sources:
  - label: "ISO/IEC 9899:2018 (C17) §7.21 Input/output <stdio.h>"
  - label: "POSIX.1-2017 — open(), read(), write(), pread(), close(), fsync(), rename(), opendir(), stat(), mmap()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "man 3 setvbuf, man 2 write, man 2 read, man 2 open, man 7 pipe, man 2 openat"
    url: https://man7.org/linux/man-pages/man3/setvbuf.3.html
  - label: "Stevens & Rago — Advanced Programming in the UNIX Environment, 3rd ed., Ch. 3 and 5"
  - label: "Kerrisk — The Linux Programming Interface, Ch. 4, 13, 14"
---

C offers two I/O interfaces. **stdio** (`FILE *`, `fopen`, `fprintf`, `fgets`, `fread`) is portable ISO C with user-space buffering and formatting. **POSIX file descriptors** (`open`, `read`, `write`, `lseek`, `close`) are thin wrappers over system calls: unbuffered, byte-oriented, and the interface for everything that isn't a regular file (sockets, pipes, devices, `epoll`). Knowing which layer you're on explains most I/O surprises.

| | stdio (`FILE *`) | POSIX (`int fd`) |
|---|---|---|
| Buffering | user-space buffer (usually 4 KiB–64 KiB) | none; each call is a syscall |
| Formatting | `printf`, `scanf`, `fgets` | none: raw bytes |
| Portability | all C implementations | POSIX systems (Windows has `_open` variants) |
| Control | limited (`fflush`, `setvbuf`, `fseek`) | `O_*` flags, `fcntl`, `ioctl`, `pread/pwrite`, `O_DIRECT`, `O_CLOEXEC`, `dup2`, non-blocking, `mmap`, `sendfile` |
| Errors | `ferror`, `feof`, `errno` | return value `-1` + `errno` |
| Thread safety | each call locks the stream | atomic per syscall |
| Typical use | text, config files, simple programs | servers, system tools, performance-sensitive I/O |

Mixing both on the same file is possible (`fileno(f)`, `fdopen(fd, "r")`) but dangerous: the stdio buffer and the descriptor's file offset go out of sync unless you `fflush` first.

## Buffering modes

stdio chooses a mode at the first I/O on a stream:

- **Fully buffered** (regular files, pipes): data is collected until the buffer fills, then `write`n in a block.
- **Line buffered** (terminals): flushed at each newline (or when input is requested from the terminal).
- **Unbuffered** (`stderr` by default): every call becomes a `write`.

Change it with `setvbuf(f, buf, mode, size)` (before any other operation on the stream) or `setbuf`. `fflush(f)` forces output; `fflush(NULL)` flushes all; `exit()` and normal `return` from `main` flush; `_exit`, `abort`, a crash or `kill -9` **lose** buffered data. Reading from a terminal flushes line-buffered stdout (so prompts without a newline appear, `fflush(stdout)` otherwise).

The cost is dramatic. Ten thousand 7-byte records, counting `write` system calls through `/proc/self/io` (`syscw`):

```c
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* /proc/self/io counts this process's system calls: syscw = number of write-like syscalls so far. */
static long syscw(void) {
    FILE *f = fopen("/proc/self/io", "r");
    char line[128]; long v = -1;
    while (f && fgets(line, sizeof line, f)) if (sscanf(line, "syscw: %ld", &v) == 1) break;
    if (f) fclose(f);
    return v;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "full";
    FILE *f = fopen("/tmp/bufmodes.out", "w");
    if (!f) return 1;
    if (!strcmp(mode, "none")) setvbuf(f, NULL, _IONBF, 0);          /* unbuffered: every call is a syscall */
    else if (!strcmp(mode, "line")) setvbuf(f, NULL, _IOLBF, 4096);   /* flush at each newline */
    else if (!strcmp(mode, "full")) setvbuf(f, NULL, _IOFBF, 65536);  /* flush when 64 KiB accumulate */
    long before = syscw();
    if (!strcmp(mode, "raw")) {                                       /* bypass stdio: one write() per record */
        int fd = fileno(f);
        for (int i = 0; i < 10000; i++) write(fd, "record\n", 7);
    } else {
        for (int i = 0; i < 10000; i++) fputs("record\n", f);
    }
    fflush(f);
    long after = syscw();
    printf("%-5s: 10000 records of 7 bytes -> %5ld write syscalls\n", mode, after - before - 0);
    fclose(f);
    return 0;
}
```

```text
$ gcc -O1 -Wall -Wextra bufmodes.c -o bufmodes && for m in none line full raw; do ./bufmodes $m; done; rm -f /tmp/bufmodes.out
none : 10000 records of 7 bytes -> 10000 write syscalls
line : 10000 records of 7 bytes -> 10000 write syscalls
full : 10000 records of 7 bytes ->    18 write syscalls
raw  : 10000 records of 7 bytes -> 10000 write syscalls
```

Fully buffered stdio turned 10,000 `fputs` into **18** syscalls (glibc writes in block-sized chunks), while unbuffered, line-buffered (every record ended in `\n`) and raw `write()` each did 10,000. At roughly 0.4 µs of syscall overhead (plus kernel file-system work) per call, that is the difference between milliseconds and the better part of a second at scale.

Related pitfalls:

- **Output ordering between stdout and stderr** differs when stdout is a pipe/file (fully buffered) and stderr isn't: logs appear "out of order". Set `setvbuf(stdout, NULL, _IOLBF, 0)` when interleaving matters.
- **Crash = lost output**: buffered `printf` debugging lines before a segfault never appear; use `fprintf(stderr, ...)` or flush.
- **`fork()` duplicates unflushed buffers**, so output may be printed twice; flush before forking.
- A `FILE *` write only moves data to the stdio buffer, then `write` moves it to the kernel page cache, and `fsync` finally to storage (three levels; see the filesystem durability note). `fclose` can fail (e.g. `ENOSPC` when flushing): check it.

## Robust reading and writing with file descriptors

`read(fd, buf, n)` returns the number of bytes actually read (possibly fewer than `n`), `0` at EOF, `-1` on error. `write` likewise may transfer **fewer bytes than requested** ("short write") on pipes, sockets, terminals, signals, and full disks. Neither loops for you, and either can fail with `EINTR` if a signal handler interrupted the call (unless `SA_RESTART`). Real code wraps them:

```c
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* write(2) may write fewer bytes than asked (pipes, sockets, signals, disk full): loop until done. */
static int write_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; n -= (size_t)w;
    }
    return 0;
}

/* read(2) may return fewer bytes too; 0 means EOF. Returns bytes read (== n unless EOF) or -1. */
static ssize_t read_full(int fd, void *buf, size_t n) {
    char *p = buf; size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;
        got += (size_t)r;
    }
    return (ssize_t)got;
}

/* Atomically replace a file: write temp, fsync, rename, fsync the directory. */
static int write_file_atomic(const char *dir, const char *name, const void *data, size_t n) {
    char tmp[256], dst[256];
    snprintf(tmp, sizeof tmp, "%s/.%s.XXXXXX", dir, name);
    snprintf(dst, sizeof dst, "%s/%s", dir, name);
    int fd = mkstemp(tmp);                                         /* O_EXCL, mode 0600, unique name */
    if (fd < 0) return -1;
    int rc = -1;
    if (write_all(fd, data, n) == 0 && fsync(fd) == 0) rc = 0;
    if (close(fd) != 0) rc = -1;                                    /* close can report deferred write errors */
    if (rc == 0 && rename(tmp, dst) != 0) rc = -1;
    if (rc != 0) { unlink(tmp); return -1; }
    int dfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) { fsync(dfd); close(dfd); }                       /* make the rename durable */
    return 0;
}

int main(void) {
    /* demonstrate a SHORT WRITE: a non-blocking pipe accepts only what fits in its buffer */
    int p[2];
    if (pipe2(p, O_NONBLOCK | O_CLOEXEC)) return 1;
    static char big[1 << 20];
    memset(big, 'x', sizeof big);
    ssize_t w = write(p[1], big, sizeof big);
    printf("write(pipe, 1 MiB) returned %zd (short write: the pipe buffer is 64 KiB)\n", w);
    ssize_t w2 = write(p[1], big, 10);
    printf("second write on the full pipe: %zd, errno=%s\n", w2, w2 < 0 ? strerror(errno) : "-");
    close(p[0]); close(p[1]);

    const char msg[] = "config v2\n";
    if (write_file_atomic("/tmp", "demo_config.txt", msg, sizeof msg - 1) == 0) {
        char back[64] = {0};
        int fd = open("/tmp/demo_config.txt", O_RDONLY | O_CLOEXEC);
        ssize_t r = read_full(fd, back, sizeof back - 1);
        close(fd);
        printf("atomic write ok, read back %zd bytes: %s", r, back);
        struct stat st; stat("/tmp/demo_config.txt", &st);
        printf("mode=%o size=%lld\n", st.st_mode & 0777, (long long)st.st_size);
        unlink("/tmp/demo_config.txt");
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra rw_all.c -o rw_all && ./rw_all
write(pipe, 1 MiB) returned 65536 (short write: the pipe buffer is 64 KiB)
second write on the full pipe: -1, errno=Resource temporarily unavailable
atomic write ok, read back 10 bytes: config v2
mode=600 size=10
```

- The non-blocking pipe accepted only **65,536** of 1,048,576 bytes (its buffer capacity); the next write failed with `EAGAIN`. A `write(fd, buf, n)` whose return value is ignored silently drops data. In event loops, buffer the remainder and wait for writability (`EPOLLOUT`).
- `read_full` loops until it has `n` bytes or EOF; for text protocols you may instead read into a buffer and scan for delimiters (framing).
- **Regular files** rarely short-read except at EOF or on signals/NFS, but code must not assume it.
- `O_CLOEXEC` (always set it, or use `fcntl(FD_CLOEXEC)`) prevents leaking descriptors into `exec`'d children; `O_NONBLOCK`, `O_APPEND` (atomic append: each `write` positions at the end), `O_EXCL | O_CREAT` (atomic create-if-absent), `O_NOFOLLOW`, `O_DIRECTORY`, `O_TMPFILE` (anonymous temporary file), `O_SYNC/O_DSYNC`, `O_DIRECT`.
- `pread(fd, buf, n, offset)` / `pwrite` read/write at an offset **without** touching the shared file offset: thread-safe random access.
- `readv`/`writev` (scatter/gather) avoid copying headers and payload into one buffer; `sendfile`, `splice`, `copy_file_range` move data between descriptors in the kernel.

### Atomic replacement and durability

`write_file_atomic` implements the safe update pattern: create a unique temp file in the **same directory** with `mkstemp` (mode `0600`, `O_EXCL`), write everything, `fsync`, check `close`, `rename` over the target (atomic on POSIX filesystems: readers see the old or the new file, never a partial one), and `fsync` the directory so the rename itself survives a crash. The run printed `mode=600`: `mkstemp`'s private mode survives the rename: set the final permissions explicitly (`fchmod`) if the target should differ. Cost: an `fsync` can take milliseconds (measured at 5.5 ms on a virtual disk in the filesystem note); use it for state that must not be lost, not for every log line.

## Reading speed: syscalls dominate

Counting newlines in a 15.5 MiB cached file with five strategies (`readbench.c`):

```c
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define PATH "/tmp/readbench.dat"
static long count_nl_bytes_read(void) {           /* read() one byte at a time: one syscall per byte */
    int fd = open(PATH, O_RDONLY); char c; long n = 0;
    while (read(fd, &c, 1) == 1) n += c == '\n';
    close(fd); return n;
}
static long count_nl_stdio(void) {                /* getc: buffered, takes a lock per call */
    FILE *f = fopen(PATH, "r"); int c; long n = 0;
    while ((c = getc(f)) != EOF) n += c == '\n';
    fclose(f); return n;
}
static long count_nl_unlocked(void) {             /* getc_unlocked: buffered, no locking */
    FILE *f = fopen(PATH, "r"); int c; long n = 0;
    while ((c = getc_unlocked(f)) != EOF) n += c == '\n';
    fclose(f); return n;
}
static long count_nl_block(void) {                /* read() in 64 KiB blocks */
    int fd = open(PATH, O_RDONLY); static char buf[1 << 16]; long n = 0; ssize_t r;
    while ((r = read(fd, buf, sizeof buf)) > 0) for (ssize_t i = 0; i < r; i++) n += buf[i] == '\n';
    close(fd); return n;
}
static long count_nl_mmap(void) {                 /* mmap the whole file */
    int fd = open(PATH, O_RDONLY); struct stat st; fstat(fd, &st);
    const char *p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0); long n = 0;
    for (off_t i = 0; i < st.st_size; i++) n += p[i] == '\n';
    munmap((void *)p, (size_t)st.st_size); close(fd); return n;
}
int main(void) {
    FILE *f = fopen(PATH, "w");                    /* 8 MiB of text lines */
    for (int i = 0; i < 400000; i++) fprintf(f, "line number %d of the benchmark file\n", i);
    fclose(f);
    struct { const char *name; long (*fn)(void); } t[] = {
        {"read() 1 byte at a time", count_nl_bytes_read}, {"getc (stdio, locked)", count_nl_stdio},
        {"getc_unlocked", count_nl_unlocked}, {"read() 64 KiB blocks", count_nl_block}, {"mmap", count_nl_mmap} };
    struct stat st; stat(PATH, &st);
    printf("file size: %.1f MiB (page cache warm)\n", st.st_size / 1048576.0);
    for (size_t k = 0; k < 5; k++) { double t0 = now(); long n = t[k].fn(); printf("%-26s %8.1f ms   lines=%ld\n", t[k].name, (now() - t0) * 1e3, n); }
    unlink(PATH);
    return 0;
}
```

```text
$ gcc -O2 readbench.c -o readbench && ./readbench
file size: 15.5 MiB (page cache warm)
read() 1 byte at a time      6279.9 ms   lines=400000
getc (stdio, locked)           42.3 ms   lines=400000
getc_unlocked                  14.5 ms   lines=400000
read() 64 KiB blocks           11.8 ms   lines=400000
mmap                           10.1 ms   lines=400000
```

| Method | Why it performs this way |
|---|---|
| `read(fd, &c, 1)` per byte | one syscall per byte: **~600× slower** than blocks (16 million system calls) |
| `getc` | buffered; ~4× slower than the best because every call locks/unlocks the `FILE` |
| `getc_unlocked` | buffered without locking; near block speed (use only when one thread owns the stream) |
| `read` in 64 KiB blocks | few syscalls, then a tight loop over memory; the practical default |
| `mmap` | no copy from the page cache into a user buffer; best for random access or repeated passes; page faults on first touch (`MAP_POPULATE` pre-faults); errors appear as `SIGBUS` if the file shrinks |

Guidelines: read big blocks (64 KiB–1 MiB), process in memory; use `fgets`/`getline` for text lines; avoid per-byte syscalls and per-character `fgetc` in hot loops; use `posix_fadvise(POSIX_FADV_SEQUENTIAL/WILLNEED/DONTNEED)` and `readahead` hints for large scans, `O_DIRECT` + aligned buffers only for database-style caches.

## Text vs. binary, seeking and `ftell`

- On POSIX, text and binary streams behave identically (`"r"` ≡ `"rb"`); on Windows, text mode translates `\r\n`. Open binary data with `"rb"`/`"wb"` for portability.
- `fseek`/`ftell` use `long` (32-bit on LLP64/32-bit systems): use `fseeko`/`ftello` with `off_t` and `-D_FILE_OFFSET_BITS=64` for files > 2 GiB. `fseek` on a pipe or terminal fails.
- Switching between reading and writing on an update stream (`"r+"`) requires an intervening `fflush`/`fseek`/`rewind`.
- `fgets` stops at newline or size−1 and keeps the newline; check for truncation (no `\n` and not EOF); `getline` allocates for you. Never use `gets`.
- `fread(buf, size, count, f)` returns the item count; for byte counts use `size = 1`. `feof(f)` is true only *after* a read hit EOF, so `while (!feof(f))` loops one time too many: test the return value of the read instead.
- `scanf` family: always check the return value and give `%s` a width; prefer `fgets` + `strtol`/`sscanf`.

## Directories and metadata

```c
#include <dirent.h>
#include <sys/stat.h>

DIR *d = opendir(path);                         /* or fdopendir(fd) for TOCTOU-safe traversal with openat */
for (struct dirent *e; (e = readdir(d)); ) {    /* order is unspecified; includes "." and ".." */
    struct stat st;
    if (fstatat(dirfd(d), e->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(st.st_mode))
        printf("%s %lld\n", e->d_name, (long long)st.st_size);
}
closedir(d);
```

- `stat` follows symlinks, `lstat`/`AT_SYMLINK_NOFOLLOW` doesn't; `st_mode` type tests `S_ISREG/S_ISDIR/S_ISLNK`; times (`st_mtim`) have nanosecond resolution; use `fstat(fd)` to inspect what you actually opened.
- For recursive walks use `nftw`/`fts` (careful with symlink loops and mount points: `FTW_PHYS`, `FTW_MOUNT`) or `openat` recursion.
- `dirent.d_type` is a hint (may be `DT_UNKNOWN`). `scandir` + `alphasort` gives sorted lists. Names are byte strings, not necessarily UTF-8, with no `/` or NUL.
- Deleting while iterating is allowed for the current entry on POSIX; behavior for other entries is unspecified.

## Error handling in I/O code

1. Check **every** return value, including `fclose`, `close`, `fflush`, `fsync`, `rename`, `unlink`.
2. Loop on `EINTR`; treat `EAGAIN/EWOULDBLOCK` as "try later" for non-blocking descriptors; handle `ENOSPC`, `EIO`, `EPIPE` (and ignore `SIGPIPE` in servers).
3. Report with context (`strerror(errno)` plus the path).
4. Close descriptors on every path (`goto cleanup`); beware descriptor leaks (`ulimit -n`, `lsof -p`), double-close races in multi-threaded code (close can return `EINTR`/`EIO` but must not be retried on Linux).
5. Validate sizes from files before allocating; never trust `st_size` for special files (`/proc` reports 0).

## Key takeaways

- stdio buffers in user space (18 syscalls instead of 10,000 for the same output); POSIX descriptors expose each syscall and the full control of the OS. Know the buffering mode and flush before fork, exit via `_exit`, or crashes.
- `read`/`write` can transfer less than requested and fail with `EINTR`/`EAGAIN`: always loop; never ignore return values, including `close`/`fclose`.
- Use big block reads (or `mmap`); byte-at-a-time syscalls were ~600× slower in the measurement.
- Replace files atomically (`mkstemp` → write → `fsync` → `rename` → `fsync` dir) and check `fclose`/`close`; use `O_CLOEXEC`, `pread`, `openat`, and `off_t` with 64-bit offsets.
