---
title: "Latency and Real-Time Tuning: PREEMPT_RT, SCHED_FIFO/DEADLINE, Isolation and Jitter"
track: linux
module: Performance and Observability
moduleOrder: 9
order: 4
level: expert
summary: "Where wake-up latency comes from (timer slack, scheduler, interrupts, SMIs, page faults), the real-time scheduling classes and priority inheritance, kernel preemption models and PREEMPT_RT, CPU and IRQ isolation, and a jitter-measurement tool whose results show timer slack and the scheduler's sleeper handling."
prerequisites:
  - "The Linux Scheduler: Classes, EEVDF/CFS, Priorities, Affinity and CPU Limits"
  - "Performance Methodology: USE, Load, CPU and Memory Analysis"
  - "Profiling and Tracing — perf, ftrace, Flame Graphs and Friends"
sources:
  - label: "Linux kernel documentation — Real-Time group scheduling and SCHED_FIFO/RR"
    url: https://docs.kernel.org/scheduler/sched-rt-group.html
  - label: "Linux kernel documentation — Deadline Task Scheduling (SCHED_DEADLINE)"
    url: https://docs.kernel.org/scheduler/sched-deadline.html
  - label: "Linux kernel documentation — Real-time Linux Analysis tools (rtla, osnoise, timerlat)"
    url: https://docs.kernel.org/tools/rtla/index.html
  - label: "Linux kernel documentation — The kernel's command-line parameters (isolcpus, nohz_full, rcu_nocbs, irqaffinity)"
    url: https://docs.kernel.org/admin-guide/kernel-parameters.html
  - label: "Linux kernel documentation — RT-mutex implementation design (priority inheritance)"
    url: https://docs.kernel.org/locking/rt-mutex-design.html
  - label: "Real-Time Linux wiki — PREEMPT_RT"
    url: https://wiki.linuxfoundation.org/realtime/start
  - label: "sched(7), sched_setscheduler(2), prctl(2) PR_SET_TIMERSLACK, mlockall(2), clock_nanosleep(2)"
    url: https://man7.org/linux/man-pages/man7/sched.7.html
---

A **real-time** system is one where *being late is a failure*: an audio buffer underrun, a robot control loop that misses its deadline, a trading engine's reaction time, a 5G fronthaul slot. Real-time does not mean fast. It means **bounded**: you care about the *worst case* latency (the tail), not the average. Standard Linux is tuned for throughput and fairness and can have milliseconds of occasional delay; with the right configuration it can bound latency to tens of microseconds, and with PREEMPT_RT to a few tens of microseconds on suitable hardware.

## Where latency comes from

When a thread should run at time *T* (a timer fires, a packet arrives), the delay until its first instruction executes is the sum of several parts:

| Source | Typical size | Controlled by |
|---|---|---|
| **Timer slack**: the kernel may delay a timer wake-up to coalesce it with others and save power | default 50 µs for normal threads | `prctl(PR_SET_TIMERSLACK)`; real-time threads have 0 |
| **Interrupt latency**: hardware raises the interrupt, the CPU is in a section with interrupts disabled | µs; can be ms with bad drivers | PREEMPT_RT (threaded IRQs), driver quality |
| **Hardware/firmware**: System Management Interrupts (SMIs) steal the CPU invisibly; deep C-states take time to exit; frequency scaling | 10 µs to ms | BIOS settings, `idle=poll`/C-state limits, `hwlat` tracer to detect |
| **Scheduling latency**: a higher-priority task, or the scheduler's choice of what runs next | µs to ms | scheduling class and priority, CPU isolation |
| **Kernel non-preemptible sections**: the CPU is inside kernel code that cannot be preempted (spinlock held, `preempt_disable`) | up to ms on stock kernels | preemption model, PREEMPT_RT |
| **Page faults and TLB misses**: first touch of memory, swapped or reclaimed pages | µs to ms | `mlockall`, pre-faulting, huge pages |
| **Contention**: a lock held by a lower-priority thread (priority inversion) | unbounded without inheritance | PI mutexes |
| **Cache and memory effects**: another core thrashes shared caches/memory bandwidth | µs | cache partitioning (resctrl), isolation |

