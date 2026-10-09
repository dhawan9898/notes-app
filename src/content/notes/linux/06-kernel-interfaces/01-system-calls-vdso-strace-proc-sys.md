---
title: System Calls, the vDSO, strace, /proc, /sys and Netlink
track: linux
module: Kernel Interfaces
moduleOrder: 6
order: 1
level: advanced
summary: How user programs call the kernel and what that costs, the vDSO fast path, tracing syscalls with strace, and the virtual filesystems and sockets (/proc, /sys, sysctl, netlink) through which Linux exposes its internals.
sources:
  - label: "man 2 syscalls, man 2 syscall, man 7 vdso, man 1 strace, man 7 errno"
    url: https://man7.org/linux/man-pages/man2/syscalls.2.html
  - label: "Linux kernel docs — The Linux ABI (Documentation/ABI), /proc filesystem, sysfs"
    url: https://docs.kernel.org/admin-guide/abi.html
  - label: "man 5 proc, man 5 sysfs, man 8 sysctl, man 7 netlink, man 7 rtnetlink"
    url: https://man7.org/linux/man-pages/man5/proc.5.html
  - label: "System V AMD64 ABI — syscall calling convention"
  - label: "Kerrisk — The Linux Programming Interface, Ch. 3 System Programming Concepts"
---

The kernel is the only code that can touch hardware, page tables or other processes. Everything a program does that matters — files, memory, networking, process creation — is a **system call** (syscall). Linux also publishes its state as pseudo-files (`/proc`, `/sys`) and message sockets (netlink), which is how tools like `ps`, `ip` and `top` work.

## User mode, kernel mode and the syscall

The CPU runs in privilege levels. Application code runs unprivileged (x86 ring 3); the kernel runs privileged (ring 0). A syscall is a controlled gate between them:

1. The program (via libc) loads the **syscall number** into `rax` and up to six arguments into `rdi, rsi, rdx, r10, r8, r9` (x86-64; ARM64 uses `x8` and `x0–x5`).
2. It executes the **`syscall`** instruction. The CPU switches to kernel mode at an entry point (`entry_SYSCALL_64`), saves user state, switches stacks, and dispatches through the syscall table (`sys_call_table`).
3. The handler validates arguments (never trusting user pointers: `copy_from_user`/`copy_to_user` with fault handling), does the work, and returns a value in `rax`.
4. On return the kernel checks for pending signals and reschedule needs, then executes `sysret`.

**Errors**: the raw convention returns a negative `-errno` (values −4095…−1). The libc wrapper converts this: it sets the thread-local `errno` and returns −1. Check every return value; `strerror(errno)`/`perror` give text; `EINTR`, `EAGAIN`, `ENOENT`, `EACCES`, `EPERM`, `ENOMEM`, `EINVAL` are the everyday ones.

Linux has ~350 syscalls (`man 2 syscalls`; `ausyscall --dump`). The **syscall ABI is stable forever** ("we don't break userspace"), which is why old binaries still run and why containers and static binaries (Go) can bypass libc and call the kernel directly. Libraries lag behind the kernel: new calls (`openat2`, `clone3`, `pidfd_open`, `io_uring_setup`, `close_range`) may lack glibc wrappers for a while: use `syscall(SYS_xxx, ...)`.

### Cost of a syscall

A syscall is far more expensive than a function call: privilege switch, entry/exit work, cache and TLB effects, and, since 2018, **Meltdown/Spectre mitigations** (page-table isolation, retpolines, IBRS...) that add substantial overhead. Measured on a typical virtual machine:

```text
real syscall getppid:            359 ns/call
clock_gettime via vDSO:           55 ns/call
clock_gettime forced syscall:    452 ns/call
```

So ≈ 0.4 µs per syscall on this VM (the exact figure depends on CPU generation and mitigations). A program making a syscall per byte (`read(fd, &c, 1)`) is 100× slower than buffered I/O: batch (`readv`, big buffers, `sendmmsg`, `io_uring`), use stdio buffering, avoid polling loops that call `gettimeofday` unnecessarily.

