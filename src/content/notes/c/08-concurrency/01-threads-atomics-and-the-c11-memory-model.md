---
title: "Threads, Atomics and the C11 Memory Model"
track: c
module: Concurrency in C
moduleOrder: 8
order: 1
level: expert
summary: "Data races as undefined behaviour, pthreads vs C11 threads, _Atomic and the six memory orders, release/acquire message passing, what each order costs on x86-64 (read from real compiler output), a spinlock, ThreadSanitizer, and the ABA and false-sharing traps."
prerequisites:
  - "C Standards, the Abstract Machine and Undefined Behavior"
  - "Qualifiers, Storage Classes, Scope and Linkage"
  - "Performance-Oriented C: Caches, Branches, Data Layout and Vectorization"
sources:
  - label: "ISO/IEC 9899:2011 §5.1.2.4 — Multi-threaded executions and data races"
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
  - label: "ISO/IEC 9899:2011 §7.17 — <stdatomic.h>"
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
  - label: "POSIX.1-2017 — pthread.h and §4.12 Memory Synchronization"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "Boehm & Adve, \"Foundations of the C++ Concurrency Memory Model\" (PLDI 2008)"
    url: https://doi.org/10.1145/1375581.1375591
  - label: "Intel 64 and IA-32 Architectures SDM, Vol. 3A §8.2 — Memory Ordering"
    url: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
  - label: "Linux kernel — Documentation/memory-barriers.txt"
    url: https://www.kernel.org/doc/Documentation/memory-barriers.txt
  - label: "ThreadSanitizer (Clang/GCC documentation)"
    url: https://clang.llvm.org/docs/ThreadSanitizer.html
---

Before C11 the language had no concept of a thread: concurrency was a library (POSIX threads) whose rules lived outside the standard, and compilers could not be told what a "shared variable" was. C11 added a **memory model**, `<threads.h>` and `<stdatomic.h>`. The model answers one question: *when a thread writes a variable, when must another thread see it, and in what order?*

## Data races are undefined behaviour

Two accesses to the same object **conflict** if at least one is a write. They form a **data race** if they come from different threads and are not ordered by *happens-before* (§5.1.2.4). A data race is **undefined behaviour**, not "a stale value". The compiler is entitled to assume there are none: it can keep a shared variable in a register across a loop (so a `while (!flag);` spins forever), reorder, split or duplicate accesses.

Three things people reach for that do **not** fix a race:

- `volatile`: it prevents the compiler from eliding accesses, but gives no atomicity and no ordering with respect to other memory. It is for memory-mapped I/O, not threads.
- Using a `bool`/`int` flag "because writes are atomic on x86": the compiler, not the CPU, is what breaks you.
- "It worked in testing".

The tools are **mutexes** (which establish happens-before on lock/unlock), **atomics**, and joining a thread.

```mermaid
graph LR
  A["Thread A: data=1;<br/>ready.store(1, release)"] -- "synchronizes-with" --> B["Thread B: ready.load(acquire)==1;<br/>reads data"]
```

## Threads: POSIX and C11

POSIX `pthread_create`/`pthread_join`/`pthread_mutex_*`/`pthread_cond_*` is what real code uses on Linux and macOS (link with `-pthread`). C11's optional `<threads.h>` (`thrd_create`, `mtx_lock`, `cnd_wait`) mirrors it but is optional and absent on some platforms (glibc added it only in 2.28). Prefer pthreads unless you target a toolchain that has `<threads.h>` and need portability beyond POSIX.

Rules worth memorising: always join or detach every thread; condition variables need a predicate loop (`while (!cond) pthread_cond_wait(...)`) because of spurious wakeups; never hold a lock across a blocking call you can avoid; define one global lock order to prevent deadlock; `fork()` in a multithreaded process leaves only the calling thread in the child, and only async-signal-safe functions are safe afterward.

## Atomics and the memory orders

`_Atomic int x;` (or `atomic_int`) makes loads, stores and read-modify-write operations on `x` indivisible. Every atomic operation also takes a **memory order** that says how much *other* memory it orders:

| Order | Guarantee | Use |
|---|---|---|
| `memory_order_relaxed` | atomicity only; no ordering of other accesses | counters, statistics |
| `memory_order_acquire` (load) | later reads/writes in this thread cannot move before it | take a lock, consume a published pointer |
| `memory_order_release` (store) | earlier reads/writes cannot move after it | release a lock, publish data |
| `memory_order_acq_rel` | both, for read-modify-write | CAS in lock-free structures |
| `memory_order_seq_cst` | acquire+release **and** one global total order of all seq_cst operations | the default; simplest to reason about |
| `memory_order_consume` | like acquire, only for data-dependent loads | unusable in practice; compilers treat it as acquire |

