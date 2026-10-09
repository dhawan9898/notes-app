---
title: Threads and Synchronization
track: linux
module: Processes
moduleOrder: 3
order: 5
level: advanced
summary: How Linux implements threads, the pthread synchronization primitives and when to use each, the C11 memory model, data races and deadlocks with runnable examples, and tools that find concurrency bugs.
sources:
  - label: "POSIX.1-2017 — Threads (pthread_create, mutexes, condition variables, rwlocks, barriers)"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
  - label: "ISO/IEC 9899:2011 §5.1.2.4 and §7.17 — Multi-threaded executions and data races; Atomics"
  - label: "man 7 pthreads, man 2 clone, man 3 pthread_mutexattr_setprotocol"
    url: https://man7.org/linux/man-pages/man7/pthreads.7.html
  - label: "Drepper — What Every Programmer Should Know About Memory (false sharing, caches)"
  - label: "McKenney — Is Parallel Programming Hard, And, If So, What Can You Do About It?"
    url: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html
  - label: "Clang ThreadSanitizer documentation"
    url: https://clang.llvm.org/docs/ThreadSanitizer.html
---

## What a thread is on Linux

The kernel schedules **tasks**. A *thread* is a task created with `clone()` flags that share resources with its creator: `CLONE_VM` (address space), `CLONE_FILES` (fd table), `CLONE_FS`, `CLONE_SIGHAND`, `CLONE_THREAD` (same thread group), `CLONE_SYSVSEM`. A *process* is a task that shares nothing (`fork`). So threads share memory, descriptors and signal dispositions, but each has its own **stack**, registers, **thread ID**, signal mask, `errno` and thread-local storage.

- **PID vs TID**: `getpid()` returns the thread-group ID (same for all threads); `gettid()` the kernel task ID. `/proc/PID/task/TID/` has per-thread data; `ps -L`, `top -H`, `htop` show threads.
- The glibc **NPTL** library implements POSIX threads 1:1 on kernel tasks (so a blocking syscall blocks only that thread, and the scheduler sees every thread).
- Default thread stack: `ulimit -s` (8 MiB *virtual*, committed lazily); thousands of threads cost memory and context switching; tune with `pthread_attr_setstacksize`.
- A thread that calls `exit()` ends the whole process; `pthread_exit` ends only itself. Compile and link with `-pthread`.

Cost: creating a thread ≈ 10–50 µs; a context switch between threads of one process is cheaper than between processes (no address-space switch, TLB mostly preserved) but still ~1–5 µs including cache effects. Use **thread pools** and event loops (epoll) instead of thread-per-connection for large concurrency.

## Data races and why synchronization is needed

A **data race** (two threads access the same memory location concurrently, at least one writing, with no synchronization) is **undefined behavior** in C and C++. The compiler may reorder, cache values in registers, or eliminate loads; CPUs reorder memory operations and cache them per core. "It works on my x86" proves nothing.

The experiment: eight threads each increment a shared counter one million times.

```c
long plain = 0; atomic_long atom = 0; long locked = 0;     /* three variants */
plain++;                                                    /* racy */
atomic_fetch_add_explicit(&atom, 1, memory_order_relaxed);  /* atomic */
pthread_mutex_lock(&m); locked++; pthread_mutex_unlock(&m); /* mutex */
```

```text
expected 8000000
plain  (racy)   7889681     <- lost updates; varies run to run
atomic          8000000
mutex           8000000
```

`plain++` is load, add, store: two threads can both load the same value and one increment is lost. ThreadSanitizer reports it deterministically:

```text
$ gcc -g -O1 -fsanitize=thread -pthread thr.c && ./a.out
WARNING: ThreadSanitizer: data race
  Write of size 8 at 0x... by thread T2:
  Location is global 'plain' of size 8
```

## Synchronization primitives

### Mutex

`pthread_mutex_lock/unlock` provides mutual exclusion around a **critical section**. Types (`pthread_mutexattr_settype`): *normal* (default; relocking self deadlocks), *recursive* (same thread may relock; count), *errorcheck* (returns `EDEADLK`). Attributes: `PTHREAD_PROCESS_SHARED` (between processes), `PTHREAD_MUTEX_ROBUST` (owner died → `EOWNERDEAD`, repair with `pthread_mutex_consistent`), `PTHREAD_PRIO_INHERIT` (avoid priority inversion). Uncontended lock/unlock is ~20 ns (a futex fast path); contended locks sleep in the kernel.

Rules: hold locks briefly; never call unknown code or block on I/O while holding one; lock the *data*, not the code; document which mutex protects which fields.

### Condition variables

Wait for a *condition on shared state*, always together with the mutex and in a **loop** (spurious wakeups and stolen wakeups happen):

```c
pthread_mutex_lock(&m);
while (count == CAP) pthread_cond_wait(&not_full, &m);   /* atomically unlock + sleep, relock on wake */
buf[tail] = v; tail = (tail + 1) % CAP; count++;
pthread_cond_signal(&not_empty);
pthread_mutex_unlock(&m);
```

A complete bounded producer/consumer (this runs clean under ThreadSanitizer and prints `consumed sum = 55 (expected 55)`): producers wait on `not_full`, consumers on `not_empty`, a `done` flag plus `pthread_cond_broadcast` ends the consumers. `signal` wakes one waiter, `broadcast` all. Never wait without a predicate; never signal "outside the lock" without understanding the missed-wakeup consequences.

### Other primitives

