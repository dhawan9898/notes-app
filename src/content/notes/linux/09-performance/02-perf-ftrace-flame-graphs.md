---
title: Profiling and Tracing — perf, ftrace, Flame Graphs and Friends
track: linux
module: Performance and Observability
moduleOrder: 9
order: 2
level: expert
summary: How to find where CPU time goes and why threads wait — sampling vs tracing, perf stat/record/report, flame graphs (on-CPU, off-CPU), symbol and stack-unwinding problems, ftrace, language profilers and hardware counter interpretation.
sources:
  - label: "perf wiki and man perf-stat(1), perf-record(1), perf-report(1), perf-top(1), perf-sched(1), perf-trace(1), perf-c2c(1)"
    url: https://perfwiki.github.io/main/
  - label: "Gregg — Flame Graphs; CPU Flame Graphs; Off-CPU Analysis; perf-tools"
    url: https://www.brendangregg.com/flamegraphs.html
  - label: "Linux kernel docs — ftrace (Documentation/trace/ftrace.rst), Event tracing, kprobes, uprobes"
    url: https://docs.kernel.org/trace/ftrace.html
  - label: "Yasin — A Top-Down Method for Performance Analysis and Counters Architecture (ISPASS 2014)"
  - label: "Linux kernel docs — perf_event_open, perf security (kernel.perf_event_paranoid)"
    url: https://docs.kernel.org/admin-guide/perf-security.html
  - label: "man 2 perf_event_open"
    url: https://man7.org/linux/man-pages/man2/perf_event_open.2.html
---

Once the bottleneck class is known (CPU, waiting, memory…), you need to know **which code**. Two techniques, with different trade-offs:

| | Sampling | Tracing |
|---|---|---|
| How | interrupt N times per second and record the instruction pointer / stack | record every event (syscall, function call, tracepoint) |
| Overhead | low and bounded (99 Hz ≈ 1–3 %) | proportional to event rate; can be huge |
| Output | statistical profile ("where time is spent") | exact sequence and timing |
| Misses | rare/short events (< sampling period) | nothing, but can distort timing |
| Tools | `perf record`, `perf top`, profilers, `bpftrace profile:` | `strace`, `ftrace`, `perf trace`, bpftrace tracepoints, LTTng |

Use sampling to find hot code, tracing to explain a specific slow event.

## Hardware counters and `perf stat`

