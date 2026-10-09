---
title: "Inter-Process Communication: Pipes, FIFOs, Unix Sockets, Shared Memory and Futexes"
track: linux
module: Processes
moduleOrder: 3
order: 4
level: advanced
summary: The Linux IPC toolbox compared — capacities and atomicity of pipes, Unix domain socket features (fd passing, credentials), POSIX vs SysV shared memory, message queues, the *fd family (eventfd, timerfd, pidfd, memfd) and how futexes underpin userspace locks.
sources:
  - label: "man 7 pipe, man 7 fifo, man 7 unix, man 7 shm_overview, man 7 mq_overview, man 7 sem_overview"
    url: https://man7.org/linux/man-pages/man7/pipe.7.html
  - label: "man 2 futex, man 7 futex; Drepper — Futexes Are Tricky"
    url: https://man7.org/linux/man-pages/man2/futex.2.html
  - label: "man 2 eventfd, signalfd, timerfd_create, pidfd_open, memfd_create"
    url: https://man7.org/linux/man-pages/man2/memfd_create.2.html
  - label: "POSIX.1-2017 — pipe(), mkfifo(), shm_open(), mq_open(), sem_open()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "Kerrisk — The Linux Programming Interface, Ch. 43–54"
---

Processes have separate address spaces, so they need explicit channels to cooperate. Each IPC mechanism trades off **speed**, **ordering/atomicity**, **data model** (byte stream, messages, shared memory), **access control** and **ease of cleanup**.

## Overview

| Mechanism | Model | Namespace / name | Typical use |
|---|---|---|---|
| **Pipe** (`pipe`) | unidirectional byte stream | anonymous (inherit via `fork`) | shell pipelines, parent↔child |
| **FIFO** (`mkfifo`) | same, named | filesystem path | unrelated processes, simple logging |
| **Unix domain socket** | stream / datagram / seqpacket, bidirectional | path or abstract name | local services (D-Bus, Docker, X11, nginx↔php-fpm) |
| **Shared memory** | raw memory region | `shm_open` name (`/dev/shm`), SysV key | highest throughput; you synchronize |
| **POSIX message queue** | prioritized messages | `/name` | small structured messages |
| **SysV msg/sem/shm** | legacy | integer key | old software, databases |
| **TCP/UDP loopback** | network sockets | IP:port | portability, remote capability |
| **Signals** | notification | PID | "wake up", not data |
| **eventfd, timerfd, signalfd, pidfd** | fd-based events | file descriptor | event loops |
| **futex** | wait/wake on a memory word | address | the building block of userspace locks |
| **D-Bus, gRPC, etc.** | higher level | | system services |

## Pipes

`pipe(fds)` returns a read end and a write end backed by an in-kernel ring buffer.

- **Capacity**: 16 pages = **65,536 bytes** by default (verified: `fcntl(F_GETPIPE_SZ)` → 65536); adjust with `F_SETPIPE_SZ` up to `/proc/sys/fs/pipe-max-size` (1 MiB). Unprivileged per-user total is limited.
- **Blocking**: `write` blocks when full, `read` blocks when empty. **EOF**: when all write ends are closed, `read` returns 0. If all read ends are closed, `write` raises `SIGPIPE`/`EPIPE`. Hence the classic bug: **a forgotten duplicate of the write end** (inherited by another child) prevents EOF; close unused ends in each process.
- **Atomicity**: writes of at most `PIPE_BUF` (**4096** bytes on Linux; POSIX min 512) are guaranteed not to interleave with other writers; larger writes may be split and interleaved. Multiple writers using records ≤ 4 KiB are safe without locking.
- **Packet mode** (`O_DIRECT` pipes) preserves message boundaries up to PIPE_BUF.
- Zero-copy helpers: `splice`, `tee`, `vmsplice` move pipe pages without copying to user space.

**FIFOs** behave identically but have a name: `mkfifo /tmp/q`; `open()` blocks until the other end opens (use `O_NONBLOCK`/`O_RDWR` to avoid). They have no persistent contents; ownership and permissions follow normal file rules.

## Unix domain sockets

The workhorse for local services; same socket API as TCP without the network stack:

```c
int s = socket(AF_UNIX, SOCK_STREAM, 0);       /* or SOCK_DGRAM, SOCK_SEQPACKET */
struct sockaddr_un a = { .sun_family = AF_UNIX };
strcpy(a.sun_path, "/run/app.sock");           /* abstract: sun_path[0] = '\0', "\0name" */
bind(s, (void*)&a, sizeof a); listen(s, 128);
```

Distinguishing features:

- **Filesystem permissions** on the socket file control who may connect (`chmod 660`, group ownership): simple access control. **Abstract sockets** (name starting with NUL, shown as `@name` in `ss`) have no file, vanish with the process, and are scoped to the **network namespace**.
- **Credential passing**: `SO_PEERCRED` gives the connected peer's PID/UID/GID (unforgeable, taken by the kernel), and `SCM_CREDENTIALS` can pass them per message.
- **File descriptor passing** (`SCM_RIGHTS` in ancillary data of `sendmsg`): hand an open file/socket to another process (privilege separation, zero-downtime restarts via socket activation, `systemd` socket units).
- `SOCK_SEQPACKET` = reliable, ordered, **message-oriented** and connection-based (ideal for RPC).
- Faster than loopback TCP (no IP/TCP processing, no checksums): typically 2× the throughput and lower latency.
- Inspect: `ss -xlp` (listening Unix sockets), `lsof -U`, `/proc/net/unix`.

## Shared memory

