---
title: "Event-Driven I/O: select, poll, epoll and io_uring"
track: linux
module: Storage and Filesystems
moduleOrder: 5
order: 4
level: expert
summary: How servers handle thousands of connections with one thread — readiness notification (select/poll/epoll, level vs edge triggering) versus completion-based I/O (io_uring), with a working epoll echo server and the pitfalls of each model.
sources:
  - label: "man 7 epoll, man 2 epoll_ctl, man 2 select, man 2 poll, man 7 aio"
    url: https://man7.org/linux/man-pages/man7/epoll.7.html
  - label: "Axboe — Efficient IO with io_uring; man 7 io_uring, liburing documentation"
    url: https://kernel.dk/io_uring.pdf
  - label: "Kegel — The C10K problem"
    url: http://www.kegel.com/c10k.html
  - label: "Kerrisk — The Linux Programming Interface, Ch. 63 Alternative I/O Models"
  - label: "Linux kernel docs — io_uring sysctl (kernel.io_uring_disabled)"
    url: https://docs.kernel.org/admin-guide/sysctl/kernel.html#io-uring-disabled
---

A server with 10,000 connections cannot afford a thread per connection (stacks, scheduler load, lock contention). The solution is **I/O multiplexing**: one thread waits on many descriptors and acts only on those that are ready, so each connection costs a few hundred bytes and a state machine instead of a thread.

## Blocking, non-blocking and multiplexed I/O

- **Blocking**: `read` sleeps until data arrives. Simple, but one thread per concurrent wait.
- **Non-blocking** (`O_NONBLOCK`, `SOCK_NONBLOCK`): calls return immediately with `EAGAIN` / `EWOULDBLOCK` if they'd block. You must not spin; combine with a readiness API.
- **Readiness-based multiplexing** (select/poll/epoll): the kernel tells you *which fds can be read or written without blocking*; you then perform the I/O (the **reactor** pattern: Nginx, Node.js/libuv, Redis, Netty, HAProxy).
- **Completion-based** (POSIX AIO, Linux AIO, **io_uring**): you submit the operation and the kernel tells you when it **finished** (the **proactor** pattern; Windows IOCP is the analog).

## The readiness APIs

| API | Interface | Cost | Limits |
|---|---|---|---|
| **select** | three `fd_set` bitmaps, rebuilt each call | O(n) in highest fd, copies sets | `FD_SETSIZE` = 1024 |
| **poll** | array of `struct pollfd` | O(n) per call (kernel scans all) | no hard limit; array copied each call |
| **epoll** | kernel-side interest list; `epoll_wait` returns only ready fds | **O(1) per ready event** | scales to 100k+ fds |

epoll API:

```c
int ep = epoll_create1(EPOLL_CLOEXEC);
struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);          /* also _MOD, _DEL */
int n = epoll_wait(ep, events, MAXEV, timeout_ms);   /* -1 = wait forever */
```

### Level-triggered vs. edge-triggered

- **Level-triggered (default)**: `epoll_wait` reports an fd as long as it *is* readable. Forgiving: if you read only part of the data, you're notified again.
- **Edge-triggered (`EPOLLET`)**: reports only on a *state change* (new data arrived). You **must** drain the fd until `EAGAIN`, otherwise you'll never be woken for the remaining data. Fewer wakeups; needs non-blocking fds and discipline.
- `EPOLLONESHOT` disables the fd after one event until re-armed (safe handoff to worker threads); `EPOLLEXCLUSIVE` avoids waking all processes sharing a listening fd (thundering herd); `EPOLLRDHUP` signals peer shutdown.

