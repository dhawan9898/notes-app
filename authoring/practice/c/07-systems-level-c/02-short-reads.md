---
kind: question
title: Short reads
module: Systems-Level C
difficulty: Hard
---

`read(fd, buf, 64)` on a pipe may return 3. Write `read_all(fd, buf, cap)` that keeps reading until end of file or the buffer is full, retrying on `EINTR`. Demonstrate it with a child process that writes `abc`, pauses, then writes `defgh`.

## Explanation

```c
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* read() may return fewer bytes than asked (pipes, terminals, sockets) and may be interrupted: loop until the data ends. */
static long read_all(int fd, unsigned char *buf, size_t cap) {
    size_t total = 0;
    while (total < cap) {
        ssize_t n = read(fd, buf + total, cap - total);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;                                   /* end of file: the writer closed its end */
        total += (size_t)n;
    }
    return (long)total;
}

int main(void) {
    int fds[2];
    if (pipe(fds) != 0) return 1;
    pid_t pid = fork();
    if (pid == 0) {                                          /* child: a slow writer */
        close(fds[0]);
        if (write(fds[1], "abc", 3) != 3) _exit(1);
        usleep(200000);                                      /* the reader will see "abc" first, on its own */
        if (write(fds[1], "defgh", 5) != 5) _exit(1);
        _exit(0);
    }
    close(fds[1]);
    unsigned char buf[64];
    ssize_t first = read(fds[0], buf, sizeof buf);           /* asks for 64, gets what has arrived */
    printf("a single read() asked for %zu bytes and returned %zd\n", sizeof buf, first);
    long rest = read_all(fds[0], buf, sizeof buf);
    printf("read_all() collected %ld more bytes before end of file\n", rest);
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return 0;
}
```

```text
$ gcc -std=gnu17 -Wall -Wextra -fsanitize=address,undefined -o p7_readall p7_readall.c && ./p7_readall; rm -f p7_readall
a single read() asked for 64 bytes and returned 3
read_all() collected 5 more bytes before end of file
```

The first `read` asked for 64 bytes and got only the 3 that had arrived: a **short read**, which is normal for pipes, sockets and terminals. `read_all` loops: `n > 0` advances the buffer, `n == 0` means the writer closed its end (end of file), and `n < 0` with `errno == EINTR` means a signal interrupted the call, so retry. Any other negative result is a real error. The same discipline applies to `write`, which may also write less than requested.
