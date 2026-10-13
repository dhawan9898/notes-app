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
