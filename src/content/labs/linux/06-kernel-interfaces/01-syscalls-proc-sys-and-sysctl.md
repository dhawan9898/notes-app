---
title: System Calls, the vDSO and /proc, /sys, sysctl
track: linux
module: Kernel Interfaces
moduleOrder: 6
order: 1
level: advanced
minutes: 40
env: machine
summary: "Five exercises on the kernel-interfaces module: making system calls by number from C, reading errno, finding the vDSO in the address space and measuring what it saves, reading kernel state from /proc and /sys, and changing a per-namespace sysctl as an ordinary user, with captured output."
prerequisites:
  - "System Calls, the vDSO, strace, /proc, /sys and Netlink"
  - "Kernel Modules, Device Drivers and udev"
sources:
  - label: "man 2 syscall, man 7 vdso, man 5 proc, man 5 sysfs, man 8 sysctl"
    url: https://man7.org/linux/man-pages/man7/vdso.7.html
  - label: "Linux kernel documentation — The /proc filesystem and sysctl"
    url: https://docs.kernel.org/admin-guide/sysctl/index.html
  - label: "Kerrisk — The Linux Programming Interface, Ch. 3 (System programming concepts) and Ch. 12 (System and process information)"
---

Everything a program does to the outside world, reading a file, creating a process, sending a packet, is a request to the kernel through the **system call** interface. These exercises call the kernel directly, look at the one trick that avoids the call for hot paths, and walk the two virtual filesystems the kernel uses to expose its state. The syscall numbers differ between architectures (and between 32-bit and 64-bit x86), which is why the program uses the names from `<sys/syscall.h>`. Parts 1 to 3 run in the Linux terminal of this app as `./syscalls_demo`; the rest needs a real machine.

## The program

```c
/* System calls by number, the vDSO, and what a real system call costs compared with a vDSO call. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/* the raw system call: 32-bit systems with 64-bit time use clock_gettime64 and a 64-bit timespec */
#ifdef SYS_clock_gettime64
#define RAW_CLOCK_GETTIME SYS_clock_gettime64
struct kernel_ts { long long sec, nsec; };
#else
#define RAW_CLOCK_GETTIME SYS_clock_gettime
struct kernel_ts { long sec, nsec; };
#endif

static double ns_per_call(int use_syscall, int n) {
    struct timespec ts, t0, t1; struct kernel_ts kts;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < n; i++) { if (use_syscall) syscall(RAW_CLOCK_GETTIME, CLOCK_MONOTONIC, &kts); else clock_gettime(CLOCK_MONOTONIC, &ts); }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / n;
}

int main(void) {
    printf("1. a system call made by number, bypassing the C wrapper:\n"); fflush(stdout);
    long raw = syscall(SYS_getpid);
    printf("   syscall(SYS_getpid) returned %s\n", raw == getpid() ? "the same PID as getpid()" : "a different value"); fflush(stdout);
    const char msg[] = "   hello written with syscall(SYS_write, fd=1, ...)\n";
    long n = syscall(SYS_write, 1, msg, sizeof msg - 1);
    printf("   write returned %ld (the number of bytes written)\n", n);
    errno = 0; long bad = syscall(SYS_write, 999, msg, sizeof msg - 1);
    printf("   write to an invalid descriptor returned %ld, errno %d (%s)\n", bad, errno, strerror(errno));

    printf("2. the vDSO: part of the kernel mapped into every process\n");
    FILE *f = fopen("/proc/self/maps", "r"); char line[512];
    while (f && fgets(line, sizeof line, f)) if (strstr(line, "[vdso]") || strstr(line, "[vvar]")) { char range[64], tag[32]; sscanf(line, "%63s", range); char *t = strrchr(line, '['); sscanf(t, "%31s", tag); printf("   %s %s\n", range, tag); }
    if (f) fclose(f);

    int N = 200000; double fast = ns_per_call(0, N), slow = ns_per_call(1, N);
    printf("3. clock_gettime via libc/vDSO: %.0f ns per call; via a real system call: %.0f ns per call (ratio %.1fx)\n", fast, slow, slow / fast);
    return 0;
}
```

