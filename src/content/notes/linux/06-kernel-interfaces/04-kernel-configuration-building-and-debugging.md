---
title: "Kernel Configuration, Building Modules, Oops Analysis and kdump"
track: linux
module: Kernel Interfaces
moduleOrder: 6
order: 4
level: expert
summary: "How the kernel is configured (Kconfig, .config, what the running kernel's options mean), built (Kbuild, out-of-tree modules shown with a real build and modinfo), and debugged: reading an oops, symbolizing addresses, dynamic debug, ftrace/kprobes, lockdep and KASAN, magic SysRq, and the kexec/kdump crash-dump workflow."
prerequisites:
  - Kernel Modules, Device Drivers and udev
  - "The Boot Process — Firmware, Bootloader, Kernel, initramfs and init"
  - "System Calls, the vDSO, strace, /proc, /sys and Netlink"
sources:
  - label: "Linux kernel documentation — Kconfig language"
    url: https://docs.kernel.org/kbuild/kconfig-language.html
  - label: "Linux kernel documentation — Kernel Build System (Kbuild) and building external modules"
    url: https://docs.kernel.org/kbuild/modules.html
  - label: "Linux kernel documentation — Bug hunting and Oops tracing"
    url: https://docs.kernel.org/admin-guide/bug-hunting.html
  - label: "Linux kernel documentation — Dynamic debug"
    url: https://docs.kernel.org/admin-guide/dynamic-debug-howto.html
  - label: "Linux kernel documentation — Documentation for Kdump (kexec-based crash dumping)"
    url: https://docs.kernel.org/admin-guide/kdump/kdump.html
  - label: "Linux kernel documentation — Kernel Address Sanitizer (KASAN), lockdep, KFENCE"
    url: https://docs.kernel.org/dev-tools/kasan.html
  - label: "Linux kernel documentation — Linux Magic System Request Key Hacks"
    url: https://docs.kernel.org/admin-guide/sysrq.html
  - label: "Linux kernel documentation — Kernel debugging with kgdb"
    url: https://docs.kernel.org/process/debugging/kgdb.html
  - label: "crash utility white paper — analysing kernel crash dumps"
    url: https://crash-utility.github.io/crash_whitepaper.html
---

A distribution kernel is one particular answer to roughly ten thousand yes/no/numeric questions. Understanding how those answers are given (configuration), how the answer becomes a binary (build), and what to do when the binary fails (debugging) is the difference between treating the kernel as a black box and being able to reason about a production incident.

## Kconfig: ten thousand questions

Every kernel feature has a `CONFIG_*` symbol defined in a `Kconfig` file in the source tree. A symbol is typically one of:

| Value | Meaning |
|---|---|
| `y` | built into the kernel image (`vmlinuz`) |
| `m` | built as a loadable module (`.ko`), loaded on demand by `modprobe`/udev |
| `n` / `# CONFIG_X is not set` | not built |
| number / string | parameters such as `CONFIG_HZ=250`, `CONFIG_NR_CPUS=8192` |

Symbols have **dependencies** (`depends on`), **reverse dependencies** (`select`, which forces another symbol on regardless of its dependencies, so use sparingly) and defaults. The user-facing tools (`make menuconfig`, `make nconfig`, `make olddefconfig`, `make localmodconfig`, `make defconfig`) edit the `.config` file. A kernel built with `CONFIG_IKCONFIG_PROC` exposes it in `/proc/config.gz`; distributions also ship it as `/boot/config-<version>`.

The script below summarizes a config and prints the symbols that matter most in practice. It runs on the config of the kernel running this machine (a distribution kernel):