If you omit the order (`atomic_store(&x, 1)`), you get `seq_cst`. Start there and weaken only where profiling shows it matters and you can justify it in a comment.

### Atomic counters

Four threads each add 1,000,000 to a shared counter, first with a plain (racy) `int`, then an atomic, then a mutex:

```c
// Lost updates: plain int vs C11 atomic vs mutex, 4 threads x 1,000,000 increments.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define THREADS 4
#define N 1000000

static int plain;                       // data race: undefined behaviour in C11
static atomic_int atom;
static int locked;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

static void *work(void *arg) {
    int mode = *(int *)arg;
    for (int i = 0; i < N; i++) {
        if (mode == 0) { *(volatile int *)&plain += 1; }   // volatile stops the optimizer folding the loop; it does NOT make it atomic
        else if (mode == 1) atomic_fetch_add_explicit(&atom, 1, memory_order_relaxed);
        else { pthread_mutex_lock(&mu); locked++; pthread_mutex_unlock(&mu); }
    }
    return NULL;
}

int main(void) {
    const char *name[] = {"plain int (racy)", "atomic_fetch_add", "mutex"};
    for (int mode = 0; mode < 3; mode++) {
        pthread_t t[THREADS];
        for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, work, &mode);
        for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
        int v = mode == 0 ? plain : mode == 1 ? atomic_load(&atom) : locked;
        printf("%-18s result %7d (expected %d)\n", name[mode], v, THREADS * N);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -pthread counter.c -o counter && ./counter
plain int (racy)   result 2189683 (expected 4000000)
atomic_fetch_add   result 4000000 (expected 4000000)
mutex              result 4000000 (expected 4000000)
```

The racy counter loses most of its updates (the exact figure changes every run, because `x += 1` is a load, an add and a store, and two threads can both load the same value). Both the atomic and the mutex are exact. The atomic needs only `relaxed` because the counter orders nothing else.

### Publishing data: release and acquire

A **release** store pairs with an **acquire** load that reads the value it wrote: everything the producer did before the release is visible to the consumer after the acquire. That is how you hand a buffer to another thread without a lock:

```c
// Message passing: the producer fills a buffer, then publishes a flag with a release store;
// the consumer spins on the flag with an acquire load, then reads the buffer.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static int data[4];
static atomic_int ready;

static void *producer(void *a) {
    (void)a;
    for (int i = 0; i < 4; i++) data[i] = 100 + i;                 // plain writes
    atomic_store_explicit(&ready, 1, memory_order_release);        // publishes everything above
    return NULL;
}
static void *consumer(void *a) {
    (void)a;
    while (!atomic_load_explicit(&ready, memory_order_acquire)) ;  // pairs with the release store
    printf("consumer sees %d %d %d %d\n", data[0], data[1], data[2], data[3]);
    return NULL;
}
int main(void) {
    pthread_t p, c;
    pthread_create(&c, NULL, consumer, NULL);
    pthread_create(&p, NULL, producer, NULL);
    pthread_join(p, NULL); pthread_join(c, NULL);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -pthread mp.c -o mp && ./mp
consumer sees 100 101 102 103
```

Without the release/acquire pair the consumer could legally see `ready == 1` and still read stale `data` (the compiler may move the plain writes after the flag store, and on weakly ordered CPUs the hardware may too).

### What the orders cost: read the instructions

```c
// What the compiler emits for each memory order on x86-64 (compile with: gcc -O2 -S -o - orders.c)
#include <stdatomic.h>
atomic_int a, b;
void store_relaxed(int v) { atomic_store_explicit(&a, v, memory_order_relaxed); }
void store_release(int v) { atomic_store_explicit(&a, v, memory_order_release); }
void store_seq_cst(int v) { atomic_store_explicit(&a, v, memory_order_seq_cst); }
int  load_acquire(void)   { return atomic_load_explicit(&a, memory_order_acquire); }
int  load_seq_cst(void)   { return atomic_load_explicit(&a, memory_order_seq_cst); }
int  fetch_add(void)      { return atomic_fetch_add_explicit(&a, 1, memory_order_relaxed); }
int  cas(int e, int d)    { return atomic_compare_exchange_strong(&a, &e, d); }
```