```text
$ ./syscalls_demo
1. a system call made by number, bypassing the C wrapper:
   syscall(SYS_getpid) returned the same PID as getpid()
   hello written with syscall(SYS_write, fd=1, ...)
   write returned 52 (the number of bytes written)
   write to an invalid descriptor returned -1, errno 9 (Bad file descriptor)
2. the vDSO: part of the kernel mapped into every process
   b7f72000-b7f76000 [vvar]
   b7f76000-b7f78000 [vdso]
3. clock_gettime via libc/vDSO: 5976 ns per call; via a real system call: 22431 ns per call (ratio 3.8x)
```

## Exercises

**1. A system call by number.** Section 1 calls `getpid` and `write` by their numbers (`SYS_getpid` and `SYS_write`) through the generic `syscall()` function. What did each return, and what is the difference between this and calling `getpid()`?

**2. Errors.** The last call in section 1 wrote to descriptor 999. What did the kernel return, how does C code see the reason, and why is the return value `-1` here but a negative error number inside the kernel?

**3. The vDSO.** Section 2 prints two mappings in the process's address space. What are they, and what does section 3 show about time functions? Why can the kernel serve `clock_gettime` without a real system call?

**4. Kernel state as files.** What do `/proc/sys/kernel/pid_max`, `/sys/class/net/lo/mtu` and `/proc/loadavg` give you, and how do `/proc`, `/sys` and `sysctl` relate? Read the following and explain each line.

```text
$ cat /sys/class/net/lo/mtu
65536
$ sysctl kernel.pid_max vm.max_map_count
kernel.pid_max = 4194304
vm.max_map_count = 1048576
$ cat /proc/loadavg
0.24 0.26 0.27 1/2113 41101
```

**5. Tuning without root.** An ordinary user cannot write `/proc/sys/vm/swappiness`, but inside a new network namespace they can change network sysctls. Show it with `net.ipv4.icmp_echo_ignore_all` and explain why the host is unaffected.

```text
$ cat /proc/sys/net/ipv4/icmp_echo_ignore_all
0
$ unshare -Urn bash -c 'echo 1 > /proc/sys/net/ipv4/icmp_echo_ignore_all; echo -n "inside the namespace: "; cat /proc/sys/net/ipv4/icmp_echo_ignore_all'
inside the namespace: 1
$ cat /proc/sys/net/ipv4/icmp_echo_ignore_all      # on the host
0
$ echo 1 > /proc/sys/vm/swappiness
bash: /proc/sys/vm/swappiness: Permission denied
```

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** `syscall(SYS_getpid)` returned the **same PID** as `getpid()`, and `syscall(SYS_write, 1, msg, len)` (write to descriptor 1, standard output) printed the message and returned **52, the number of bytes written** (the same convention as `write(2)`). Normal programs never use numbers: the C library has a **wrapper** for each call (`getpid()`, `write()`) that puts the number and arguments into registers, executes the `syscall` instruction, and converts the result. The raw interface shows what is underneath: one numbered entry point per kernel service (several hundred on x86-64), the number identifying *which*, and up to six integer arguments. The numbers are architecture-specific and stable forever (the kernel never reuses or changes them), which is how a 20-year-old binary still runs. Tools such as `strace` print each call as it happens; on a system without it, `/proc/PID/syscall` shows the one a stopped process is in.

**2.** The call returned **`-1`** and **errno 9, `Bad file descriptor`** (`EBADF`). The kernel actually returns the error as a *negative number* in the result register (`-9`); the C library detects that, stores `9` in the thread-local variable **`errno`** and returns `-1` to your code, so a failing call is recognised by the return value and *explained* by `errno` (and `perror`/`strerror` turn it into text). Two rules follow: **read `errno` immediately** after the failing call (any other library call may overwrite it), and **only trust it when the call reported failure**: a successful call may leave a stale `errno`. Descriptor 999 is not open in this process, so `write` fails cleanly and nothing is written.