## The vDSO

For a few calls that only read kernel-maintained data, Linux avoids the mode switch entirely. The kernel maps a small shared object, the **vDSO** (virtual dynamic shared object, `[vdso]` in `/proc/PID/maps`), into every process, along with a read-only **`[vvar]`** page holding clock data updated by the kernel. glibc resolves `clock_gettime`, `gettimeofday`, `time`, `getcpu` to functions inside it which compute the time from the TSC and `vvar` in user space: ~55 ns instead of ~450 ns (above). Consequences: these calls don't appear in `strace` output; in some virtualized environments the clocksource is not TSC-based and vDSO falls back to real syscalls (check `cat /sys/devices/system/clocksource/clocksource0/current_clocksource`: `kvm-clock`, `tsc`; `hpet`/`acpi_pm` are slow).

```text
7f5f6c40d000-7f5f6c411000 r--p 00000000 00:00 0   [vvar]
7f5f6c411000-7f5f6c413000 r-xp 00000000 00:00 0   [vdso]
```

## strace: seeing the syscalls

`strace` uses `ptrace` to stop the target at each syscall entry/exit and prints name, arguments and result. It is the first tool to reach for when "the program hangs", "can't find a file", or "fails with a mysterious error".

```bash
strace -f -tt -T -o /tmp/trace.txt ./prog args       # -f follow forks/threads, -tt wall time, -T time spent in each call
strace -p 1234 -f                                    # attach to a running process
strace -e trace=openat,read,write ./prog             # filter by syscall
strace -e trace=file -f ./prog                       # file-related: open, stat, access...
strace -e trace=network ./prog                       # socket, connect, sendto...
strace -c -f ./prog                                  # summary table: calls, errors, time per syscall
strace -y -e trace=read,write ./prog                 # show paths/addresses behind fds
strace -k ./prog                                     # print a stack trace for each syscall (with libunwind)
```

Illustrative output:

```text
openat(AT_FDCWD, "/etc/app/config.yaml", O_RDONLY|O_CLOEXEC) = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/usr/share/app/config.yaml", O_RDONLY|O_CLOEXEC) = 3
read(3, "port: 8080\n", 4096) = 11 <0.000021>
connect(4, {sa_family=AF_INET, sin_port=htons(5432), sin_addr=inet_addr("10.0.0.5")}, 16) = -1 ETIMEDOUT <21.1>
futex(0x7f3c2c0013d0, FUTEX_WAIT_PRIVATE, 2, NULL) = ?  <unfinished ...>
```

Read it for: failed `openat`/`stat` (wrong path, permissions), long `<time>` in `connect`/`read` (network or disk wait), `futex` waits (lock contention), `EAGAIN` busy loops, repeated `clock_gettime`, `write` sizes (unbuffered I/O), `execve` chains. **Caveats:** `ptrace` makes the process 2–100× slower, can change timing bugs, and only one tracer can attach (conflicts with debuggers); not for production hot paths. Alternatives: `perf trace` (lower overhead), `bpftrace`/`bcc` (`opensnoop`, `execsnoop`, `syscount`), `ltrace` (library calls), `gdb`, `sysdig`/`falco`. `strace -ff -o out` writes one file per process.

## Syscall filtering: seccomp

A process can install a **seccomp-BPF** filter that checks each syscall number/arguments and allows, errors, kills or traps it. Container runtimes apply a default profile (blocking ~40–60 dangerous calls: `kexec_load`, `mount`, `reboot`, `ptrace` historically, `bpf`...), browsers and `systemd` units (`SystemCallFilter=@system-service`, `SystemCallArchitectures=native`) use it to shrink the attack surface. See the security notes.

## /proc: the process and kernel information filesystem

`procfs` is a virtual filesystem; reading a file runs kernel code that formats current state.