```text
$ gcc -O2 -S -o - orders.c | grep -E '^(store|load|fetch|cas)|movl|xchg|lock'
store_relaxed:
	movl	%edi, a(%rip)
store_release:
	movl	%edi, a(%rip)
store_seq_cst:
	xchgl	a(%rip), %edi
load_acquire:
	movl	a(%rip), %eax
load_seq_cst:
	movl	a(%rip), %eax
fetch_add:
	movl	$1, %eax
	lock xaddl	%eax, a(%rip)
cas:
	movl	%edi, %eax
	lock cmpxchgl	%esi, a(%rip)
```

On x86-64 (a **total-store-order** machine) every normal load already behaves like an acquire and every normal store like a release, so `release` and `acquire` compile to ordinary `mov`. Only `seq_cst` **stores** pay: `xchg` (an implicitly locked instruction) drains the store buffer. Read-modify-writes use the `lock` prefix whatever the order. On ARM64 and POWER the picture reverses: relaxed is cheap, release/acquire use `stlr`/`ldar`, and the compiler really emits barriers (`dmb`). Code that uses `relaxed` or omits `release/acquire` can pass every test on x86 and fail on a phone. That is the strongest argument for writing the correct order even where x86 would forgive you.

### Why seq_cst exists: store buffering

Each x86 core buffers its stores, so a core can read a later location before its own earlier store is visible to others. The classic litmus test: initially `x = y = 0`; thread 0 does `x = 1; r0 = y`, thread 1 does `y = 1; r1 = x`. Under sequential consistency at least one thread must read 1, but with store buffers both can read 0 unless a full barrier (`seq_cst` store, `mfence`) separates each store from the following load (Intel SDM Vol. 3A §8.2.3.4).

```c
// Store-buffering litmus test:  T0: x=1; r0=y    T1: y=1; r1=x     Can both threads read 0?
// Sequential consistency says no. Real CPUs (store buffers) say yes unless a full fence is used.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define ROUNDS 2000000

static _Alignas(128) atomic_int x;
static _Alignas(128) atomic_int y;           // separate cache lines
static _Alignas(128) atomic_int go;          // main thread bumps this once per round; both workers spin on it
static _Alignas(128) atomic_int done;
static int r0[ROUNDS], r1[ROUNDS];           // per-round results, examined after the threads finish
static int mo;                               // memory order under test: 0 relaxed, 1 release/acquire, 2 seq_cst

static void store(atomic_int *p, int v) {
    atomic_store_explicit(p, v, mo == 0 ? memory_order_relaxed : mo == 1 ? memory_order_release : memory_order_seq_cst);
}
static int load(atomic_int *p) {
    return atomic_load_explicit(p, mo == 0 ? memory_order_relaxed : mo == 1 ? memory_order_acquire : memory_order_seq_cst);
}
static void *thread(void *arg) {
    int me = *(int *)arg;
    for (int i = 1; i <= ROUNDS; i++) {
        while (atomic_load_explicit(&go, memory_order_acquire) != i) ;   // both spin on the same line
        if (me == 0) { store(&x, 1); r0[i - 1] = load(&y); }
        else         { store(&y, 1); r1[i - 1] = load(&x); }
        atomic_fetch_add(&done, 1);
    }
    return NULL;
}

int main(void) {
    const char *name[] = {"relaxed", "release/acquire", "seq_cst"};
    for (mo = 0; mo < 3; mo++) {
        atomic_store(&go, 0); atomic_store(&done, 0);
        pthread_t a, b; int id0 = 0, id1 = 1;
        pthread_create(&a, NULL, thread, &id0); pthread_create(&b, NULL, thread, &id1);
        for (int i = 1; i <= ROUNDS; i++) {
            atomic_store(&x, 0); atomic_store(&y, 0); atomic_store(&done, 0);
            atomic_store_explicit(&go, i, memory_order_release);        // release both workers
            while (atomic_load(&done) != 2) ;
        }
        pthread_join(a, NULL); pthread_join(b, NULL);
        int both_zero = 0;
        for (int i = 0; i < ROUNDS; i++) both_zero += (r0[i] == 0 && r1[i] == 0);
        printf("%-16s both threads read 0 in %d of %d rounds\n", name[mo], both_zero, ROUNDS);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -pthread litmus.c -o litmus && ./litmus
relaxed          both threads read 0 in 0 of 2000000 rounds
release/acquire  both threads read 0 in 0 of 2000000 rounds
seq_cst          both threads read 0 in 0 of 2000000 rounds
```