**3.** `[vdso]` and `[vvar]` are two small regions the **kernel maps into every process**: the **vDSO** (virtual dynamic shared object) is a few kilobytes of kernel-supplied *code*, and `vvar` is a read-only page of kernel *data* (including the current time, updated by the kernel). Reading the time needs no privilege, so the C library's `clock_gettime` calls the vDSO function, which reads the shared page and computes the answer **in user space, with no mode switch**. A real system call (the same function through `syscall(SYS_clock_gettime, ...)`) must trap into the kernel and back: in the output above it is several times slower than the vDSO call. (The virtual PC in this app is an emulator, so the absolute times are far larger than on real hardware; the same program compiled natively and run on a 64-bit ARM phone measured about 120 ns for the vDSO call against about 860 ns for the real system call. On x86-64 expect a few tens of nanoseconds against a few hundred, and more with mitigations for CPU vulnerabilities.) The vDSO serves `clock_gettime`, `gettimeofday`, `time` and `getcpu`. This is why timing code with `clock_gettime` is cheap enough to put in a hot loop, and why a system that falls back to a real call (some virtualised clock sources) suddenly gets much slower.

**4.** All three are **virtual files**: nothing is stored on disk, and reading one asks the kernel for the current value. `/proc/sys/kernel/pid_max` is the highest process ID (here 4,194,304, so PIDs can be large; it is a *tunable*); `/sys/class/net/lo/mtu` is an attribute of the loopback device (65,536 bytes, the biggest packet it carries) and `/sys` is organised as the **device model**: `class`, `bus`, `devices`, `block`, `fs`, `module` list the kernel's objects, one directory per object and one file per attribute; `/proc/loadavg` has the 1-, 5- and 15-minute load averages, then `running/total` scheduling entities (`1/2113`) and the last PID allocated. **`/proc`** is the older interface for process information and system statistics, **`/sys`** is the structured one for devices and kernel objects, and **`/proc/sys`** is the **sysctl** tree: `sysctl kernel.pid_max` is just a convenient way to read `/proc/sys/kernel/pid_max` (dots stand for slashes), and `sysctl -w name=value` writes it. `vm.max_map_count` (1,048,576) is the limit on memory mappings per process, which large Java or Elasticsearch processes often need raised. Persistent settings go in `/etc/sysctl.d/*.conf`, loaded at boot; a `sysctl -w` change is lost on reboot.

**5.** Global sysctls such as `vm.swappiness` belong to the whole machine, so writing them needs `CAP_SYS_ADMIN` in the **initial** namespaces and an ordinary user gets `Permission denied`. But the `net.*` sysctls are **per network namespace**: each namespace has its own copy. `unshare -Urn` gave the shell a *new* network namespace and made it root *there*, so it may change that namespace's settings: the value became `1` inside and stayed `0` on the host. This is exactly how a container tunes `net.ipv4.ip_forward` or `net.core.somaxconn` for itself without affecting the host (the runtime sets them from the container's configuration). Other namespaced sysctls include parts of `kernel.*` (hostname, `shm*`, `msg*`) and `fs.mqueue`, while `vm.*` and `kernel.pid_max` are global. When tuning a server, record the sysctl, its old value, why you changed it and which namespace it applies to.

</details>

## Key takeaways

1. System calls are numbered kernel entry points reached with the `syscall` instruction; C wrappers return `-1` and set `errno`, which must be read immediately after a failure.
2. The vDSO maps kernel code and data into every process so `clock_gettime` and similar calls need no mode switch.
3. `/proc` (processes and statistics), `/sys` (the device model) and `/proc/sys` (sysctl) are virtual files; reading one asks the kernel, writing one can change its behaviour.
4. Network sysctls are per namespace, so a user (or container) can tune them for its own namespace without affecting the host; global ones need real privilege.