### Per process (`/proc/PID/`, `/proc/self/`)

| File | Content |
|---|---|
| `cmdline` | argv, NUL-separated (`tr '\0' ' '`) |
| `environ` | environment (readable only by owner/root) |
| `exe`, `cwd`, `root` | symlinks to the binary, working directory, root dir |
| `fd/`, `fdinfo/` | open files (symlinks; offsets and flags); `ls -l` shows sockets as `socket:[inode]` |
| `maps`, `smaps`, `smaps_rollup` | memory mappings with RSS/PSS |
| `status` | human-readable: state, UIDs, `VmRSS`, threads, signal masks, capabilities, ctx switches |
| `stat`, `statm` | machine-readable CPU times, state, RSS (what `ps` reads) |
| `limits` | resource limits (`ulimit`) |
| `io` | bytes read/written (needs same-user access) |
| `sched`, `schedstat` | scheduler statistics |
| `wchan`, `stack`, `syscall` | what a sleeping process waits on (kernel symbol), kernel stack (root), current syscall |
| `ns/` | namespace links (`pid:[4026531836]`) |
| `cgroup`, `mountinfo`, `net/` | cgroup membership, mounts, the network namespace's tables |
| `task/TID/` | per-thread versions of the above |
| `oom_score`, `oom_score_adj` | OOM killer |

```bash
tr '\0' '\n' < /proc/$PID/environ | sort           # a process's environment
ls -l /proc/$PID/fd | head                          # what files/sockets it holds
cat /proc/$PID/wchan ; echo                         # where a hung process sleeps (e.g. nfs_wait..., io_schedule)
readlink /proc/$PID/ns/net                          # same inode = same network namespace
```

### System-wide

`/proc/cpuinfo`, `meminfo`, `loadavg`, `stat` (CPU time counters), `vmstat`, `interrupts`, `softirqs`, `diskstats`, `mounts`, `filesystems`, `devices`, `modules`, `kallsyms` (addresses hidden unless `kptr_restrict=0`), `cmdline` (kernel parameters), `version`, `uptime`, `locks`, `net/{dev,tcp,udp,unix,route,arp,snmp,netstat}`, `pressure/{cpu,memory,io}` (PSI), `config.gz` (if enabled). Almost every monitoring tool is a parser of these files; reading them directly is cheap and dependency-free.

### sysctl: kernel tunables

`/proc/sys/` is a tree of tunables: `kernel/`, `vm/`, `net/`, `fs/`. Dots map to slashes: `net.ipv4.ip_forward` = `/proc/sys/net/ipv4/ip_forward`.

```bash
sysctl net.ipv4.ip_forward                    # read
sysctl -w vm.swappiness=10                    # change until reboot
echo "vm.swappiness = 10" > /etc/sysctl.d/90-local.conf ; sysctl --system   # persist
sysctl -a | grep -i somaxconn                 # search
```

Document why each non-default exists: tunables copied from blog posts are a common source of regressions.

## /sys: the device and kernel object model

`sysfs` exports the kernel's **object hierarchy** (kobjects): one directory per device, bus, driver, class, with attributes as small text files, typically one value per file.

| Path | What |
|---|---|
| `/sys/devices/` | the real device tree (PCI → host → disk ...) |
| `/sys/bus/<bus>/{devices,drivers}` | devices and drivers per bus (`pci`, `usb`, `virtio`); `bind`/`unbind` files attach a driver |
| `/sys/class/<class>/` | devices by function: `net/eth0`, `block/sda`, `thermal`, `power_supply`, `backlight` |
| `/sys/block/<dev>/queue/` | block tunables (scheduler, rotational, read_ahead_kb) |
| `/sys/module/<name>/parameters/` | module parameters, often writable at runtime |
| `/sys/fs/cgroup/` | cgroup v2 hierarchy; `/sys/fs/bpf` bpffs; `/sys/kernel/{mm,debug,tracing,btf}` |
| `/sys/firmware/` | ACPI tables, EFI variables, DMI |

