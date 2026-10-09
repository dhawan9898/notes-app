---
title: "The Linux Scheduler: Classes, EEVDF/CFS, Priorities, Affinity and CPU Limits"
track: linux
module: Processes
moduleOrder: 3
order: 2
level: advanced
summary: How the kernel decides which task runs where and for how long — scheduling classes and policies, fair scheduling with virtual runtime, nice and weights, real-time and deadline tasks, load balancing, affinity, cgroup CPU control and what load average really means.
sources:
  - label: "Linux kernel docs — Scheduler (CFS, EEVDF, sched-design-CFS, sched-rt-group, sched-deadline)"
    url: https://docs.kernel.org/scheduler/index.html
  - label: "man 7 sched, man 2 sched_setscheduler, man 2 sched_setattr, man 1 chrt taskset nice ionice"
    url: https://man7.org/linux/man-pages/man7/sched.7.html
  - label: "Stoica & Abdel-Wahab — Earliest Eligible Virtual Deadline First (1995)"
  - label: "Lozi et al. — The Linux Scheduler - a Decade of Wasted Cores (EuroSys 2016)"
  - label: "Brendan Gregg — Linux Load Averages - Solving the Mystery"
    url: https://www.brendangregg.com/blog/2017-08-08/linux-load-averages.html
  - label: "Linux kernel docs — PSI (Pressure Stall Information)"
    url: https://docs.kernel.org/accounting/psi.html
---

The scheduler multiplexes runnable tasks (threads) onto CPUs. Its goals conflict: **throughput**, **fairness**, **low latency** for interactive tasks, **determinism** for real-time tasks and **energy efficiency**. Linux solves this with *scheduling classes* tried in strict order.

## Scheduling classes and policies

The kernel checks classes from highest to lowest and runs a task from the first class that has one runnable:

| Class | Policies | Priority | Typical use |
|---|---|---|---|
| **stop** | (internal) | highest | CPU migration, stop_machine |
| **deadline** | `SCHED_DEADLINE` | by earliest deadline | hard-ish real-time with guarantees (runtime/deadline/period) |
| **real-time** | `SCHED_FIFO`, `SCHED_RR` | 1–99 (higher wins) | audio, control loops, driver threads |
| **fair** | `SCHED_OTHER` (aka `SCHED_NORMAL`), `SCHED_BATCH` | nice −20…19 | everything normal |
| **idle** | `SCHED_IDLE` | lowest | background crumbs |

```bash
$ chrt -m
SCHED_OTHER min/max priority	: 0/0
SCHED_FIFO min/max priority	: 1/99
SCHED_RR min/max priority	: 1/99
SCHED_BATCH min/max priority	: 0/0
SCHED_IDLE min/max priority	: 0/0
SCHED_DEADLINE min/max priority	: 0/0
$ chrt -p $$
pid 2303093's current scheduling policy: SCHED_OTHER
```

- **`SCHED_FIFO`**: runs until it blocks, yields or is preempted by a higher-priority RT task. **No time slicing among equals**. A busy loop at RT priority starves all normal tasks on that CPU.
- **`SCHED_RR`**: same, but equal-priority tasks round-robin with a time slice.
- **`SCHED_DEADLINE`**: each task declares *runtime* within *deadline* every *period* (`chrt -d --sched-runtime 5000000 --sched-deadline 10000000 --sched-period 16666666 0 cmd`); admission control rejects over-commit; the earliest absolute deadline runs first (EDF).
- **`SCHED_BATCH`**: for CPU-bound jobs; fewer wakeup preemptions. **`SCHED_IDLE`**: runs only when nothing else wants the CPU.

**RT safety valve**: `sched_rt_runtime_us / sched_rt_period_us` = `950000 / 1000000` reserves 5 % of CPU time for non-RT tasks so a runaway RT task can't lock up the machine (set `-1` to disable, for dedicated RT systems with care).

## The fair class

### Weights and nice

Each fair task has a **weight** derived from its nice value (−20 … +19); nice 0 = weight 1024, and each step changes the weight by ≈ 25 % (nice −20 = 88761, +19 = 15). With only two competing CPU-bound tasks, CPU share = weight / sum of weights: nice 0 vs nice 5 (weight 335) gives ≈ 75 % / 25 %. Unprivileged users can only raise nice (be nicer); lowering needs `CAP_SYS_NICE`/`RLIMIT_NICE`.

