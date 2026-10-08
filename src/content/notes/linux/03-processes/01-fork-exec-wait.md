---
title: Process Lifecycle — fork, exec, wait
track: linux
module: Processes
moduleOrder: 3
order: 1
level: intermediate
summary: How Unix creates, transforms and reaps processes, exactly what is inherited, and the failure modes (zombies, fd leaks, signal races).
sources:
  - label: "POSIX.1-2017 (IEEE Std 1003.1) — fork(), exec family, waitpid(), posix_spawn()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "man 2 fork, man 2 execve, man 2 wait, man 2 clone, man 7 signal (Linux man-pages)"
    url: https://man7.org/linux/man-pages/
  - label: "Kerrisk, The Linux Programming Interface, Ch. 24–28"
---

Unix separates *creating a process* (`fork`) from *loading a program* (`exec`). The gap between the two calls is where shells set up redirections, pipes, environment and signal state, which is the key to understanding both shell behavior and containers.

## fork(): one call, two returns

`fork()` creates a child that is a near-exact copy of the caller. It returns **twice**: the child's PID in the parent, `0` in the child, `-1` on error (`EAGAIN` for process/memory limits, `ENOMEM`).

```c
pid_t pid = fork();
if (pid < 0)       { perror("fork"); }
else if (pid == 0) { /* child */ }
else               { /* parent: pid is the child's PID */ }
```

### Copy-on-write

Linux does not copy memory. Parent and child share the same physical pages, marked read-only; the first write by either triggers a page fault and the kernel copies **that page**. So `fork` costs roughly the page tables, not the data. A large process (many GB) still pays for duplicating page tables, which is why `fork` of a huge heap gets slow, and why `vfork`/`posix_spawn` exist.

### What the child inherits, and what it does not

| Inherited (copied) | Not inherited |
|---|---|
| memory, stack, heap (COW) | the parent's PID; the child gets a new one |
| open file descriptors, **sharing the same open file description** (offset and status flags are shared) | pending signals (cleared in the child) |
| signal dispositions and the signal mask | timers (`alarm`, `timer_create`) |
| environment, cwd, umask, rlimits, nice value | file locks set by `fcntl(F_SETLK)` (POSIX record locks) |
| credentials (uid/gid), session and process group | memory locks, async I/O operations |
| the process's controlling terminal | **all threads except the caller** |

The shared open file description deserves emphasis. Parent and child writing to an inherited fd share one offset, so their output interleaves in order rather than overwriting. That is exactly how `cmd1 > file` followed by `cmd2 >> file` in a shell behaves.