```bash
cat /sys/class/net/eth0/{address,mtu,speed,carrier,operstate}
echo 1 > /sys/block/sda/device/rescan                 # rescan a resized disk
echo 0 > /sys/devices/system/cpu/cpu3/online          # offline a CPU
cat /sys/devices/system/clocksource/clocksource0/current_clocksource
```

Rules: `/sys` is the kernel's **ABI for userspace tools** (documented in `Documentation/ABI/`), but individual files may be missing across kernels/hardware; use `udevadm info -a -p /sys/...` to walk parent attributes. **debugfs** (`/sys/kernel/debug`, root only) and **tracefs** (`/sys/kernel/tracing`) are unstable debugging interfaces. `securityfs`, `configfs` (LIO targets, usb gadget), `cgroup2fs`, `bpffs`, `pstore` and `efivarfs` are other special filesystems.

## ioctl, netlink and other control channels

| Mechanism | Used for |
|---|---|
| **`ioctl(fd, request, arg)`** | device-specific commands on a file descriptor (terminal size `TIOCGWINSZ`, block `BLKGETSIZE64`, KVM, DRM/GPU, old `SIOCSIFADDR` network config). Opaque per-driver ABI, hard to evolve |
| **netlink** (`AF_NETLINK` sockets) | structured, extensible, asynchronous messages between kernel and userspace: **rtnetlink** (links, addresses, routes, neighbors, qdiscs: what `ip` and `tc` use), `nfnetlink` (nftables, conntrack), **genetlink** (nl80211, ethtool-netlink, devlink, taskstats), `NETLINK_KOBJECT_UEVENT` (udev events), audit. `ip monitor` shows live events; `ss -f netlink` |
| **`prctl`, `arch_prctl`, `sysctl`, `fcntl`** | process or file attributes |
| **`ptrace`** | debugger and tracer control (restricted by `kernel.yama.ptrace_scope`) |
| **`perf_event_open`, `bpf`** | performance monitoring and eBPF (see the eBPF note) |
| **`/dev/*` nodes** | character-device interfaces: `/dev/kvm`, `/dev/urandom`, `/dev/fuse`, `/dev/dri/*` |

Tip: legacy tools (`ifconfig`, `route`, `netstat`, `arp`) read `/proc/net` and use ioctls; the modern replacements (`ip`, `ss`, `ip neigh`) use netlink, which scales to large tables and gives accurate data (`ip -s -d link`).

## Debugging recipes

| Question | Command |
|---|---|
| Why is it slow to start? | `strace -f -tt -T ./prog 2>&1 | less`, look at long gaps and failed lookups |
| Which config files does it read? | `strace -f -e trace=openat,stat ./prog 2>&1 | grep -v ENOENT` |
| What is process 1234 doing right now? | `cat /proc/1234/{wchan,syscall,stack}`, `strace -p`, `gdb -p ... bt` |
| Which process holds this file or port? | `lsof /path`, `ss -ltnp`, `fuser -v /path` |
| Which files are heavily used? | `perf trace`, `bpftrace`, `fatrace`, `/proc/PID/io` deltas |
| What did the kernel tell me? | `dmesg -T`, `journalctl -k` |

## Key takeaways

- A syscall is a privilege switch via the `syscall` instruction with arguments in registers; errors come back as `-errno`; each call costs ~0.4 µs (with mitigations), so batch.
- The vDSO serves `clock_gettime` & co. in user space (~55 ns, invisible to `strace`).
- `strace -f -tt -T -e …` is the quickest explanation for hangs and missing files; use `perf trace`/eBPF where overhead matters.
- `/proc` = processes and system state; `/proc/sys` = tunables via `sysctl`; `/sys` = the device model and driver knobs; netlink = structured control (what `ip` uses).
- The syscall ABI is stable; `/proc`, `/sys` and netlink are mostly stable; debugfs/tracefs/tracepoints are not promises.
