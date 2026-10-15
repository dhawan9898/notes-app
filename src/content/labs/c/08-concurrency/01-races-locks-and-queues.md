---
title: Races, Locks and a Blocking Queue
track: c
module: Concurrency in C
moduleOrder: 8
order: 1
level: expert
minutes: 45
summary: "Four exercises on the concurrency module: showing a data race and fixing it with a mutex and with an atomic, detecting it with ThreadSanitizer, building a bounded producer-consumer queue with a condition variable, and reproducing and avoiding a lock-order deadlock, with verified programs and reports."
prerequisites:
  - "Threads, Atomics and the C11 Memory Model"
sources:
  - label: "ISO/IEC 9899:2018 §7.26 (Threads, optional), §7.17 (Atomics), §5.1.2.4 (Multi-threaded executions and data races)"
  - label: "POSIX.1-2017 — pthread_mutex_lock(), pthread_cond_wait(), pthread_mutex_timedlock()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_cond_wait.html
  - label: "ThreadSanitizer documentation"
    url: https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual
  - label: "Butenhof — Programming with POSIX Threads (Addison-Wesley, 1997), Ch. 3-4"
---

Concurrency bugs are the hardest class to find by reading code, so these exercises are experimental: you will provoke each failure on purpose and then remove it. Build with `gcc -std=gnu17 -O1 -Wall -Wextra -pthread`.

## Exercises

**1. Provoke a race.** Four threads each increment three shared counters 200,000 times: one plain `long`, one protected by a mutex, one `atomic_long`. Predict the three final values. Run it; which counter is wrong, and why is the result different on every run?

**2. Let ThreadSanitizer find it.** Rebuild with `-fsanitize=thread -g`. What does the report say, and what are the two accesses it points at? What does the mutex or atomic do to the report?

**3. A bounded queue.** Write `put` and `get` for a 4-slot buffer shared by a producer thread and a consumer thread, using one mutex and two condition variables (`not_full`, `not_empty`). Pass the squares of 1 to 12 through it and check the sum (650). Why must `pthread_cond_wait` always be inside a `while` loop, not an `if`?

**4. Deadlock.** Two threads lock mutexes A and B, one in the order A then B, the other B then A. Show the deadlock safely (without hanging the program), then state the rule that prevents it.

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define THREADS 4
#define ITERS 200000

static long plain_counter;                       /* shared, unprotected */
static long locked_counter;                      /* protected by a mutex */
static atomic_long atomic_counter;               /* updated with atomic instructions */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *work(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        plain_counter++;                         /* data race: load, add, store can interleave between threads */
        pthread_mutex_lock(&lock); locked_counter++; pthread_mutex_unlock(&lock);
        atomic_fetch_add(&atomic_counter, 1);
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    long expected = (long)THREADS * ITERS;
    printf("expected      %ld\n", expected);
    printf("mutex         %ld %s\n", locked_counter, locked_counter == expected ? "(correct)" : "(WRONG)");
    printf("atomic        %ld %s\n", (long)atomic_counter, atomic_counter == expected ? "(correct)" : "(WRONG)");
    printf("unprotected   %ld %s\n", plain_counter, plain_counter == expected ? "(correct by luck this time)" : "(WRONG: updates were lost; the number differs on every run)");
    return 0;
}
```

```text
$ gcc -std=gnu17 -O1 -Wall -Wextra -pthread -o p8_race p8_race.c && ./p8_race; rm -f p8_race
expected      800000
mutex         800000 (correct)
atomic        800000 (correct)
unprotected   799948 (WRONG: updates were lost; the number differs on every run)
```

The mutex and atomic counters reach exactly 800,000. The plain counter is **short** (the run above shows how many updates were lost, and the number differs every run). `plain_counter++` is really three steps: load, add one, store. Two threads can load the *same* value, both add one and both store it, so one increment vanishes. The loss depends on exact timing, which is why the number is different each time, why a test can pass by luck, and why races surface under load in production. In C a data race is **undefined behaviour**, not merely a wrong count: the compiler may also assume there is none. The **mutex** makes the three steps one critical section; the **atomic** does the read-modify-write as a single indivisible hardware instruction (cheaper than a mutex for a single counter).

**2.**

```text
$ gcc -std=gnu17 -g -fsanitize=thread -pthread -o p8_tsan p8_race.c && ./p8_tsan 2>&1 | grep -E "WARNING|Write of|Previous write|SUMMARY" | sed -e 's/==[0-9]*==//' -e 's#/[^ ]*/##g' -e 's/0x[0-9a-f]\{6,\}/ADDR/g' -e 's/pid=[0-9]*/pid=N/' | head -4; rm -f p8_tsan
WARNING: ThreadSanitizer: data race (pid=N)
  Previous write of size 8 at ADDR by thread T1:
SUMMARY: ThreadSanitizer: data race p8_race.c:16 in work
WARNING: ThreadSanitizer: data race (pid=N)
```

ThreadSanitizer reports a **data race** at the line `plain_counter++` (line 16): a *write* in one thread and a *previous write* in another with no synchronisation between them. It stays silent about the mutex-protected and atomic counters because those accesses are ordered. TSan watches every memory access at run time (about 5 to 15 times slower), finds races even when the program happened to print the right answer, and reports the two stacks; it needs the program to actually execute the racing code. Use it in CI on your concurrent tests.

**3.**

```c
#include <pthread.h>
#include <stdio.h>

#define CAP 4
#define ITEMS 12

static int buf[CAP], head, count;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER, not_empty = PTHREAD_COND_INITIALIZER;