The right first step is always to **measure**, not to tune.

## Measuring: a cyclictest-style tool

The program below sleeps until an absolute time every millisecond and records how late it actually wakes, then prints the distribution. This is what `cyclictest` (from the rt-tests package) does; here it is in 40 lines so every option is visible. `--slack` sets the timer slack, `--cpu` pins the thread, `--fifo` requests `SCHED_FIFO`, `--mlock` locks all memory.

```c
// Wake-up latency: sleep until an absolute time every 1 ms and record how late the thread actually runs (like cyclictest).
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <time.h>
#include <errno.h>

#define N 3000
static int cmp(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return (x > y) - (x < y); }

int main(int argc, char **argv) {
    int slack_ns = -1, fifo = 0, lock = 0, cpu = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--slack")) slack_ns = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fifo")) fifo = 1;
        else if (!strcmp(argv[i], "--mlock")) lock = 1;
        else if (!strcmp(argv[i], "--cpu")) cpu = atoi(argv[++i]);
    }
    if (slack_ns >= 0) prctl(PR_SET_TIMERSLACK, slack_ns);
    if (cpu >= 0) { cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s); sched_setaffinity(0, sizeof s, &s); }
    if (lock && mlockall(MCL_CURRENT | MCL_FUTURE) != 0) printf("mlockall: %s\n", strerror(errno));
    if (fifo) { struct sched_param p = { .sched_priority = 50 }; if (sched_setscheduler(0, SCHED_FIFO, &p) != 0) printf("SCHED_FIFO refused: %s\n", strerror(errno)); }
    static long late[N];
    struct timespec next; clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < N; i++) {
        next.tv_nsec += 1000000; if (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        late[i] = (now.tv_sec - next.tv_sec) * 1000000000L + (now.tv_nsec - next.tv_nsec);
    }
    qsort(late, N, sizeof *late, cmp);
    printf("wake-up latency over %d wakeups (us): min %ld  median %ld  p99 %ld  max %ld\n", N, late[0] / 1000, late[N / 2] / 1000, late[N * 99 / 100] / 1000, late[N - 1] / 1000);
    return 0;
}
```

```bash
#!/bin/bash
# Wake-up latency of a 1 ms periodic thread, measured four ways. Numbers depend on the machine; compare the lines with each other.
echo "== default timer slack: $(cat /proc/self/timerslack_ns) ns =="
echo "idle machine:";               ./jitter
echo "timer slack set to 1 ns:";    ./jitter --slack 1
echo "SCHED_FIFO request (needs root or CAP_SYS_NICE, or an RLIMIT_RTPRIO allowance):"; ./jitter --fifo --mlock
echo; echo "== eight CPU hogs competing on all CPUs =="
pids=""; for i in $(seq 1 $(( $(nproc) )) ); do (while :; do :; done) & pids="$pids $!"; done
sleep 0.5
echo "default scheduling, any CPU:";                 ./jitter
echo "timer slack 1 ns, any CPU:";                   ./jitter --slack 1
echo "CPU 0 shared with a hog, pinned to CPU 0:";    taskset -c 0 ./jitter --cpu 0
kill $pids 2>/dev/null; wait 2>/dev/null
echo; echo "== real-time scheduling limits of this account =="
ulimit -r | sed 's/^/RLIMIT_RTPRIO: /'
```