```bash
nice -n 10 make -j8         # start with lower priority
renice -n 5 -p 1234         # change a running process
ionice -c 2 -n 7 rsync ...  # I/O priority: classes 1 realtime, 2 best-effort (0-7), 3 idle
```

### CFS (until Linux 6.5)

The Completely Fair Scheduler tracked **virtual runtime** (`vruntime`): actual CPU time scaled inversely by weight. Tasks sit in a red-black tree ordered by vruntime; the scheduler runs the *leftmost* (least-served) task. A task's slice depended on `sched_latency_ns` split by weight, bounded by `sched_min_granularity_ns`. Newly woken tasks received a vruntime close to the queue minimum (with a bonus) so sleepers got prompt service.

### EEVDF (Linux 6.6 and later)

Replaced CFS's heuristics with a principled algorithm: **Earliest Eligible Virtual Deadline First**.

- Each task has a *lag*: how much service it is owed versus what an ideal fair scheduler would have given. A task is **eligible** if its lag ≥ 0 (it hasn't consumed more than its share).
- Each task also gets a **virtual deadline** = eligible time + (requested slice ÷ weight).
- The scheduler picks the **eligible task with the earliest virtual deadline**. Tasks that ask for *shorter slices* (via `sched_setattr`'s `sched_runtime` / "latency-nice") get earlier deadlines → lower wakeup latency without stealing long-run share.

This gives bounded lag (provable fairness) and makes latency a first-class parameter. Tunables moved to debugfs (`/sys/kernel/debug/sched/base_slice_ns`, a few ms). (For example, a 6.12 kernel uses EEVDF.)

### Preemption and wakeups

A task runs until it blocks, its slice expires (checked on the timer tick, `CONFIG_HZ` 100–1000, or tickless `NO_HZ`), or a woken task deserves to preempt it. Kernel preemption models (`CONFIG_PREEMPT_NONE` / `VOLUNTARY` / `PREEMPT` / `PREEMPT_RT`) set how soon the *kernel* itself can be preempted: server builds trade latency for throughput; `PREEMPT_RT` (merged mainline in 6.12) makes almost all kernel code preemptible with PI mutexes for hard real-time.

Context switches show in `/proc/PID/status`: `voluntary_ctxt_switches` (blocked on I/O or a lock: normal for I/O-bound) vs `nonvoluntary_ctxt_switches` (preempted: CPU contention).

## Multi-core: runqueues and load balancing

Each CPU has its own **runqueue** (avoids a global lock). Balancing happens by:

- **Wakeup placement** (`select_task_rq`): prefer the previous CPU (cache warm) or an idle sibling sharing cache (LLC).
- **Periodic and idle balancing**: walk **scheduling domains** mirroring the hardware topology: SMT siblings → cores sharing LLC → sockets → NUMA nodes; migrate tasks from busier to idler groups, with costs rising at each level (cache, NUMA memory distance).
- **NUMA balancing** (`kernel.numa_balancing`) moves tasks and pages together.
- The paper *"A Decade of Wasted Cores"* documented bugs where cores idled while tasks waited: topology awareness is delicate.

### CPU affinity and isolation

```bash
taskset -p $$                 # current mask (here: ff = CPUs 0-7)
taskset -c 2,3 ./app          # pin to CPUs 2 and 3
chrt -f 50 taskset -c 3 ./rt  # SCHED_FIFO prio 50 on CPU 3
numactl --cpunodebind=0 --membind=0 ./db     # keep CPU and memory on one NUMA node
```

- `cpuset` cgroup restricts CPUs/memory nodes per group; `isolcpus=`, `nohz_full=`, `rcu_nocbs=` (boot parameters) keep housekeeping noise off dedicated cores for latency-critical workloads; IRQ affinity (`/proc/irq/N/smp_affinity`) steers interrupts.
- Pinning removes balancing flexibility: pin only when measured benefits exist (cache locality, NUMA, determinism).
- Hyper-threads share a core's execution units: siblings' loads interact (`lscpu -e`).

## cgroup CPU control (v2)

| File | Meaning |
|---|---|
| `cpu.weight` (1–10000, default 100) | **proportional** share among siblings when CPUs are contended |
| `cpu.max` (`"quota period"`, e.g. `50000 100000`) | **hard cap**: 50 ms of CPU per 100 ms period (= 0.5 CPU); exceeding throttles the group |
| `cpu.stat` | `nr_throttled`, `throttled_usec`: evidence of quota throttling |
| `cpu.pressure` | PSI for the group |

Container platforms translate Kubernetes `requests` → `cpu.weight` and `limits` → `cpu.max`. **Quota throttling hurts latency** (a multithreaded service can burn its quota in the first few ms of each period, then stall for the rest): prefer requests/weights, or raise the limit and period, for latency-sensitive services. `systemd-run -p CPUWeight=200 -p CPUQuota=50% cmd` applies the same controls.

Also `autogroup` (`/proc/sys/kernel/sched_autogroup_enabled`) groups tasks per session so a `make -j` doesn't swamp interactive apps.

## Observing the scheduler

| Metric | Where | Interpretation |
|---|---|---|
| **Run queue length** (`r` in `vmstat 1`) | `vmstat`, `/proc/stat` | tasks wanting CPU; consistently > CPU count ⇒ saturation |
| **Load average** | `/proc/loadavg`, `uptime` | see below |
| **CPU utilization** per mode | `mpstat -P ALL 1`, `top` | `us` user, `sy` kernel, `wa` iowait (idle with pending I/O), `st` steal (hypervisor took CPU), `si` softirq |
| **Scheduling latency** | `perf sched latency`, `runqlat` (bcc), `schedstat` | time waiting on a runqueue |
| **Pressure (PSI)** | `/proc/pressure/cpu` | `some avg10=…`: share of time tasks stalled for CPU |
| Per-task | `pidstat -w -u 1`, `/proc/PID/sched` | switches, run delay |

### Load average

The three numbers in `/proc/loadavg` (`4.52 4.50 4.51 5/1199 2303097`) are 1-, 5-, 15-minute **exponentially damped averages of the number of tasks that are runnable (R) or in uninterruptible sleep (D)**, sampled every ~5 s; then `running/total` tasks and the last PID. On Linux, D-state tasks (waiting for disk or NFS) **count**, unlike traditional Unix, so a high load can mean I/O wait rather than CPU pressure. Interpret relative to CPU count: load 8 on 8 CPUs ≈ fully busy; 16 means a queue; trend 1 > 5 > 15 shows whether it's rising. Pair with PSI, which separates CPU, memory and I/O stalls.

## Practical guidance

1. **Don't guess priorities**: measure run-queue latency and throttling first.
2. For latency-critical services: isolate cores, avoid hard CPU quotas, pin IRQs, consider `SCHED_FIFO` with small bounded work, disable frequency scaling (`performance` governor) and deep C-states.
3. For batch jobs: `nice 19` plus `SCHED_BATCH`/`SCHED_IDLE` and `ionice -c3`.
4. Avoid RT priorities for anything that can loop; keep the 5 % RT reserve; use watchdogs.
5. Watch for **priority inversion** (a low-priority task holds a lock needed by a high-priority one): use PI mutexes (`PTHREAD_PRIO_INHERIT`).
6. Account for **steal time** in VMs and **noisy neighbors**: the scheduler can't give what the hypervisor withholds.

## Key takeaways

- Classes in order: deadline > real-time (FIFO/RR, 1–99) > fair (nice) > idle; an RT task can starve everything below it.
- The fair class shares CPU by weight: nice steps ≈ ×1.25; EEVDF (6.6+) picks the eligible task with the earliest virtual deadline, replacing CFS's vruntime heuristics.
- Each CPU has a runqueue; balancing follows hardware topology; affinity and isolation trade flexibility for determinism.
- cgroup `cpu.weight` is proportional sharing, `cpu.max` is a hard cap that causes throttling latency.
- Load average counts runnable *and* uninterruptible tasks; use PSI and run-queue latency for diagnosis.