`perf` (the kernel's `perf_event_open` interface plus the `perf` tool from `linux-tools-*`) reads CPU performance counters (PMU) and software events.

```bash
perf stat -d ./prog                          # whole-run counters
perf stat -p 1234 -- sleep 10                # an existing process for 10 s
perf stat -e cycles,instructions,cache-misses,branch-misses,context-switches,cpu-migrations,page-faults -a -- sleep 5
```

Key numbers:

- **IPC = instructions / cycles**. Modern x86 can retire 4+; IPC around 2–3 is healthy compute; **< 1 indicates stalls**, usually memory latency (cache misses). Cloud VMs often don't expose the PMU (`<not supported>`).
- **Cache-miss and branch-miss rates**: LLC misses are 60–100 ns each; a loop with random memory access across a large array is memory-bound regardless of the arithmetic.
- **context-switches / cpu-migrations / page-faults** hint at scheduling or memory behavior.
- **Top-down analysis** (`perf stat --topdown`, `toplev` from pmu-tools) classifies slots into Retiring, Bad Speculation, Frontend Bound, Backend Bound (memory vs core): a guided way to explain IPC.

## Sampling CPU profiles

```bash
perf record -F 99 -a -g -- sleep 30            # system-wide, 99 Hz, with call stacks, 30 s
perf record -F 99 -g -p $(pidof myapp) -- sleep 30
perf record -F 99 --call-graph dwarf -p PID -- sleep 10     # DWARF-based unwinding when no frame pointers
perf report --stdio --no-children | head -50   # who burns CPU (self time); --children for inclusive time
perf top -g                                    # live
perf annotate --stdio -s symbol                # hot instructions inside a function
```

Use a sampling rate that isn't a round number (99 Hz, not 100) to avoid lockstep with periodic timers. `perf.data` can be large; use `-o`, and `perf script` to export.

### Getting good stacks

A profile is only useful if stack traces and symbols are right:

- **Frame pointers**: the fast unwinder walks `rbp` chains. Compilers omit them by default at `-O2`; use `-fno-omit-frame-pointer` (distros such as Fedora, Ubuntu 24.04 and Arch now build the base system with frame pointers precisely for profiling). Without them stacks are broken or truncated.
- **DWARF unwinding** (`--call-graph dwarf`) copies stack bytes (large data, higher overhead). **LBR** (`--call-graph lbr`, Intel) and **ORC** (the kernel's own unwinder) are other options.
- **Symbols**: install debug symbols (`-dbgsym`, `debuginfod`), keep the same binary used in production (build-id match). Stripped binaries show raw addresses.
- **JIT / interpreted languages**: perf needs map files: Node `--perf-basic-prof`, Java `perf-map-agent`/async-profiler, .NET `DOTNET_PerfMapEnabled=1`, Python 3.12+ `-X perf`.
- **Permissions**: `kernel.perf_event_paranoid` controls who may profile (−1 everything, 0 allows CPU events, 1 allows kernel profiling for users, 2 user-space only, **3 (Debian/Ubuntu patches) disables unprivileged use entirely**); `kernel.kptr_restrict` hides kernel addresses. Use root, `CAP_PERFMON`, or adjust deliberately.

## Flame graphs

A **flame graph** (Brendan Gregg) visualizes aggregated stack samples:

- Each box is a function; **width = fraction of samples** in which it appeared (including callees). The **y-axis is stack depth** (callers below, callees above). The **x-axis is alphabetical, not time**.
- Look for the **wide plateaus at the top**: functions that consume time themselves. Wide towers show where entire code paths cost.
- Colors are decorative (or indicate user/kernel/JIT).

```bash
git clone https://github.com/brendangregg/FlameGraph
perf record -F 99 -a -g -- sleep 30
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > cpu.svg
```

Variants:

- **Off-CPU flame graph**: samples *blocking* time (why threads are waiting: I/O, locks, sleeps, scheduler delays): the answer for "latency high but CPU idle". Collect with `offcputime-bpfcc -df -p PID 30` (bcc) or bpftrace on `sched_switch`; flame graph with `flamegraph.pl --color=io --countname=us`. Combine with on-CPU for the full picture ("where does wall time go").
- **Differential flame graphs** (`difffolded.pl`): compare before/after a change.
- **Memory flame graphs**: `malloc` call stacks (`memleak`, heaptrack), page faults (`perf record -e page-faults`), or `perf record -e cache-misses` for hot cache-missing code.
- Interactive tools: speedscope, Firefox Profiler, Pyroscope/Parca (continuous), `perf report` TUI.

## Beyond CPU: perf subcommands

| Command | Use |
|---|---|
| `perf sched record/latency/timehist` | scheduler wait times and context-switch timeline per task |
| `perf trace -p PID` | `strace`-like with far lower overhead (syscall latencies, page faults) |
| `perf lock` / `contention` | kernel and user lock contention (with tracepoints) |
| `perf mem`, `perf c2c` | memory access sampling; **false sharing** detection (cache-to-cache HITM) |
| `perf probe` | dynamic kprobes/uprobes: `perf probe -a 'tcp_sendmsg size'` then `perf record -e probe:tcp_sendmsg` |
| `perf list` | available events: hardware, software, tracepoints (`sched:*`, `block:*`, `syscalls:*`) |

## ftrace: the kernel's built-in tracer

ftrace lives in **tracefs** (`/sys/kernel/tracing`, root; older `/sys/kernel/debug/tracing`). It traces kernel functions, function graphs (entry/exit timing), tracepoints and latency (irqs-off, preempt-off, wakeup) with very low overhead via dynamic patching of function prologues.

```bash
cd /sys/kernel/tracing
cat available_tracers                      # function function_graph nop ...
echo function_graph > current_tracer
echo vfs_read > set_graph_function         # only this function's call tree
echo 1 > tracing_on ; sleep 1 ; echo 0 > tracing_on
head -40 trace                             # indented call graph with durations in µs
echo nop > current_tracer ; echo > set_graph_function

echo 1 > events/sched/sched_switch/enable  # tracepoint events
cat trace_pipe                             # streaming, blocking reader
echo 'p:myprobe tcp_sendmsg size=%dx'  >> kprobe_events   # dynamic kprobe event (arch-specific args)
```

Front-ends: **`trace-cmd record -e sched -p function_graph -g do_sys_openat2 …` / `trace-cmd report`** and KernelShark (GUI) wrap the files; Gregg's `perf-tools` (`funccount`, `iosnoop`, `opensnoop`, `execsnoop`, `kprobe`) use ftrace on kernels without eBPF. Typical uses: why does this syscall take 40 ms (function_graph with `tracing_thresh`), which functions are called per second (`funccount`), what woke this thread (`sched_wakeup` events), latency sources (`irqsoff`, `wakeup_rt`).

Compared with eBPF (see that note): ftrace is always available and simple but less programmable; eBPF/bpftrace can aggregate in-kernel (histograms) instead of dumping every event.

## Application-level profilers

| Language / platform | Tools |
|---|---|
| C/C++/Rust | `perf`, `valgrind --tool=callgrind` (precise, 20–50× slower), `cachegrind`, `gprof`, `heaptrack`, `hotspot` GUI |
| Go | `net/http/pprof`, `go tool pprof -http`, `runtime/trace`, `GODEBUG=gctrace=1` |
| Java/JVM | JFR (Java Flight Recorder), async-profiler (wall-clock/CPU/alloc/lock), `jstack`, GC logs |
| Python | `py-spy` (sampling, no code change, attach by PID), `cProfile`, `scalene`, `memray` |
| Node.js | `--cpu-prof`, `--prof`, `clinic.js`, `0x`, V8 inspector |
| Ruby, PHP, .NET | `rbspy`, `xdebug`/`spx`, `dotnet-trace`/`dotnet-counters` |
| Databases | `EXPLAIN (ANALYZE, BUFFERS)`, `pg_stat_statements`, slow query log, `perf` on the database process |

Poor man's profiler: `for i in 1 2 3 4 5; do gdb -p PID -batch -ex 'thread apply all bt' ; sleep 1; done | sort | uniq -c | sort -rn` shows the stacks that recur (it pauses the process each time: use only when nothing better exists).

Continuous profiling (Parca, Pyroscope, Google Cloud Profiler, Datadog) runs low-rate eBPF/language samplers all the time, so there is already a profile from *before* the incident.

## Worked investigation (pattern)

1. `top -H` / `pidstat -t` shows a service at 400 % CPU in user mode; `perf stat` shows IPC 0.8, high LLC misses.
2. `perf record -F 99 -g -p PID -- sleep 30` then a flame graph: a wide plateau in `json_decode` → `memcpy` → page faults from allocation.
3. Hypothesis: the decoder allocates a new buffer per request. Check with `perf record -e page-faults`/`ltrace -c` or `bpftrace` counting `mmap`.
4. Fix: reuse buffers/arena; re-run the same load; compare flame graphs differentially and `perf stat` IPC. Confirm latency percentiles improved, not just CPU.
5. Add a continuous-profiling dashboard and a regression test.

## Pitfalls

- Profiling the wrong thing: an on-CPU profile is blind to waiting; check the CPU is actually saturated first.
- **Inlining** hides functions; **tail calls** and PLT stubs confuse stacks; compile with `-g` (debug info doesn't change codegen) and consider `-fno-inline` only for experiments.
- Sampling bias in VMs without PMU (falls back to timer-based cpu-clock, skewed toward kernel safe points).
- Short runs give noisy profiles: collect ≥ 30 s or 10,000 samples for a stable picture.
- Overhead of tracing high-frequency events (`malloc`, every packet): sample or aggregate in-kernel.
- Don't optimize what isn't on the critical path; re-measure end-to-end after every change.

## Key takeaways

- Sample to find hot code (`perf record -F 99 -g`), trace to explain specific slow events (`perf trace`, ftrace, eBPF).
- Good stacks need frame pointers or DWARF, matching symbols and JIT map files; `perf_event_paranoid` governs access.
- Flame graphs: width = time share, read plateaus; use off-CPU graphs when CPU is idle but latency is high.
- IPC and cache-miss counters distinguish compute-bound from memory-bound code; top-down analysis structures it.
- ftrace is the always-on kernel tracer; language profilers and continuous profiling cover applications.