```text
$ ./lab.sh 2>&1
== default timer slack: 50000 ns ==
idle machine:
wake-up latency over 3000 wakeups (us): min 27  median 74  p99 144  max 304
timer slack set to 1 ns:
wake-up latency over 3000 wakeups (us): min 13  median 21  p99 88  max 1819
SCHED_FIFO request (needs root or CAP_SYS_NICE, or an RLIMIT_RTPRIO allowance):
SCHED_FIFO refused: Operation not permitted
wake-up latency over 3000 wakeups (us): min 27  median 74  p99 265  max 6265

== eight CPU hogs competing on all CPUs ==
default scheduling, any CPU:
wake-up latency over 3000 wakeups (us): min 28  median 59  p99 86  max 4470
timer slack 1 ns, any CPU:
wake-up latency over 3000 wakeups (us): min 3  median 4  p99 9  max 5480
CPU 0 shared with a hog, pinned to CPU 0:
wake-up latency over 3000 wakeups (us): min 8  median 57  p99 87  max 2133

== real-time scheduling limits of this account ==
RLIMIT_RTPRIO: 0
```

These results come from an ordinary unprivileged user on a shared virtual machine, so absolute numbers say little. What they do show, and what repeated runs confirm:

- **Timer slack dominates the typical case.** With the default 50 µs slack the median wake-up is about 60-75 µs late; with the slack set to 1 ns the median falls to 10-25 µs. The kernel is allowed to delay the wake-up by up to the slack so that several timers can share one interrupt, and unless you ask otherwise a normal thread gets that delay. Real-time threads have zero slack automatically.
- **The tail is a different story.** The maximum (and often p99) is orders of magnitude larger than the median: a few wake-ups were delayed by several milliseconds. On a virtual machine that is partly the hypervisor descheduling the virtual CPU, which no setting inside the guest can fix. This is why real-time work uses the maximum, never the average, and why the test must run for hours on the target hardware.
- **CPU hogs did not break the periodic thread** in these runs: the scheduler (EEVDF in recent kernels, CFS before) favors threads that sleep and wake, so a task that runs for a few microseconds every millisecond preempts CPU-bound tasks quickly. Fair scheduling gives *good typical* latency to light tasks, but offers **no bound**; a worst case can still be milliseconds, visible in the maximum column.
- **`SCHED_FIFO` was refused** (`EPERM`, `RLIMIT_RTPRIO` is 0) because real-time priorities need `CAP_SYS_NICE` or an explicit allowance (`/etc/security/limits.conf`, `LimitRTPRIO=` in systemd units). Because it is dangerous (a runaway FIFO task can starve the whole CPU), it is gated.

## Real-time scheduling classes

| Class | Policy | Behavior |
|---|---|---|
| Real-time | `SCHED_FIFO` (priority 1-99) | runs until it blocks, yields or is preempted by a higher priority; **no time slice** |
| Real-time | `SCHED_RR` | like FIFO but equal priorities take turns (time slice, default 100 ms) |
| Deadline | `SCHED_DEADLINE` | each task declares *runtime*, *deadline*, *period* (EDF + CBS); the kernel does admission control and rejects task sets that cannot be guaranteed |
| Fair | `SCHED_OTHER`/`NORMAL`, `BATCH`, `IDLE` | the default time-sharing class (EEVDF) |

Real-time tasks always beat fair tasks. Within real time, `SCHED_DEADLINE` beats FIFO/RR. Guard rails: **RT throttling** (`/proc/sys/kernel/sched_rt_runtime_us` defaults to 950000 of every 1000000 µs) limits real-time tasks to 95% of each CPU so a runaway cannot lock out the system (set to -1 only on isolated CPUs you control).

```bash
chrt -f 50 ./control_loop            # run under SCHED_FIFO, priority 50
chrt -p $(pidof control_loop)        # inspect
chrt -d --sched-runtime 200000 --sched-deadline 1000000 --sched-period 1000000 0 ./task   # SCHED_DEADLINE: 0.2 ms every 1 ms
```

### Priority inversion

A low-priority task *L* holds a lock that high-priority task *H* needs; a medium-priority task *M* preempts *L*, so *H* waits indefinitely for *M*, which has nothing to do with it. (This bug famously froze the Mars Pathfinder lander in 1997.) The cure is **priority inheritance**: while *H* waits for the lock, *L* runs at *H*'s priority. In user space, request it per mutex:

```c
pthread_mutexattr_t a; pthread_mutexattr_init(&a);
pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_INHERIT);   // backed by futex PI (FUTEX_LOCK_PI) in the kernel
pthread_mutex_init(&m, &a);
```

In the kernel, `rt_mutex` provides the same; with PREEMPT_RT most spinlocks become rt_mutexes.

## Kernel preemption models and PREEMPT_RT

| Model | Behavior |
|---|---|
| `PREEMPT_NONE` | kernel code is preempted only when it blocks (server throughput) |
| `PREEMPT_VOLUNTARY` | explicit preemption points added |
| `PREEMPT` | the kernel is preemptible anywhere except in critical sections (desktop) |
| `PREEMPT_DYNAMIC` | choose among the above at boot with `preempt=` |
| `PREEMPT_RT` | spinlocks become sleeping locks with priority inheritance, interrupt handlers run as schedulable threads, softirqs are threaded, almost all of the kernel is preemptible. Merged in mainline in 6.12 |

PREEMPT_RT trades some throughput for a latency bound. It does not remove the need to tune hardware (SMIs, C-states) or to keep the real-time thread out of paths that block unpredictably (disk I/O, page faults, `malloc` in the loop).

## Isolating CPUs and interrupts

The most reliable way to keep a critical thread undisturbed is to give it a CPU nobody else uses:

- `isolcpus=` (or better, cpuset `isolated` partitions) removes CPUs from the general scheduler domain.
- `nohz_full=` stops the periodic scheduler tick on a CPU that runs one task (adaptive ticks); `rcu_nocbs=` moves RCU callback processing to other CPUs; `irqaffinity=` and `/proc/irq/*/smp_affinity` keep device interrupts off the isolated CPUs.
- `systemd` `CPUAffinity=` and cgroup `cpuset.cpus` keep ordinary services off them.
- Disable or limit idle states and frequency scaling on those CPUs when the exit latency is too large; on a laptop this costs battery.
- `tuned` profiles (`realtime`, `cpu-partitioning`) package much of this.

Check what remains: `rtla osnoise top` reports interference per CPU (IRQ, softirq, thread, and non-preemptible noise) while a spin loop runs on that CPU, and `rtla timerlat top` reports timer latency broken into IRQ and thread latency with the stack that caused the worst case. `hwlat` (ftrace tracer) detects hardware or firmware stalls. These tools are part of the kernel tree and need root.

## Application practice

- **Lock memory**: `mlockall(MCL_CURRENT | MCL_FUTURE)` and pre-touch stacks and heaps at start-up so the real-time path takes no page faults.
- **No allocation, no blocking I/O, no unbounded loops** in the real-time thread; use pre-allocated pools, lock-free queues, and a separate non-real-time thread for logging.
- **Use absolute-time sleeps** (`clock_nanosleep` with `TIMER_ABSTIME`, as above) to avoid cumulative drift.
- **Avoid `fork`, `system`, library calls with hidden locks** and anything that touches the filesystem in the loop.
- **Keep the system quiet**: disable transparent huge page compaction effects (`khugepaged`), stop unrelated timers, pin interrupts.
- **Test under load**: run the target workload plus stress (`stress-ng`, network, disk) for hours and look at the maximum, not the median.

## Key takeaways

- Real-time means a bounded worst case, not speed; measure the maximum over a long run on the target hardware.
- Typical wake-up latency has a floor from timer slack (50 µs default for normal threads) and a tail from interrupts, firmware, non-preemptible kernel code, page faults and contention.
- `SCHED_FIFO`/`RR` and `SCHED_DEADLINE` give priority and guarantees but are privileged, can starve the system, and need priority-inheriting locks.
- PREEMPT_RT makes nearly the entire kernel preemptible; CPU/IRQ isolation, locked memory and a quiet real-time thread complete the picture.
- Verify with `cyclictest`-style measurements and `rtla osnoise/timerlat`, and treat the hypervisor as part of the latency budget.