```python
#!/usr/bin/env python3
"""Summarise a kernel .config: how many options are built in (y), modular (m) or off, and the value of options that shape behavior."""
import collections, sys
path = sys.argv[1]
state, value = collections.Counter(), {}
for line in open(path):
    line = line.strip()
    if line.startswith("# CONFIG_") and line.endswith(" is not set"):
        state["not set"] += 1; value[line.split()[1]] = "n"
    elif line.startswith("CONFIG_"):
        k, v = line.split("=", 1); value[k] = v
        state["y" if v == "y" else "m" if v == "m" else "value"] += 1
print(f"options: {sum(state.values())}   built in (=y): {state['y']}   modules (=m): {state['m']}   numeric/string: {state['value']}   disabled: {state['not set']}")
for group, names in {
    "preemption": ["CONFIG_PREEMPT_NONE", "CONFIG_PREEMPT_VOLUNTARY", "CONFIG_PREEMPT", "CONFIG_PREEMPT_DYNAMIC", "CONFIG_PREEMPT_RT"],
    "timers": ["CONFIG_HZ", "CONFIG_NO_HZ_IDLE", "CONFIG_NO_HZ_FULL", "CONFIG_HIGH_RES_TIMERS"],
    "scale": ["CONFIG_SMP", "CONFIG_NR_CPUS", "CONFIG_NUMA", "CONFIG_X86_64"],
    "memory": ["CONFIG_SLUB", "CONFIG_TRANSPARENT_HUGEPAGE", "CONFIG_ZSWAP", "CONFIG_MEMCG", "CONFIG_KSM"],
    "virtualization": ["CONFIG_KVM", "CONFIG_KVM_INTEL", "CONFIG_KVM_AMD", "CONFIG_VFIO", "CONFIG_VIRTIO_NET", "CONFIG_VHOST_NET"],
    "security": ["CONFIG_SECURITY_SELINUX", "CONFIG_SECURITY_APPARMOR", "CONFIG_SECURITY_LANDLOCK", "CONFIG_RANDOMIZE_BASE", "CONFIG_STACKPROTECTOR_STRONG", "CONFIG_HARDENED_USERCOPY", "CONFIG_INIT_ON_ALLOC_DEFAULT_ON"],
    "debugging": ["CONFIG_DEBUG_INFO_BTF", "CONFIG_KPROBES", "CONFIG_FTRACE", "CONFIG_BPF_SYSCALL", "CONFIG_KASAN", "CONFIG_PROVE_LOCKING", "CONFIG_KEXEC", "CONFIG_CRASH_DUMP", "CONFIG_MAGIC_SYSRQ"],
    "modules": ["CONFIG_MODULES", "CONFIG_MODULE_SIG", "CONFIG_MODULE_SIG_FORCE", "CONFIG_MODVERSIONS", "CONFIG_MODULE_COMPRESS_XZ", "CONFIG_MODULE_COMPRESS_ZSTD"],
}.items():
    print(f"\n{group}:")
    for n in names: print(f"  {n[7:]:<28} {value.get(n, '(absent)')}")
```

```text
$ python3 kconfig_report.py /boot/config-$(uname -r)
options: 9503   built in (=y): 2650   modules (=m): 4051   numeric/string: 198   disabled: 2604

preemption:
  PREEMPT_NONE                 n
  PREEMPT_VOLUNTARY            y
  PREEMPT                      n
  PREEMPT_DYNAMIC              y
  PREEMPT_RT                   n

timers:
  HZ                           250
  NO_HZ_IDLE                   n
  NO_HZ_FULL                   y
  HIGH_RES_TIMERS              y

scale:
  SMP                          y
  NR_CPUS                      8192
  NUMA                         y
  X86_64                       y

memory:
  SLUB                         y
  TRANSPARENT_HUGEPAGE         y
  ZSWAP                        y
  MEMCG                        y
  KSM                          y

virtualization:
  KVM                          m
  KVM_INTEL                    m
  KVM_AMD                      m
  VFIO                         m
  VIRTIO_NET                   m
  VHOST_NET                    m

security:
  SECURITY_SELINUX             y
  SECURITY_APPARMOR            y
  SECURITY_LANDLOCK            y
  RANDOMIZE_BASE               y
  STACKPROTECTOR_STRONG        y
  HARDENED_USERCOPY            y
  INIT_ON_ALLOC_DEFAULT_ON     y

debugging:
  DEBUG_INFO_BTF               y
  KPROBES                      y
  FTRACE                       y
  BPF_SYSCALL                  y
  KASAN                        n
  PROVE_LOCKING                n
  KEXEC                        y
  CRASH_DUMP                   y
  MAGIC_SYSRQ                  y

modules:
  MODULES                      y
  MODULE_SIG                   y
  MODULE_SIG_FORCE             n
  MODVERSIONS                  y
  MODULE_COMPRESS_XZ           y
  MODULE_COMPRESS_ZSTD         n
```

How to read it:

- A distribution kernel builds **most of the drivers and features as modules** (`m`, about 4,000 here) and only a core (`y`, about 2,600) into the image; the initramfs loads the modules needed to reach the root filesystem. A custom embedded or cloud kernel flips this: everything needed is `y`, very little is `m`, and `CONFIG_MODULES` may be off, making the kernel smaller and faster to boot and removing the module-loading attack surface.
- `PREEMPT_DYNAMIC=y` means the preemption model (see the latency note) can be chosen at boot with `preempt=none|voluntary|full`; `PREEMPT_RT` is off (a real-time kernel is a separate configuration). `HZ=250` is the timer tick frequency; `NO_HZ_FULL=y` allows tickless operation on cores named in `nohz_full=`.
- `KVM`, `VFIO` and `VHOST_NET` are modules: virtualization support exists but loads only when needed.
- Hardening options (`RANDOMIZE_BASE` = KASLR, `STACKPROTECTOR_STRONG`, `HARDENED_USERCOPY`, `INIT_ON_ALLOC_DEFAULT_ON` zeroing new allocations) trade a few percent of performance for mitigation of whole bug classes. `MODULE_SIG=y` with `MODULE_SIG_FORCE` off means signatures are checked but unsigned modules still load, unless Secure Boot lockdown forces the issue.
- Debug facilities (`KPROBES`, `FTRACE`, `BPF_SYSCALL`, `DEBUG_INFO_BTF`) are present in production kernels because they have low cost when idle; the expensive checkers (`KASAN`, `PROVE_LOCKING`) are off and belong in test kernels.

## Building the kernel

The standard sequence from a source tree is:

```bash
make olddefconfig         # take the existing .config, answer new questions with defaults
make -j"$(nproc)"         # compile vmlinux, bzImage, and all modules
make modules_install      # copy .ko files to /lib/modules/<version>/
make install              # copy bzImage, System.map, config to /boot and update the bootloader / initramfs
```

Kbuild is a recursive `make` system: each directory's `Makefile` lists objects with `obj-y += foo.o` (built in) or `obj-$(CONFIG_FOO) += foo.o` (built as `y` or `m` according to the config). Useful facts:

- The **version string** (`uname -r`) is `VERSION.PATCHLEVEL.SUBLEVEL` plus `CONFIG_LOCALVERSION`; modules record it in a **vermagic** string and refuse to load into a kernel that differs unless forced.
- `make menuconfig` reads `Kconfig`; **config fragments** (`scripts/kconfig/merge_config.sh`) let you keep a small delta file instead of a full 9,000-line config in version control.
- Build artifacts: `vmlinux` (uncompressed ELF with symbols, needed for debugging), `arch/x86/boot/bzImage` (compressed boot image), `System.map` (symbol addresses), `Module.symvers` (exported symbol CRCs used by `MODVERSIONS`).
- `ccache`, `-j`, building out of tree with `O=build/`, and `make localmodconfig` (keep only modules currently loaded) shorten builds from an hour to minutes for a test kernel.
- Distribution packaging: Debian `make bindeb-pkg`, Red Hat `make binrpm-pkg`.

### An out-of-tree module, built for real

External modules need only the installed kernel **headers/build tree** (`linux-headers-*` package, exposed as `/lib/modules/$(uname -r)/build`). The module:

```c
// A minimal loadable kernel module: parameters, init/exit, printk. Built here against installed kernel headers; not loaded.
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int repeat = 1;
module_param(repeat, int, 0444);
MODULE_PARM_DESC(repeat, "how many times to greet");

static int __init hello_init(void)
{
	for (int i = 0; i < repeat; i++)
		pr_info("hello: greeting %d of %d\n", i + 1, repeat);
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello: goodbye\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("DeepNotes example");
MODULE_DESCRIPTION("Minimal module used to show Kbuild");
```

and its `Makefile`, which hands control to the kernel's Kbuild:

```text
obj-m += hello.o
KDIR ?= /usr/src/linux-headers-6.12.95+deb13-amd64
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

Build it and inspect the result (the path to the headers is passed on the command line here because the headers installed on this machine are for a slightly different build of the same kernel series; normally you use the default `/lib/modules/$(uname -r)/build`):

```text
$ make KDIR=/usr/src/linux-headers-6.12.95+deb13-amd64 2>&1 | sed "s#$PWD#.#g" ; modinfo ./hello.ko | sed "s#$PWD#.#g"; make clean KDIR=/usr/src/linux-headers-6.12.95+deb13-amd64 > /dev/null 2>&1
make -C /usr/src/linux-headers-6.12.95+deb13-amd64 M=. modules
make[1]: Entering directory '/usr/src/linux-headers-6.12.95+deb13-amd64'
  CC [M]  ./hello.o
  MODPOST ./Module.symvers
  CC [M]  ./hello.mod.o
  CC [M]  ./.module-common.o
  LD [M]  ./hello.ko
  BTF [M] ./hello.ko
