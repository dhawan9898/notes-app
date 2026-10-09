---
title: Signals in Depth
track: linux
module: Processes
moduleOrder: 3
order: 3
level: advanced
summary: Signal semantics from generation to delivery — the standard and real-time signals, masks and pending sets, safe handlers, EINTR, threads, signalfd and the practical patterns for graceful shutdown and core dumps.
sources:
  - label: "POSIX.1-2017 — Signal Concepts (XSH 2.4), sigaction(), pthread_sigmask()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigaction.html
  - label: "man 7 signal, man 2 sigaction, man 2 signalfd, man 7 signal-safety, man 5 core"
    url: https://man7.org/linux/man-pages/man7/signal.7.html
  - label: "man 7 signal-safety — async-signal-safe functions"
    url: https://man7.org/linux/man-pages/man7/signal-safety.7.html
  - label: "Kerrisk — The Linux Programming Interface, Ch. 20–22"
---

A **signal** is an asynchronous, minimal notification delivered to a process or thread: "something happened". Signals carry little information (a number, optionally a small `siginfo_t`), interrupt normal control flow, and are the hardest part of Unix to get right.

## Lifecycle

1. **Generation**: by the kernel (a fault: `SIGSEGV`, `SIGFPE`; terminal: Ctrl-C → `SIGINT`; child exit → `SIGCHLD`; broken pipe → `SIGPIPE`; timers → `SIGALRM`) or by another process (`kill(2)`, `sigqueue(3)`, `tgkill`).
2. **Pending**: the signal is recorded as pending for the process (process-directed) or thread (thread-directed).
3. **Delivery**: when the target next returns to user mode and the signal is **not blocked**, the kernel diverts it: runs the default action, ignores it, or invokes the registered **handler** on the thread's stack (or an alternate signal stack), then resumes via `sigreturn`.

A signal can be **blocked** (masked) per thread (`sigprocmask`/`pthread_sigmask`); while blocked it stays pending. Ignoring (`SIG_IGN`) discards it.

## The signals

| # (x86/ARM) | Name | Default | Meaning / notes |
|---|---|---|---|
| 1 | `SIGHUP` | terminate | terminal hung up; by convention "reload config" for daemons |
| 2 | `SIGINT` | terminate | Ctrl-C |
| 3 | `SIGQUIT` | core | Ctrl-\\ |
| 4 | `SIGILL` | core | illegal instruction |
| 6 | `SIGABRT` | core | `abort()`, failed assert |
| 7 | `SIGBUS` | core | bad memory access (misaligned, mmap beyond file end / disk error) |
| 8 | `SIGFPE` | core | arithmetic error (integer divide by zero) |
| **9** | **`SIGKILL`** | terminate | **cannot be caught, blocked or ignored** |
| 10, 12 | `SIGUSR1/2` | terminate | user-defined |
| 11 | `SIGSEGV` | core | invalid memory reference |
| 13 | `SIGPIPE` | terminate | write to a pipe/socket with no reader |
| 14 | `SIGALRM` | terminate | `alarm()`/`setitimer` expiry |
| **15** | **`SIGTERM`** | terminate | polite termination request (default of `kill`) |
| 17 | `SIGCHLD` | ignore | child stopped/terminated |
| 18 | `SIGCONT` | continue | resume a stopped process |
| **19** | **`SIGSTOP`** | stop | **uncatchable** stop |
| 20 | `SIGTSTP` | stop | Ctrl-Z (catchable) |
| 21, 22 | `SIGTTIN/TTOU` | stop | background process reading/writing the terminal |
| 28 | `SIGWINCH` | ignore | terminal resized |
| 31 | `SIGSYS` | core | bad syscall (also used by seccomp) |
| 34–64 | `SIGRTMIN…SIGRTMAX` | terminate | **real-time signals** |

(`kill -l` lists numbers on your architecture.) Dispositions: *Term*, *Core* (terminate + core dump), *Stop*, *Cont*, *Ign*.

### Standard vs. real-time signals

Standard signals do **not queue**: if a second `SIGUSR1` arrives while one is pending, only one is delivered. Real-time signals (34–64) **queue** (up to `RLIMIT_SIGPENDING`), can carry a data value (`sigqueue`), are delivered in order, and lowest number first. Verified:

```c
/* block both, send each 5 times, then unblock */
SIGUSR1 handler calls: 1, SIGRTMIN handler calls: 5
```

Therefore never use a standard signal as a *counter* or to carry a message sequence: treat it as "something happened, go check the state". `SIGCHLD` is the classic case: one delivery may represent several exited children, so loop on `waitpid(-1, &st, WNOHANG)`.

## Installing handlers: `sigaction`

```c
struct sigaction sa = { .sa_handler = on_term, .sa_flags = SA_RESTART };
sigemptyset(&sa.sa_mask);              /* additional signals to block while running */
sigaction(SIGTERM, &sa, NULL);
```

Prefer `sigaction` over `signal()` (portable semantics vary). Flags: `SA_RESTART` (restart interrupted syscalls), `SA_SIGINFO` (handler gets `siginfo_t`: sender PID/UID, fault address, value), `SA_NODEFER`, `SA_RESETHAND`, `SA_ONSTACK` (use the `sigaltstack`, needed to handle stack overflow `SIGSEGV`), `SA_NOCLDWAIT`/`SA_NOCLDSTOP`.

### What a handler may do

A handler can interrupt the program **at any instruction**, including in the middle of `malloc` or `printf`. Calling non-reentrant functions there can deadlock or corrupt state. Only **async-signal-safe** functions are allowed: `write`, `_exit`, `read`, `open`, `close`, `kill`, `sigaction`, `waitpid`, `signal`-manipulating calls and similar (see `man 7 signal-safety`); **not** `printf`, `malloc`, `free`, stdio, `pthread_mutex_lock`, or anything that takes locks.