Be careful about what this does and does not show. In this run the "both read 0" outcome did not appear for any order, including relaxed, although the CPU permits it: the window is a few nanoseconds wide and depends on how tightly the two threads start together, which a VM with noisy neighbours does not give. **A test that fails to show a reordering proves nothing**; the model, not the experiment, defines what is allowed. Tools like `herd7` and `litmus7` enumerate the outcomes exhaustively, and ThreadSanitizer finds the race that makes such code incorrect.

## A spinlock from atomics

`atomic_flag` is the one type guaranteed lock-free. A test-and-set lock needs *acquire* on lock and *release* on unlock so that data protected by the lock is ordered correctly:

```c
// A test-and-set spinlock built from C11 atomics, used by 4 threads.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_flag lock = ATOMIC_FLAG_INIT;
static long counter;                       // protected by the spinlock: an ordinary variable

static void spin_lock(void) {
    while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire))
        ;                                  // production code adds a CPU pause hint and backoff
}
static void spin_unlock(void) { atomic_flag_clear_explicit(&lock, memory_order_release); }

static void *work(void *a) {
    (void)a;
    for (int i = 0; i < 500000; i++) { spin_lock(); counter++; spin_unlock(); }
    return NULL;
}
int main(void) {
    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < 4; i++) pthread_join(t[i], NULL);
    printf("counter = %ld (expected %d)\n", counter, 4 * 500000);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -pthread spinlock.c -o spinlock && ./spinlock
counter = 2000000 (expected 2000000)
```

Real spinlocks add a CPU pause hint (`pause` / `yield`), exponential backoff, and test-and-test-and-set (spin on a plain read before attempting the write to avoid bouncing the cache line). Use them only for very short critical sections and when the holder cannot be descheduled; otherwise a mutex (futex-based on Linux, which spins briefly then sleeps) is better.

## Finding races: ThreadSanitizer

`-fsanitize=thread` instruments every access and tracks happens-before; it reports races that *might* happen even if this run did not corrupt anything. Counting its reports for the two programs above:

```text
$ gcc -O1 -g -fsanitize=thread -pthread counter.c -o c_tsan && ./c_tsan 2>&1 | grep -c "WARNING: ThreadSanitizer: data race"
2
```

```text
$ gcc -O1 -g -fsanitize=thread -pthread mp.c -o mp_tsan && ./mp_tsan 2>&1 | grep -c "ThreadSanitizer"
0
```

The first reports races on the plain counter (the exact number depends on how many distinct access pairs it sees); the release/acquire program, which is correct, is clean (0 reports). TSan costs roughly 5-15x in time and 5-10x in memory, cannot be combined with ASan in the same binary, and only sees paths that actually execute, so run it in CI over your test suite.

## Traps beyond the basics

- **False sharing**: two threads writing different variables on the same 64-byte cache line make the line ping-pong between cores. Pad or align hot per-thread data (`_Alignas(64)`). A per-thread counter array with no padding can be many times slower than padded.
- **The ABA problem**: a lock-free pop reads head `A`, is preempted, others pop `A` and `B` and push `A` back; the CAS of head from `A` to `A->next` succeeds but `A->next` is stale. Fixes: tagged pointers (pointer plus a version counter, in a double-width CAS), hazard pointers, epoch-based reclamation, or just not freeing nodes while readers exist.
- **Memory reclamation** is the hard part of lock-free structures, not the CAS. If you do not need lock-freedom, use a mutex.
- **Lock-free is not wait-free**: lock-free guarantees system-wide progress; an individual thread may starve.
- **Signal handlers** may only use `volatile sig_atomic_t` or lock-free atomics.
- **Initialisation once**: use `pthread_once` or a function-local static guarded by a C11 `call_once`; double-checked locking without atomics is broken.
- **Thread-local storage** (`_Thread_local`, `__thread`) removes the need to synchronise at all; prefer per-thread data merged at the end over shared counters.
- **`_Atomic` struct** operations may fall back to a hidden lock; check `atomic_is_lock_free()` and link `-latomic` where needed.

## Key takeaways

- A data race is undefined behaviour. `volatile` does not fix it; mutexes, atomics and thread joins do.
- Every atomic operation has a memory order; default `seq_cst` is correct, `release`/`acquire` is the idiom for publishing data, `relaxed` is only for independent counters.
- x86-64's strong ordering hides missing barriers; the same code can break on ARM. Reason from the C11 model, not from the machine you tested on.
- Test absence of reordering proves nothing; use ThreadSanitizer for races and model checkers for litmus-level questions.
- Watch for false sharing, ABA and reclamation; prefer mutexes and per-thread data until measurements say otherwise.