static void put(int v) {
    pthread_mutex_lock(&m);
    while (count == CAP) pthread_cond_wait(&not_full, &m);        /* always re-check in a loop: wakeups can be spurious */
    buf[(head + count) % CAP] = v; count++;
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&m);
}
static int get(void) {
    pthread_mutex_lock(&m);
    while (count == 0) pthread_cond_wait(&not_empty, &m);
    int v = buf[head]; head = (head + 1) % CAP; count--;
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&m);
    return v;
}

static void *producer(void *arg) { (void)arg; for (int i = 1; i <= ITEMS; i++) put(i * i); return NULL; }

int main(void) {
    pthread_t p;
    pthread_create(&p, NULL, producer, NULL);
    long sum = 0;
    for (int i = 0; i < ITEMS; i++) sum += get();
    pthread_join(p, NULL);
    printf("consumed %d items through a %d-slot buffer, sum of squares = %ld (expected 650)\n", ITEMS, CAP, sum);
    return 0;
}
```

```text
$ gcc -std=gnu17 -O1 -Wall -Wextra -pthread -o p8_queue p8_queue.c && ./p8_queue; rm -f p8_queue
consumed 12 items through a 4-slot buffer, sum of squares = 650 (expected 650)
```

The mutex protects the shared buffer state (`buf`, `head`, `count`). `put` waits on `not_full` while the buffer is full; `get` waits on `not_empty` while it is empty; each signals the other side after changing the state. `pthread_cond_wait` atomically **releases the mutex and sleeps**, then re-acquires it before returning. It must be in a **`while` loop** for two reasons: wakeups can be **spurious** (the standard allows a return with no signal), and between the signal and your thread actually running, *another* thread may have taken the item, so the condition may be false again. An `if` would then read from an empty buffer. The sum 650 (1² + ... + 12²) confirms nothing was lost or duplicated, and the producer was repeatedly blocked because 12 items cannot fit in 4 slots.

**4.**

```c
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER, B = PTHREAD_MUTEX_INITIALIZER;

static int timed_lock(pthread_mutex_t *m) {            /* give up after 300 ms instead of hanging forever */
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += 300 * 1000 * 1000;
    if (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
    return pthread_mutex_timedlock(m, &t);
}

static void *first_takes_a(void *arg) {                /* order A then B */
    (void)arg;
    pthread_mutex_lock(&A); usleep(100000);            /* let the other thread grab B */
    int rc = timed_lock(&B);
    printf("thread 1 holds A, wants B: %s\n", rc == ETIMEDOUT ? "TIMED OUT (this is a deadlock)" : "got it");
    if (rc == 0) pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}
static void *second_takes_b(void *arg) {               /* the opposite order: B then A */
    (void)arg;
    pthread_mutex_lock(&B); usleep(100000);
    int rc = timed_lock(&A);
    printf("thread 2 holds B, wants A: %s\n", rc == ETIMEDOUT ? "TIMED OUT (this is a deadlock)" : "got it");
    if (rc == 0) pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, first_takes_a, NULL); pthread_create(&t2, NULL, second_takes_b, NULL);
    pthread_join(t1, NULL); pthread_join(t2, NULL);
    return 0;
}
```

```text
$ gcc -std=gnu17 -Wall -Wextra -pthread -o p8_deadlock_demo p8_deadlock_demo.c && ./p8_deadlock_demo | sort; rm -f p8_deadlock_demo
thread 1 holds A, wants B: TIMED OUT (this is a deadlock)
thread 2 holds B, wants A: TIMED OUT (this is a deadlock)
```

Each thread holds one lock and waits for the one the other holds: neither can proceed. A normal `pthread_mutex_lock` would **hang forever**; the demo uses `pthread_mutex_timedlock` so that both threads give up after 300 ms and report it (never use timeouts as the fix, only as a diagnostic or a last-resort recovery). The four conditions for deadlock are mutual exclusion, hold-and-wait, no preemption and circular wait; **breaking circular wait** is the practical cure: define one global **lock order** (here, always A before B) and take locks in that order everywhere. A sample ordered version, `p8_deadlock.c`, does exactly that:

```c
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER, B = PTHREAD_MUTEX_INITIALIZER;

static void *ordered(void *arg) {                      /* both threads take A then B: a global lock order */
    (void)arg;
    pthread_mutex_lock(&A); usleep(1000); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, ordered, NULL); pthread_create(&t2, NULL, ordered, NULL);
    pthread_join(t1, NULL); pthread_join(t2, NULL);
    printf("both threads finished: a fixed lock order (A before B) cannot deadlock\n");
    return 0;
}
```

```text
$ gcc -std=gnu17 -Wall -Wextra -pthread -o p8_deadlock p8_deadlock.c && ./p8_deadlock; rm -f p8_deadlock
both threads finished: a fixed lock order (A before B) cannot deadlock
```

Other defences: hold fewer locks at once, never call unknown code (callbacks) while holding a lock, use `pthread_mutex_trylock` and back off, or avoid shared state altogether by passing messages. TSan also reports **lock-order inversions** as potential deadlocks even when no hang occurred.

</details>

## Key takeaways

1. `counter++` is load, add, store; without a mutex or an atomic, threads lose updates, and in C an unsynchronised race is undefined behaviour.
2. ThreadSanitizer finds races that did not visibly misbehave; run it on concurrent tests.
3. Wait on a condition variable in a `while` loop because of spurious wakeups and competing consumers.
4. Prevent deadlock by a global lock order, fewer nested locks and no callbacks under a lock.