Safe patterns:

1. **Set a flag**: `volatile sig_atomic_t got_term = 1;` and check it in the main loop.
2. **Self-pipe / `eventfd` trick**: handler `write()`s one byte to a pipe monitored by `poll`/`epoll`; the main loop handles the event.
3. **Best: no handler at all.** Block the signals and read them synchronously: `signalfd(2)` (a file descriptor from which `read()` returns `struct signalfd_siginfo`, usable with `epoll`), or `sigwaitinfo`/`sigtimedwait`, or dedicate a thread to `sigwait`. This eliminates asynchronous interruption.

Also save and restore `errno` in handlers.

### EINTR and races

A blocking syscall (`read`, `accept`, `sleep`) interrupted by a handled signal returns `-1`/`EINTR` unless `SA_RESTART` applies (some calls like `poll`, `select`, `nanosleep` never restart). Robust code loops on `EINTR`. The classic **race**: check a flag, then block in `select`; a signal arriving *between* them is lost. Close the window with `pselect`/`ppoll`/`epoll_pwait` (atomically swap the mask while waiting) or the self-pipe/signalfd techniques.

## Signals and threads

- A process-directed signal (`kill`) is delivered to **one arbitrary thread** that doesn't block it. Thread-directed (`pthread_kill`, `tgkill`, faults like `SIGSEGV`) goes to that thread.
- Handlers and dispositions are **process-wide**; the signal mask is **per thread**.
- Design: block all signals in every thread (`pthread_sigmask` before creating threads) and handle them in a single dedicated signal thread with `sigwait`.
- `fork` copies handlers and mask into the child; `execve` resets handlers to default but keeps ignored signals ignored and the mask (shells that start children with `SIGINT` ignored cause "Ctrl-C doesn't work" surprises).

## Sending and inspecting

```bash
kill -TERM 1234 ; kill -15 1234 ; kill -s HUP $(cat /run/app.pid)
kill -0 PID                  # no signal sent: tests existence and permission
kill -- -PGID                # signal a whole process group (negative PID)
pkill -f 'regex' ; killall name
kill -STOP 1234 ; kill -CONT 1234     # pause/resume

grep -E 'Sig(Pnd|Blk|Ign|Cgt)' /proc/PID/status
# SigPnd: pending (thread), ShdPnd: process-wide, SigBlk: blocked mask,
# SigIgn: ignored mask, SigCgt: caught (has a handler): hex bitmaps, bit n-1 = signal n
```

Permission: the sender needs the same real or effective UID as the target's real or saved UID, or `CAP_KILL`. The error `ESRCH` means no such process; `EPERM` not allowed.

## Special cases

- **PID 1** (init, or a container's main process) gets *no default actions*: signals with default dispositions are ignored unless it installed a handler. `docker stop` sends `SIGTERM` to PID 1 of the container; if your app doesn't handle it, it ignores it and is killed after 10 s by `SIGKILL`. Use `--init` (tini) or a proper handler, and the `exec` form of `CMD` so the app *is* PID 1 (shell wrappers swallow signals).
- **Zombies** (`Z`) are already dead: killing them does nothing; the parent must `wait`.
- **D state** (uninterruptible sleep, usually disk/NFS): even `SIGKILL` is deferred until the syscall returns; stuck mounts leave unkillable processes.
- **`SIGPIPE`**: servers that write to closed sockets should ignore it (`signal(SIGPIPE, SIG_IGN)`, or `send(..., MSG_NOSIGNAL)`) and handle `EPIPE`.
- **`SIGSEGV` handlers** can print backtraces but cannot safely continue; prefer core dumps.

## Graceful shutdown pattern

1. Receive `SIGTERM` (blocked + signalfd, or a flag).
2. Stop accepting new work (close the listen socket), finish or hand off in-flight requests with a deadline, flush and close.
3. Exit `0`. If the orchestrator's timeout elapses, `SIGKILL` follows, so design for crash safety anyway.

systemd: `KillSignal=SIGTERM`, `TimeoutStopSec=90s` then `SIGKILL`; `ExecReload=/bin/kill -HUP $MAINPID` for reloads; `KillMode=control-group` kills the whole cgroup (so stray children don't survive).

## Core dumps

On `Core` signals the kernel writes a memory image if `ulimit -c` allows it. `kernel.core_pattern` decides where: a file pattern (`core.%e.%p`) or a pipe to a handler (`|/usr/lib/systemd/systemd-coredump ...`, then `coredumpctl list`, `coredumpctl gdb`). Analysis: `gdb ./prog core` → `bt full`. Set `LimitCORE=infinity` in units; protect sensitive memory (`PR_SET_DUMPABLE`, `MADV_DONTDUMP`); `fs.suid_dumpable`. A watchdog with `SIGABRT` gives a stack of a hung process: `kill -ABRT PID`.

## Key takeaways

- Signals are async notifications: standard ones coalesce, real-time ones queue (1 vs 5 delivered in the test above).
- `SIGKILL` and `SIGSTOP` are uncatchable; `SIGTERM` is the polite request; `SIGHUP` is conventionally "reload".
- In handlers use only async-signal-safe calls; better, block signals and consume them with `signalfd`/`sigwait` in the event loop.
- Loop on `EINTR`; use `ppoll`/`pselect` to avoid check-then-block races; loop on `waitpid(WNOHANG)` for `SIGCHLD`.
- PID 1 and D-state processes are special; plan graceful shutdown with a hard deadline.