The fastest IPC: processes map the **same physical pages** into their address spaces; no copy, no syscall per access. The price: **you** must provide synchronization and a data format.

### POSIX shared memory (preferred)

```c
int fd = shm_open("/myseg", O_CREAT | O_RDWR, 0600);    /* object appears in /dev/shm/myseg */
ftruncate(fd, 1 << 20);
char *p = mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
...
munmap(p, 1 << 20); close(fd); shm_unlink("/myseg");   /* name removed; memory freed after last unmap */
```

Backed by `tmpfs` (counts toward memory/`/dev/shm` size; swappable). Also `mmap` of any file with `MAP_SHARED`, **`MAP_ANONYMOUS|MAP_SHARED`** before `fork` (parent/child sharing), and **`memfd_create`** (an anonymous, unnamed memory file you can pass via fd, with sealing for safe sharing: used by Wayland, systemd, JIT).

### System V shared memory

`shmget/shmat/shmdt/shmctl` keyed by integer (`ftok`); persists until removed — **leaks survive process death**: `ipcs -m`, `ipcrm -m ID`. Still used by PostgreSQL (older), Oracle, etc. Limits in `kernel.shmmax`, `shmall`.

### Synchronizing shared memory

Use process-shared primitives placed *inside* the segment:

- `pthread_mutex_t` with `PTHREAD_PROCESS_SHARED` (and `PTHREAD_MUTEX_ROBUST` so a crashed owner doesn't deadlock others: you get `EOWNERDEAD`),
- POSIX semaphores (`sem_open` named, or `sem_init` process-shared in the segment),
- C11 atomics and lock-free ring buffers (single-producer/single-consumer rings need only acquire/release ordering),
- futexes directly.

Caveats: pointers inside shared memory are invalid in other processes (different base addresses): use offsets. Cache-line sharing and false sharing apply. A crash can leave inconsistent data: design for recovery.

## Message queues and System V IPC

**POSIX message queues** (`mq_open`, `/dev/mqueue`): discrete messages with priority, notification via signal/thread, selectable with `poll` (the queue is a descriptor); limits in `/proc/sys/fs/mqueue/`. **SysV** message queues and semaphores (`msgget`, `semget`) remain for legacy; `ipcs -a` shows all. In new designs prefer Unix sockets (SEQPACKET) or a broker.

## The fd family

These expose kernel events as file descriptors so one `epoll` loop handles everything:

| Call | Purpose |
|---|---|
| `eventfd(initval, flags)` | 64-bit counter; `write` adds, `read` returns and resets; wake threads or processes; semaphore mode |
| `signalfd` | signals as readable records (see the signals note) |
| `timerfd_create` / `timerfd_settime` | timers as descriptors, monotonic or realtime |
| `pidfd_open(pid)` / `pidfd_send_signal` | race-free process handles (no PID reuse), readable when the process exits |
| `inotify_init1` / `fanotify` | filesystem events |
| `epoll_create1` | the multiplexer itself (see the event-driven I/O note) |

## Futexes: where locks really live

A **futex** ("fast userspace mutex") is a 32-bit integer in user memory plus two syscall operations: `FUTEX_WAIT(addr, expected)` (sleep if `*addr == expected`) and `FUTEX_WAKE(addr, n)`. The fast path of every lock is a single **atomic compare-and-swap in user space**, with **no kernel involvement when uncontended**; only contention calls `futex`.

```text
lock:   if CAS(state, 0 -> 1) succeeds            -> acquired (no syscall)
        else mark state = 2 (contended); futex_wait(&state, 2) in a loop
unlock: old = exchange(state, 0); if old == 2     -> futex_wake(&state, 1)
```

glibc's `pthread_mutex`, condition variables, semaphores, rwlocks, and `std::mutex` are built on this. Variants: `FUTEX_PI` (priority-inheritance mutexes), `FUTEX_WAIT_BITSET` (timeouts, condvars), `FUTEX_WAKE_OP`, requeue. A futex is keyed by the page (so process-shared memory works between processes) or by `(mm, address)` for private ones (`FUTEX_PRIVATE_FLAG`, faster). Debugging lock contention: `strace -f -e futex`, `perf trace -e futex`, `perf lock`, off-CPU flame graphs.

## Choosing

| Need | Choose |
|---|---|
| Shell pipeline / parent↔child stream | pipe |
| Local service with auth and RPC | Unix socket (stream or seqpacket), `SO_PEERCRED` |
| Pass an open file or socket | Unix socket + `SCM_RIGHTS` |
| Bulk data, microsecond latency | shared memory ring + eventfd/futex wake |
| Many small messages with priority | POSIX mq (or socket) |
| Wake another thread/process | eventfd, futex, condition variable |
| Cross-host or future distribution | TCP/gRPC/messaging (design the same API) |

Cleanup checklist: unlink FIFOs/sockets/shm names on shutdown, `ipcrm` leaked SysV objects, rely on `tmpfiles.d`/runtime directories (`RuntimeDirectory=` in systemd units), and set strict modes (`0600`/`0660`) on named IPC objects, because they are local attack surface.

## Key takeaways

- Pipes: 64 KiB buffer, writes ≤ 4096 bytes atomic, EOF only when all write ends close.
- Unix sockets add permissions, peer credentials and fd passing, and beat loopback TCP.
- Shared memory is fastest but needs process-shared (robust) locks or lock-free structures; use offsets, not pointers, and `shm_unlink`.
- eventfd/signalfd/timerfd/pidfd turn events into fds for a single epoll loop.
- Futexes keep uncontended locking in user space; the kernel is involved only to sleep and wake.