A pitfall: **epoll cannot poll regular files** (they're always "ready"), so it doesn't make disk reads asynchronous. Servers either use thread pools for file I/O, rely on the page cache, or use io_uring.

### A working epoll echo server

```c
int ls = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
/* setsockopt(SO_REUSEADDR); bind(127.0.0.1:port); listen(ls, 128); */
int ep = epoll_create1(EPOLL_CLOEXEC);
struct epoll_event ev = { .events = EPOLLIN, .data.fd = ls };
epoll_ctl(ep, EPOLL_CTL_ADD, ls, &ev);
struct epoll_event evs[64];
for (;;) {
    int n = epoll_wait(ep, evs, 64, -1);
    if (n < 0) { if (errno == EINTR) continue; die("epoll_wait"); }
    for (int i = 0; i < n; i++) {
        int fd = evs[i].data.fd;
        if (fd == ls) {                                      /* new connections: drain the accept queue */
            for (;;) {
                int c = accept4(ls, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (c < 0) break;                            /* EAGAIN: none left */
                struct epoll_event ce = { .events = EPOLLIN | EPOLLET, .data.fd = c };
                epoll_ctl(ep, EPOLL_CTL_ADD, c, &ce);
            }
        } else {
            char buf[4096]; int closed = 0;
            for (;;) {                                       /* edge-triggered: read until EAGAIN */
                ssize_t r = read(fd, buf, sizeof buf);
                if (r > 0) { if (write(fd, buf, r) < 0 && errno != EAGAIN) closed = 1; }
                else if (r == 0) { closed = 1; break; }      /* peer closed */
                else { if (errno != EAGAIN) closed = 1; break; }
            }
            if (closed) close(fd);                           /* closing removes it from the epoll set */
        }
    }
}
```

Tested with 200 concurrent clients: **one thread** (`Threads: 1` in `/proc/PID/status`) served all of them correctly in ~0.1 s. The sketch ignores partial writes (`write` returning less than `r` or `EAGAIN`): a real server buffers unsent data per connection and registers `EPOLLOUT` until it drains, which is the main complexity of reactor programs.

### Practical rules for event loops

1. Never block the loop: no blocking disk or DNS calls, no long computation; offload to worker threads and wake the loop with an `eventfd`.
2. Handle partial reads/writes and `EINTR`/`EAGAIN`; one buffer per connection; apply backpressure (stop reading when output queue is large).
3. Raise limits: `ulimit -n` / `LimitNOFILE=`, `fs.file-max`, `net.core.somaxconn`, `net.ipv4.ip_local_port_range`.
4. Use one loop per core (`SO_REUSEPORT` lets each bind the same port and the kernel load-balances connections) to scale.
5. Timers via `timerfd` or the loop's heap; signals via `signalfd`; child exit via `pidfd`: everything as an fd in the same loop.
6. For high throughput: `sendfile`, `splice`, `MSG_ZEROCOPY`, `TCP_CORK`/`MSG_MORE`, batching `epoll_wait` results.

## Asynchronous disk I/O

| Option | Notes |
|---|---|
| **POSIX AIO** (glibc `aio_read`) | implemented with user-space threads; no real asynchrony |
| **Linux native AIO** (`io_submit`, libaio) | genuinely asynchronous only with **`O_DIRECT`** (bypasses page cache), limited ops, can still block on metadata |
| **io_uring** | general, high-performance, kernel ≥ 5.1; supports buffered I/O, sockets and more |

## io_uring

`io_uring` exposes two **ring buffers shared between user space and the kernel** (mapped with `mmap`):

- **Submission Queue (SQ)**: you write *submission queue entries* (SQE: opcode, fd, buffer, length, offset, `user_data`).
- **Completion Queue (CQ)**: the kernel posts *completion entries* (CQE: `user_data`, result).

A single `io_uring_enter` syscall can submit **many** operations and wait for completions; with `IORING_SETUP_SQPOLL` a kernel thread polls the SQ so submission needs **no syscall at all**. Opcodes cover reads/writes (including vectored and fixed buffers), `accept`, `connect`, `recv`/`send` (and **multishot** variants), `openat`, `close`, `statx`, `fsync`, `poll`, `timeout`, `splice`, `fallocate`, and more, chainable with `IOSQE_IO_LINK` (dependent sequences) and `IOSQE_ASYNC`.

```c
/* liburing: asynchronous read of a file */
struct io_uring ring; io_uring_queue_init(32, &ring, 0);
struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
io_uring_prep_read(sqe, fd, buf, sizeof buf, 0);
io_uring_sqe_set_data(sqe, my_context);
io_uring_submit(&ring);                          /* 1 syscall for any number of queued SQEs */
struct io_uring_cqe *cqe; io_uring_wait_cqe(&ring, &cqe);
/* cqe->res = bytes read or -errno; cqe->user_data = my_context */
io_uring_cqe_seen(&ring, cqe);
```

Optimizations: registered (fixed) files and buffers avoid per-op refcounting and page pinning; provided buffer rings for multishot recv; `IORING_SETUP_COOP_TASKRUN`/`DEFER_TASKRUN`; zero-copy send. Benchmarks routinely reach millions of IOPS per core on NVMe.

Trade-offs and cautions:

- **Security**: its large attack surface produced many kernel vulnerabilities; some platforms restrict it (Android, ChromeOS, many container runtimes via seccomp). Linux 6.6 added `kernel.io_uring_disabled` (0 enabled, 1 only for privileged/group, 2 disabled); check yours (`cat /proc/sys/kernel/io_uring_disabled`).
- Programming model is more complex (ownership of buffers until completion, cancellation, ordering, error handling); use libraries (liburing, Tokio-uring, Glommio, Seastar, libxev) or frameworks that support it (RocksDB, QEMU, Nginx experiments).
- Gains are largest for storage-heavy and syscall-heavy workloads; for network servers epoll + good design is still competitive.

## Choosing

| Situation | Choice |
|---|---|
| Few connections, simple tool | blocking I/O or threads |
| Many connections, mostly network | epoll reactor (libuv, libevent, tokio/mio) |
| Disk-bound with parallel requests | io_uring (or thread pool + `pread`), `O_DIRECT` for databases |
| Portability needed (BSD/macOS/Windows) | libuv/libevent abstract kqueue/epoll/IOCP |
| Strong sandboxing | avoid io_uring or restrict with seccomp |

Debugging: `strace -f -e trace=epoll_wait,epoll_ctl` (event flow), `ss -tnlp` (backlog `Recv-Q`/`Send-Q` of listeners), `perf trace`, `bpftrace -e 'tracepoint:io_uring:*'`; symptoms like a busy loop pegging a core usually mean forgetting to drain an edge-triggered fd or re-arming a level-triggered `EPOLLOUT` with nothing to write.

## Key takeaways

- Multiplexing lets one thread serve thousands of connections: select (1024-fd limit, O(n)), poll (O(n)), epoll (O(ready)).
- Level-triggered is forgiving; edge-triggered requires non-blocking fds and reading until `EAGAIN`.
- epoll can't make regular-file I/O asynchronous; thread pools or io_uring do.
- io_uring uses shared SQ/CQ rings to batch (or eliminate) syscalls and covers files, sockets and more, at the cost of complexity and a security-sensitive surface.
- Never block the loop; handle partial writes; scale with one loop per core and `SO_REUSEPORT`.