make[1]: Leaving directory '/usr/src/linux-headers-6.12.95+deb13-amd64'
filename:       ././hello.ko
description:    Minimal module used to show Kbuild
author:         DeepNotes example
license:        GPL
depends:        
name:           hello
retpoline:      Y
vermagic:       6.12.95+deb13-amd64 SMP preempt mod_unload modversions 
parm:           repeat:how many times to greet (int)
```

The build steps are the Kbuild pipeline in miniature: `CC` compiles `hello.c`, `MODPOST` reads the symbols the module uses and checks them against `Module.symvers`, a generated `hello.mod.c` supplies the module's metadata (including the **vermagic**), `LD` links the final `hello.ko`, and `BTF` attaches type information when the kernel uses BTF. `modinfo` then reads the metadata from the `.ko`: license, parameters (`repeat`) and the `vermagic` that must match the running kernel (`SMP preempt mod_unload modversions` lists the configuration flags that affect the module ABI). Loading it needs root (`insmod hello.ko repeat=3`, messages appear in `dmesg`, `rmmod hello`); with Secure Boot or `MODULE_SIG_FORCE` the module must also be signed.

## When the kernel fails: reading an oops

A **BUG**, a general protection fault, a NULL pointer dereference or an explicit `panic()` prints an **oops** to the kernel log (`dmesg`, `journalctl -k`, serial console). An oops usually kills the offending task and taints the kernel; a **panic** halts the machine (or reboots, per `kernel.panic`). Anatomy of the report:

| Part | What to read |
|---|---|
| First line, e.g. `BUG: kernel NULL pointer dereference, address: 0000000000000010` | the failure class and the faulting address; a small address means a NULL pointer plus a struct-field offset (here the field at offset 0x10) |
| `RIP: 0010:foo_bar+0x4a/0x120 [mymodule]` | the faulting instruction: function, offset in it, function size, and the module |
| `Call Trace:` | the stack, innermost first; entries with `?` are unreliable guesses from stack scanning |
| `Code:` | the bytes around the faulting instruction (`<xx>` marks it), disassemble with `scripts/decodecode` |
| Register dump | `RAX`, `RDI` etc. at the fault: one will hold the bad pointer |
| `Tainted: G W O E` | flags: P proprietary module, O out-of-tree module, E unsigned module, W earlier warning, D earlier oops... Maintainers ignore reports from tainted kernels with unknown modules |
| `CPU`, `PID`, `Comm` | the context: which task, which CPU, interrupt or process context |

To turn `foo_bar+0x4a` into a source line you need a kernel with debug info (`vmlinux` with `CONFIG_DEBUG_INFO`): `scripts/decode_stacktrace.sh vmlinux < oops.txt` or `addr2line -e vmlinux -i <address>`, and `gdb vmlinux` followed by `list *(foo_bar+0x4a)`. For a module use the module's `.ko` built with debug info. Distributions publish matching debug packages (`-dbg`, `-debuginfo`).

## Debugging tools in the kernel

- **`printk` and dynamic debug**: `pr_debug()` calls compile in but are off until enabled at run time: `echo 'module hello +p' > /sys/kernel/debug/dynamic_debug/control`, or boot with `dyndbg="file drivers/net/foo.c +p"`. Log levels: `dmesg -l err,warn`, `dmesg -w` to follow.
- **ftrace and kprobes**: function and function-graph tracing, tracepoints, dynamic probes on nearly any kernel function without recompiling (see the profiling note); `bpftrace` (eBPF note) is the friendlier front end.
- **`WARN_ON`, `BUG_ON`, `lockdep`** (`CONFIG_PROVE_LOCKING`): lockdep records the order in which locks are taken and reports potential deadlocks *before* they happen in production; **`KASAN`** instruments memory accesses to detect use-after-free and out-of-bounds in the kernel (5-10x slowdown); **`KFENCE`** is a sampling variant cheap enough for production fleets; **`KCSAN`** finds data races; **`UBSAN`** detects undefined behavior; **`kmemleak`** finds leaked allocations; **`debug_pagealloc`**, **`slub_debug`** poison and track allocations.
- **Magic SysRq** (`echo 1 > /proc/sys/kernel/sysrq`, then Alt+SysRq+key, or `echo <key> > /proc/sysrq-trigger`): `w` dumps blocked tasks, `l` backtraces all CPUs, `m` memory info, `t` all tasks, `s` sync, `u` remount read-only, `b` reboot, `c` crash (trigger a crash dump on purpose). The "REISUB" sequence safely reboots a machine whose userspace is wedged.
- **kgdb** debugs a running kernel from a second machine over serial or network; **QEMU/KVM with `-s -S`** gives a gdb stub on a virtual machine, the most convenient environment for kernel development.
- **Watchdogs and lockup detectors**: the soft-lockup detector (a CPU stuck in kernel for more than about 20 s without scheduling), hard-lockup (NMI watchdog), hung-task detector (a task stuck uninterruptible for 120 s), and RCU stall warnings (see the RCU note) turn silent hangs into stack traces. Set `kernel.panic_on_oops=1` and `kernel.softlockup_panic=1` on fleets that prefer reboot (with a crash dump) to limping.

## kdump: capturing a crash for later analysis

A panicked kernel is the worst place to run complicated code, so kdump boots a **second, small kernel** to save the memory of the first:

1. At boot a region of memory is **reserved** for the capture kernel: `crashkernel=256M` (or `crashkernel=auto`/range forms) on the command line.
2. `kexec -p` (usually via `kdump.service`) **loads** the capture kernel and a minimal initramfs into that region ahead of time.
3. When the first kernel panics (or on Magic SysRq `c`), it jumps with **kexec** into the capture kernel, without going through firmware (so device and memory state are preserved).
4. The capture kernel sees the crashed kernel's memory as `/proc/vmcore` (an ELF core file) and runs `makedumpfile` to filter out unneeded pages (zero pages, page cache, free pages) and compress it to local disk, NFS or SSH, then reboots.
5. The dump (`vmcore`) is analysed offline with the **`crash`** utility or `gdb`, using the `vmlinux` with debug info for exactly that kernel build.

```bash
sudo systemctl status kdump               # confirm the capture kernel is loaded
cat /sys/kernel/kexec_crash_loaded        # 1 when the capture kernel is armed
cat /proc/cmdline | tr ' ' '\n' | grep crashkernel
echo c | sudo tee /proc/sysrq-trigger      # test the whole chain (this crashes the machine!)