> **Multithreaded programs:** after `fork` only the calling thread exists in the child. If another thread held a mutex (including the allocator's) at the moment of the fork, the child sees it locked forever. POSIX therefore only permits *async-signal-safe* functions between `fork` and `exec` in a multithreaded process. Use `pthread_atfork` sparingly, or avoid fork-without-exec in threaded code.

### Buffered output surprise

```c
printf("hello");      // no newline: stays in the stdio buffer when stdout is a pipe/file
fork();               // buffer is duplicated into both processes
```

Output: `hellohello`. Flush with `fflush(NULL)` before forking, or use `_exit` in children that did not mean to flush.

## exec(): replace the program

The `exec` family replaces the current process image; **the PID does not change**. `execve` is the only real syscall; the rest (`execl`, `execvp`, `execlp`, ...) are libc wrappers, where `p` means "search `PATH`", `v` an argv vector, `l` a list, and `e` an explicit environment.

```c
char *argv[] = { "ls", "-l", NULL };
execvp("ls", argv);
perror("execvp");        // only reached on failure
_exit(127);
```

State across exec:

- **Reset**: memory, stack, handled signals revert to default (a handler no longer exists); signals set to *ignore* **stay ignored**; the signal mask is kept.
- **Kept**: PID, PPID, process group, session, cwd, uid/gid (unless the file is set-uid/set-gid), resource limits, and **open file descriptors**, unless `FD_CLOEXEC` (`O_CLOEXEC` at open) is set.

The last point is a major source of bugs and security issues: a descriptor opened without `O_CLOEXEC` leaks into every program the process later execs. Prefer `open(..., O_CLOEXEC)`, `socket(..., SOCK_CLOEXEC)`, `pipe2(fds, O_CLOEXEC)`, `dup3`.

A script starting with `#!` is handled by the kernel: `execve` loads the named interpreter and passes the script path as an argument. An ELF binary is mapped by the kernel; if it is dynamically linked, the **dynamic linker** (`PT_INTERP`, typically `ld-linux-x86-64.so.2`) is mapped too and runs first.

## wait(): reaping children

When a process exits it becomes a **zombie**: all resources are freed except the task-table entry holding its exit status. The entry remains until the parent calls `wait*`. Zombies use no CPU or memory beyond the entry, but they hold a PID, and thousands can exhaust the PID space.

```c
int status;
pid_t r = waitpid(pid, &status, 0);   // 0 = block; WNOHANG = poll
if (r == pid) {
    if (WIFEXITED(status))   printf("exit %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status)) printf("killed by %d%s\n", WTERMSIG(status),
                                         WCOREDUMP(status) ? " (core)" : "");
}
```

- `waitpid(-1, ...)` waits for any child; `waitid` offers finer control.
- Exit status holds 8 bits, so `exit(256)` is `0`. By shell convention `128+N` means "killed by signal N", and `126`/`127` mean "found but not executable" / "not found".
- Children also send **SIGCHLD** to the parent on exit or stop. The default action is ignore; setting the disposition to `SIG_IGN` (or `SA_NOCLDWAIT`) tells the kernel to auto-reap, and `wait` then blocks until all children are gone and returns `ECHILD`.
- A long-running server should reap in a `SIGCHLD` handler using a `waitpid(-1, NULL, WNOHANG)` **loop**, because signals coalesce and one delivery may cover several exited children.

### Orphans

If the parent dies first, children are **reparented** to PID 1, or to the nearest ancestor marked as a *subreaper* (`prctl(PR_SET_CHILD_SUBREAPER)`), which reaps them. This is why a container's PID 1 must reap zombies: if your app is PID 1 and ignores `wait`, orphans accumulate (the reason for tiny init shims such as `tini`).

## A minimal shell core

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    char line[1024];
    for (;;) {
        fputs("$ ", stdout); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        char *argv[64]; int n = 0;
        for (char *t = strtok(line, " \t\n"); t && n < 63; t = strtok(NULL, " \t\n"))
            argv[n++] = t;
        argv[n] = NULL;
        if (n == 0) continue;
        if (!strcmp(argv[0], "cd")) { if (n > 1 && chdir(argv[1])) perror("cd"); continue; }
        if (!strcmp(argv[0], "exit")) break;

        pid_t pid = fork();
        if (pid == 0) {                      // child: redirections would be set up HERE
            execvp(argv[0], argv);
            perror(argv[0]); _exit(127);
        } else if (pid > 0) {
            int st; waitpid(pid, &st, 0);
        } else perror("fork");
    }
    return 0;
}
```

`cd` must be a **built-in**: a child cannot change its parent's working directory. Redirection `cmd > f` is implemented by `open` + `dup2(fd, 1)` in the child between fork and exec; a pipe `a | b` is `pipe()`, two forks, and `dup2` on each end.

## Alternatives to plain fork+exec

| API | Behavior | Use when |
|---|---|---|
| `vfork()` | child borrows the parent's address space; parent suspended until `exec`/`_exit` | legacy fast spawn; only `exec`/`_exit` allowed in the child |
| `posix_spawn()` | fork+exec as one call, with file-actions and attributes | portable, large parents, no threads-and-fork hazards |
| `clone()`/`clone3()` | fine-grained: choose what to share (`CLONE_VM`, `CLONE_FILES`, `CLONE_NEWNS`, `CLONE_NEWPID`...) | threads (`pthread_create` uses it) and **containers** (namespaces) |

In Linux a thread is simply a task created by `clone` that shares the address space, files and signal handlers. A "process" is a thread group (same TGID, the value `getpid()` returns).

## Observing it

```bash
ps -o pid,ppid,stat,wchan:20,cmd --forest   # STAT: R running, S sleeping, D uninterruptible, Z zombie, T stopped
cat /proc/$$/status | grep -E 'Pid|PPid|Threads|SigIgn|SigCgt'
ls -l /proc/$$/fd                           # inherited descriptors
strace -f -e trace=process,execve bash -c 'ls | wc -l'   # watch clone/execve/wait4
pstree -p $$
```

A `D` (uninterruptible sleep, usually blocked on I/O) process cannot be killed even with `SIGKILL` until the I/O returns. A `Z` process cannot be killed at all, because it is already dead: kill or fix the **parent**.

## Key takeaways

- `fork` copies the process (COW); the open file *descriptions* stay shared.
- `exec` replaces the image but keeps the PID and non-`CLOEXEC` descriptors; handlers reset, ignores persist.
- Always reap children; `SIGCHLD` handlers need a `WNOHANG` loop.
- Between `fork` and `exec` do as little as possible, and in threaded programs use only async-signal-safe calls.
- Redirection, pipes and job control are all built from `fork`, `dup2`, `pipe` and `exec`.
