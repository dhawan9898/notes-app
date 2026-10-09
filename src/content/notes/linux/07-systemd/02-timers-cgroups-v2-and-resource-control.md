---
title: systemd Timers, cgroups v2 and Resource Control
track: linux
module: systemd and Services
moduleOrder: 7
order: 2
level: advanced
summary: Scheduling with timers (calendar syntax, Persistent, accuracy), the unified cgroup v2 hierarchy and how systemd maps units to it, controlling CPU, memory, I/O and task counts per service, delegation, and diagnosing throttling and OOM.
sources:
  - label: "systemd.timer(5), systemd.time(7), systemd-analyze(1) calendar/timespan"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.timer.html
  - label: "Linux kernel docs — Control Group v2"
    url: https://docs.kernel.org/admin-guide/cgroup-v2.html
  - label: "systemd.resource-control(5), systemd-run(1), systemd-cgtop(1), systemd.slice(5)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.resource-control.html
  - label: "Heo — Control Group v2 (LPC 2015/2017 talks); cgroup v2 delegation (systemd CGROUP_DELEGATION)"
    url: https://systemd.io/CGROUP_DELEGATION/
  - label: "Linux kernel docs — PSI Pressure Stall Information"
    url: https://docs.kernel.org/accounting/psi.html
---

## Timers

A **timer unit** activates another unit (by default the service with the same name) on a schedule. Compared with cron: dependencies and ordering, logging in the journal, resource limits and sandboxing for the job, `systemctl status`/`list-timers` visibility, randomized delay, catch-up of missed runs, and no mail-on-output surprises.

```ini
# /etc/systemd/system/backup.service
[Unit]
Description=Nightly backup
[Service]
Type=oneshot
ExecStart=/usr/local/bin/backup.sh
Nice=10
IOSchedulingClass=idle
MemoryMax=1G

# /etc/systemd/system/backup.timer
[Unit]
Description=Run backup nightly
[Timer]
OnCalendar=*-*-* 02:30:00
Persistent=true
RandomizedDelaySec=15min
[Install]
WantedBy=timers.target
```

Enable the **timer**, not the service: `systemctl enable --now backup.timer`; inspect with `systemctl list-timers --all`.

### Trigger types

| Option | Meaning |
|---|---|
| `OnCalendar=` | wall-clock (realtime) schedule, calendar expressions below |
| `OnBootSec=` | relative to boot |
| `OnStartupSec=` | relative to systemd manager start (matters for user managers) |
| `OnUnitActiveSec=` / `OnUnitInactiveSec=` | relative to the last activation / deactivation of the target unit: "every 5 minutes after the last run finished" (no overlap) |
| `OnActiveSec=` | relative to the timer's own activation |
| `Persistent=true` | store last-run time on disk; run immediately at boot if a scheduled run was missed while powered off (anacron behavior) |
| `RandomizedDelaySec=` | spread load across machines |
| `AccuracySec=` | allowed coalescing window (**default 1 min**: timers may fire up to a minute late to batch wakeups; set `1s` for precision) |
| `WakeSystem=true` | wake from suspend (RTC alarm) |

### Calendar syntax

`DOW YYYY-MM-DD HH:MM:SS`, with `*` wildcards, lists `,`, ranges `..`, and repetition `/N`. Verify instead of guessing:

```text
$ systemd-analyze calendar "Mon..Fri *-*-* 02:30" "daily" "*:0/15"
  Original form: Mon..Fri *-*-* 02:30
Normalized form: Mon..Fri *-*-* 02:30:00
    Next elapse: Mon 2026-10-12 02:30:00 IST
  Original form: daily
Normalized form: *-*-* 00:00:00
    Next elapse: Sat 2026-10-10 00:00:00 IST
  Original form: *:0/15
Normalized form: *-*-* *:00/15:00
    Next elapse: Fri 2026-10-09 07:45:00 IST

$ systemd-analyze timespan "1h 30min 15s"
      μs: 5415000000
   Human: 1h 30min 15s
```