crash vmlinux vmcore
crash> bt             # backtrace of the panicking task
crash> log            # the kernel log at the time of the crash
crash> ps | grep UN   # tasks in uninterruptible sleep
crash> kmem -i        # memory usage summary
crash> struct task_struct <address>
```

Practical points: test kdump when it is configured and after every kernel or firmware upgrade (a kdump that has never been exercised usually does not work); size the reservation for the crash kernel plus the drivers it needs (large NIC and storage drivers need more); filter the dump level to keep it practical on machines with terabytes of RAM; make sure the dump target is reachable from the minimal initramfs; and keep `vmlinux` debug packages for every deployed kernel.

## A debugging workflow

1. **Capture**: the first oops is the important one (later ones are often consequences); save `dmesg`/journal, `uname -a`, `/proc/cmdline`, loaded modules (`lsmod`), the taint flags and the exact kernel build.
2. **Classify**: NULL dereference, use-after-free, deadlock/hang, memory corruption, hardware error (MCE/EDAC messages), or out-of-memory. Hardware errors show up in `mcelog`/`rasdaemon` or `EDAC` lines and are not software bugs.
3. **Localize**: symbolize the RIP and call trace to a source line, read the code, check the register that holds the bad pointer.
4. **Reproduce** on a debug kernel (KASAN, lockdep, slub_debug) and bisect with `git bisect` between a good and a bad release if the regression is recent.
5. **Isolate** third-party and out-of-tree modules (unload or blacklist them) and test with the vendor-supported kernel before reporting upstream with the full oops, config and steps.

## Key takeaways

- A kernel is defined by its `.config`: `y` builds in, `m` builds a module, and distribution kernels put most drivers in modules; `/boot/config-*` shows exactly what your running kernel has.
- Kbuild compiles the kernel and external modules from the same system; a module's `vermagic` must match the kernel, and `modinfo` reads its metadata.
- An oops names the fault, the instruction and the call trace; symbolize with `decode_stacktrace.sh`/`addr2line` against the matching `vmlinux`, and watch the taint flags.
- Use dynamic debug, ftrace/kprobes, lockdep and KASAN/KFENCE to find bugs before and after they occur; SysRq gives a lifeline on wedged systems.
- kdump keeps a reserved capture kernel that saves `/proc/vmcore` after a panic for offline analysis with `crash`; test it before you need it.