| Primitive | Use |
|---|---|
| `pthread_rwlock_t` | many readers or one writer; beware writer starvation and that read-lock overhead often exceeds a mutex for short sections |
| `sem_t` (POSIX semaphore) | counting resources; works across processes when named/shared |
| `pthread_barrier_t` | all N threads wait until everyone arrives (phases in parallel algorithms) |
| `pthread_spinlock_t` / user spinlocks | busy-wait for very short sections on multi-core with no preemption (in userspace usually a mistake: a descheduled lock holder makes others burn CPU) |
| `pthread_once` / `call_once` | one-time initialization |
| Thread-local storage `__thread` / `_Thread_local` | per-thread data (e.g. `errno`) to avoid sharing |

## Atomics and the memory model

C11 `<stdatomic.h>` defines atomic types and **memory orders** that tell compiler and CPU how much reordering is allowed:

| Order | Guarantee |
|---|---|
| `relaxed` | atomicity only (a counter) |
| `acquire` (load) / `release` (store) | a release store *publishes* all earlier writes to a thread that acquire-loads the same variable and sees the value |
| `acq_rel` | for read-modify-write |
| `seq_cst` (default) | a single global order of all such operations; simplest, slightly slower |

The classic publication pattern:

```c
/* writer */ data = 42;                        atomic_store_explicit(&ready, 1, memory_order_release);
/* reader */ while (!atomic_load_explicit(&ready, memory_order_acquire)) ; use(data);   /* sees 42 */
```

`volatile` is **not** a synchronization tool in C (it prevents some compiler optimizations of device registers but gives no atomicity or ordering between threads). x86 is strongly ordered (TSO: only store→load reordering), so races often hide there and break on ARM/POWER, which allow far more reordering.

Lock-free structures (queues, stacks) use compare-and-swap (`atomic_compare_exchange_*`) and suffer the **ABA problem**, memory reclamation difficulties (hazard pointers, RCU), and are easy to get wrong. Use a mutex first; reach for lock-free only with measurements.

## Deadlock, livelock, starvation

**Deadlock** needs four conditions (Coffman): mutual exclusion, hold-and-wait, no preemption, circular wait. Break one — typically circular wait by imposing a **global lock order** (e.g. by address or a documented hierarchy): thread 1 locks A then B while thread 2 locks B then A is the textbook failure.

Practical rules: take locks in a fixed order; use `pthread_mutex_trylock`/timed locks with backoff for optional locks; avoid callbacks while locked; don't lock in signal handlers; avoid holding a lock across `fork` (use `pthread_atfork` carefully). **Livelock**: threads keep reacting to each other without progress; **starvation**: a thread never gets the resource (unfair locks, reader-preferring rwlocks).

Detecting: `gdb -p PID` → `thread apply all bt` shows who waits on what; `helgrind`/`drd` (Valgrind) and TSan's `-fsanitize=thread` detect lock-order inversions; `perf lock`, `strace -f -e futex`; deadlock watchdogs.

## Performance pitfalls

- **Lock contention**: serialization limits scaling (Amdahl's law). Fix by reducing critical section size, sharding (per-CPU or per-bucket locks), read-mostly structures (RCU), batching, or removing sharing.
- **False sharing**: different variables on the same 64-byte cache line, written by different cores, cause the line to ping-pong. Pad hot per-thread counters (`alignas(64)`).
- **Cache coherence traffic** from atomic operations on a shared counter: use per-thread counters aggregated occasionally.
- **Thundering herd** from `pthread_cond_broadcast` to many waiters; wake only who can proceed.
- **Oversubscription**: more runnable threads than cores → context-switch overhead; size pools near core count for CPU-bound work.
- NUMA: keep a thread's data local (first-touch allocation, `numactl`).

## Concurrency and the rest of the system

- **Signals**: process-directed signals go to any thread; block them and use `sigwait` in one thread.
- **fork**: only the calling thread survives in the child; locks held by others stay locked: `exec` immediately.
- **Async-signal-safety**, `errno` is per-thread; many libc functions have reentrant `_r` variants (`strtok_r`, `localtime_r`).
- **Cancellation** (`pthread_cancel`) is dangerous with resources; prefer cooperative shutdown flags and condition variables.
- Languages: Go goroutines, Rust's `Send`/`Sync` and ownership, Java's memory model: all map to these concepts.

## Tooling

```bash
gcc -Wall -pthread -fsanitize=thread -g prog.c        # TSan: data races, lock-order inversions
valgrind --tool=helgrind ./prog                       # races, lock misuse (slow)
ps -L -p PID -o pid,tid,psr,pcpu,comm                 # threads and the CPU each last ran on
top -H -p PID ; pidstat -t -p PID 1                   # per-thread CPU and switches
perf record -g -p PID ; perf lock record ...          # profiles; lock contention
gdb -p PID -batch -ex 'thread apply all bt'           # snapshot of all stacks (deadlock hunting)
```

## Key takeaways

- Linux threads are tasks sharing memory, files and signal handlers via `clone`; they have their own stack, mask and TLS.
- A data race is undefined behavior: protect shared state with a mutex or make it atomic (measured: 7.89 M instead of 8.0 M increments when racy).
- Condition variables need a mutex and a predicate loop; spurious wakeups are normal.
- Understand acquire/release ordering; `volatile` isn't a sync primitive; x86 hides bugs ARM exposes.
- Prevent deadlock with a global lock order; find races with TSan; avoid false sharing and over-contended locks.