Shortcuts: `minutely`, `hourly`, `daily`, `weekly`, `monthly`, `yearly`. Time zones can be appended (`Europe/Berlin`), and `--iterations=5` lists upcoming runs. Pitfall: `OnCalendar=*:0/15` means minutes 0, 15, 30, 45 of every hour. Another: services started by a timer **run in a fresh cgroup with a clean environment**: no login `PATH`, no `~/.bashrc`.

Overlap behavior: if the previous run is still active when the timer elapses, systemd does *not* start a second copy of a `Type=oneshot` service (it's already activating); use `flock` in scripts only for cross-unit exclusion. `systemd-run --on-calendar='*-*-* 03:00' --unit=once-job /usr/local/bin/job` creates a transient timer.

## cgroups v2

**Control groups** organize processes into a hierarchy and apply resource accounting and limits per group. cgroup v2 (the default on current distributions) uses a **single unified hierarchy** mounted at `/sys/fs/cgroup`, with controllers (cpu, memory, io, pids, cpuset, hugetlb, rdma, misc) enabled per subtree. Differences from v1: one hierarchy for all controllers, consistent semantics, better memory accounting (page cache and kernel memory together), PSI, delegation rules, and the "no internal processes" rule.

```bash
cat /sys/fs/cgroup/cgroup.controllers          # cpuset cpu io memory hugetlb pids rdma misc
cat /proc/self/cgroup                           # 0::/user.slice/user-1000.slice/session-3.scope
mount | grep cgroup2                            # cgroup2 on /sys/fs/cgroup type cgroup2 (rw,nosuid,nodev,noexec,relatime,nsdelegate,memory_recursiveprot)
```

Core mechanics:

- A cgroup is a **directory**; `mkdir` creates one, processes join by writing their PID to `cgroup.procs` (moving a whole process; `cgroup.threads` for threaded mode).
- A controller is available to children only if the parent lists it in `cgroup.subtree_control` (`echo "+cpu +memory" > cgroup.subtree_control`).
- **No internal processes rule**: a non-root cgroup that delegates controllers to children can't itself contain processes (leaves hold processes). This shapes systemd's layout.
- Resources are **distributed top-down**: a parent's limit bounds all descendants.
- Removing a cgroup needs it empty (`rmdir`). `cgroup.kill` (write `1`) kills every process in the subtree (kernel 5.14+); `cgroup.freeze` freezes it.

### systemd's layout

systemd owns the tree and maps units onto it:

```text
-.slice
├─system.slice          # services: ssh.service, nginx.service, ...
├─user.slice
│  └─user-1000.slice
│     ├─session-3.scope   # login session processes
│     └─user@1000.service # the user's systemd --user manager (+ its units)
└─machine.slice         # VMs and containers (nspawn, libvirt)
```

`systemd-cgls` prints the tree, `systemd-cgtop` shows live CPU/memory/IO per cgroup, and `systemctl status` includes the unit's cgroup and process list. Because every process of a service is in its cgroup, `KillMode=control-group` reliably stops leaked children, and accounting is accurate (a service that forks 30 helpers is still one entity).

## Resource control directives

Set in unit files or at runtime (`systemctl set-property --runtime myapp.service MemoryMax=2G`; `systemd-run -p …` for transient units):

| Directive | cgroup file | Semantics |
|---|---|---|
| `CPUWeight=` (1–10000, default 100) | `cpu.weight` | **proportional share** when CPU is contended |
| `CPUQuota=50%` (200% = 2 CPUs) | `cpu.max` (`quota period`) | **hard cap**; exceeding is throttled |
| `AllowedCPUs=0-3`, `AllowedMemoryNodes=` | `cpuset.cpus`, `cpuset.mems` | pinning/NUMA |
| `MemoryMin=` / `MemoryLow=` | `memory.min` / `memory.low` | protected memory (not reclaimed below min; best-effort low) |
| `MemoryHigh=` | `memory.high` | **soft limit**: allocations above it are throttled and forced to reclaim; no kill |
| `MemoryMax=` | `memory.max` | **hard limit**: exceeding triggers reclaim, then the cgroup's OOM killer |
| `MemorySwapMax=` | `memory.swap.max` | cap swap use (0 disables) |
| `IOWeight=`, `IODeviceWeight=` | `io.weight` | proportional I/O share |
| `IOReadBandwidthMax=/dev/sda 50M`, `IOWriteIOPSMax=` | `io.max` | hard I/O limits |
| `TasksMax=` | `pids.max` | cap threads/processes (fork-bomb protection; default 15 % of kernel limit or distro setting) |
| `Delegate=yes` | | give the unit's subtree to the service to manage (containers, user managers) |
| `OOMPolicy=`, `ManagedOOMMemoryPressure=kill` | | what happens on OOM; hand to `systemd-oomd` |

Real properties applied by systemd for a test unit with `MemoryMax=64M`, `CPUWeight=50`, `TasksMax=20`:

```text
$ systemctl --user show demo.service -p MemoryMax,CPUWeight,TasksMax
CPUWeight=50
MemoryMax=67108864
TasksMax=20
```

Design guidance:

- Use **weights/protection** (CPUWeight, IOWeight, MemoryLow) for fair sharing and **maximums** (MemoryMax, TasksMax) as safety rails. Prefer `MemoryHigh` slightly under `MemoryMax` to throttle before killing.
- **CPU quotas harm latency**: a service gets its quota in bursts, and after using it, is throttled until the next period (default 100 ms) even if the machine is idle. Check `cpu.stat` (`nr_throttled`, `throttled_usec`). Prefer weights, or larger quotas with a longer period, for latency-sensitive services.
- Memory accounting includes **page cache** and kernel memory: a service reading big files can "use" its limit in cache (reclaimable: harmless unless limit < working set) — read `memory.stat` (`anon`, `file`, `kernel`, `slab`, `pgfault`, `workingset_refault`) and `memory.events` (`high`, `max`, `oom`, `oom_kill`).
- Per-user limits: a drop-in for all users, `systemctl edit user-.slice` → `[Slice] MemoryMax=80% TasksMax=4096`, protects shared hosts.
- Always set `MemoryMax` on risky workloads (builds, converters, untrusted code) so one job can't take down the host.

### Delegation

Only the cgroup's owner may manage subtrees. systemd's per-user manager `user@1000.service` is delegated `cpu memory pids` by default (as seen in its `cgroup.controllers`), but not necessarily `io` or `cpuset`: so `systemd-run --user -p IOWeight=…` may be refused, a limit that surprises people. Administrators can extend with `Delegate=cpu cpuset io memory pids` in a drop-in for `user@.service`. Containers get `Delegate=yes` so their own init/runtime manages the subtree. Never write to a cgroup that systemd manages without delegation: it will fight you.

### Diagnosing

```bash
systemd-cgtop -m                                  # sort by memory
cat /sys/fs/cgroup/system.slice/myapp.service/{memory.current,memory.max,memory.events,cpu.stat,pids.current}
cat /sys/fs/cgroup/system.slice/myapp.service/{cpu,memory,io}.pressure    # PSI: % of time stalled, some/full
journalctl -k | grep -i 'oom\|killed process\|memory cgroup'
systemctl show myapp.service -p MemoryCurrent,CPUUsageNSec,TasksCurrent,NRestarts,Result
```

Interpreting: `memory.events high` rising → throttled by `MemoryHigh`; `oom_kill` > 0 → the cgroup killed something (container exit code 137); `nr_throttled` rising with low CPU use → quota too tight; PSI `cpu some avg10` > 10 → CPU contention; PSI `memory full` > 0 → thrashing.

## Key takeaways

- Timers add dependency-aware, journaled, resource-limited scheduling to cron-like behavior; verify expressions with `systemd-analyze calendar`; use `Persistent=true`, mind `AccuracySec` (1 min default).
- cgroup v2 is one hierarchy; systemd maps slices/services/scopes to it and uses it for accurate process tracking and limits.
- `CPUWeight`/`MemoryLow`/`IOWeight` = fair share; `CPUQuota`/`MemoryMax`/`IOMax`/`TasksMax` = hard limits; `MemoryHigh` throttles before kill.
- CPU quota throttling hurts latency; page cache counts toward memory; delegation controls who may manage which controllers.
- Read `memory.events`, `cpu.stat` and PSI to see what the limits are doing.
