---
title: "Practice: Races, Locks and a Blocking Queue"
track: c
module: Concurrency in C
moduleOrder: 8
order: 2
level: expert
summary: "Four exercises on the concurrency module: showing a data race and fixing it with a mutex and with an atomic, detecting it with ThreadSanitizer, building a bounded producer-consumer queue with a condition variable, and reproducing and avoiding a lock-order deadlock, with verified programs and reports."
prerequisites:
  - "Threads, Atomics and the C11 Memory Model"
sources:
  - label: ISO/IEC 9899:2018 §7.26 (Threads, optional), §7.17 (Atomics), §5.1.2.4 (Multi-threaded executions and data races)
  - label: "POSIX.1-2017 — pthread_mutex_lock(), pthread_cond_wait(), pthread_mutex_timedlock()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_cond_wait.html
  - label: ThreadSanitizer documentation
    url: https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual
  - label: Butenhof — Programming with POSIX Threads (Addison-Wesley, 1997), Ch. 3-4
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

{{include c/practice/p8_race.c}}

{{run c/practice :: gcc -std=gnu17 -O1 -Wall -Wextra -pthread -o p8_race p8_race.c && ./p8_race; rm -f p8_race}}

The mutex and atomic counters reach exactly 800,000. The plain counter is **short** (the run above shows how many updates were lost, and the number differs every run). `plain_counter++` is really three steps: load, add one, store. Two threads can load the *same* value, both add one and both store it, so one increment vanishes. The loss depends on exact timing, which is why the number is different each time, why a test can pass by luck, and why races surface under load in production. In C a data race is **undefined behaviour**, not merely a wrong count: the compiler may also assume there is none. The **mutex** makes the three steps one critical section; the **atomic** does the read-modify-write as a single indivisible hardware instruction (cheaper than a mutex for a single counter).

**2.**

{{run c/practice :: gcc -std=gnu17 -g -fsanitize=thread -pthread -o p8_tsan p8_race.c && ./p8_tsan 2>&1 | grep -E "WARNING|Write of|Previous write|SUMMARY" | sed -e 's/==[0-9]*==//' -e 's#/[^ ]*/##g' -e 's/0x[0-9a-f]\{6,\}/ADDR/g' -e 's/pid=[0-9]*/pid=N/' | head -4; rm -f p8_tsan}}

ThreadSanitizer reports a **data race** at the line `plain_counter++` (line 16): a *write* in one thread and a *previous write* in another with no synchronisation between them. It stays silent about the mutex-protected and atomic counters because those accesses are ordered. TSan watches every memory access at run time (about 5 to 15 times slower), finds races even when the program happened to print the right answer, and reports the two stacks; it needs the program to actually execute the racing code. Use it in CI on your concurrent tests.

**3.**

{{include c/practice/p8_queue.c}}

{{run c/practice :: gcc -std=gnu17 -O1 -Wall -Wextra -pthread -o p8_queue p8_queue.c && ./p8_queue; rm -f p8_queue}}

The mutex protects the shared buffer state (`buf`, `head`, `count`). `put` waits on `not_full` while the buffer is full; `get` waits on `not_empty` while it is empty; each signals the other side after changing the state. `pthread_cond_wait` atomically **releases the mutex and sleeps**, then re-acquires it before returning. It must be in a **`while` loop** for two reasons: wakeups can be **spurious** (the standard allows a return with no signal), and between the signal and your thread actually running, *another* thread may have taken the item, so the condition may be false again. An `if` would then read from an empty buffer. The sum 650 (1² + ... + 12²) confirms nothing was lost or duplicated, and the producer was repeatedly blocked because 12 items cannot fit in 4 slots.

**4.**

{{include c/practice/p8_deadlock_demo.c}}

{{run c/practice :: gcc -std=gnu17 -Wall -Wextra -pthread -o p8_deadlock_demo p8_deadlock_demo.c && ./p8_deadlock_demo | sort; rm -f p8_deadlock_demo}}

Each thread holds one lock and waits for the one the other holds: neither can proceed. A normal `pthread_mutex_lock` would **hang forever**; the demo uses `pthread_mutex_timedlock` so that both threads give up after 300 ms and report it (never use timeouts as the fix, only as a diagnostic or a last-resort recovery). The four conditions for deadlock are mutual exclusion, hold-and-wait, no preemption and circular wait; **breaking circular wait** is the practical cure: define one global **lock order** (here, always A before B) and take locks in that order everywhere. A sample ordered version, `p8_deadlock.c`, does exactly that:

{{include c/practice/p8_deadlock.c}}

{{run c/practice :: gcc -std=gnu17 -Wall -Wextra -pthread -o p8_deadlock p8_deadlock.c && ./p8_deadlock; rm -f p8_deadlock}}

Other defences: hold fewer locks at once, never call unknown code (callbacks) while holding a lock, use `pthread_mutex_trylock` and back off, or avoid shared state altogether by passing messages. TSan also reports **lock-order inversions** as potential deadlocks even when no hang occurred.

</details>

## Key takeaways

1. `counter++` is load, add, store; without a mutex or an atomic, threads lose updates, and in C an unsynchronised race is undefined behaviour.
2. ThreadSanitizer finds races that did not visibly misbehave; run it on concurrent tests.
3. Wait on a condition variable in a `while` loop because of spurious wakeups and competing consumers.
4. Prevent deadlock by a global lock order, fewer nested locks and no callbacks under a lock.
