---
title: "Practice: Namespaces and cgroups by Hand"
track: linux
module: Containers
moduleOrder: 8
order: 5
level: advanced
summary: "Six exercises that build the two halves of a container without any container runtime: separate hostname, process IDs, network and user ID mappings with unshare, and resource limits with cgroup v2 through systemd, including reading the limit files and measuring a CPU quota, all with captured output."
prerequisites:
  - "Linux Namespaces — The Isolation Primitive Behind Containers"
  - "Container Resource Limits, Seccomp, Capabilities and Hardening"
  - "Container Runtimes, the OCI Specs and overlayfs"
sources:
  - label: "man 7 namespaces, man 1 unshare, man 7 user_namespaces, man 7 pid_namespaces"
    url: https://man7.org/linux/man-pages/man7/namespaces.7.html
  - label: "Linux kernel documentation — Control Group v2"
    url: https://docs.kernel.org/admin-guide/cgroup-v2.html
  - label: "systemd.resource-control(5)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.resource-control.html
---

A container is not a thing the kernel knows about. It is an ordinary process wrapped in **namespaces** (what it can *see*) and a **cgroup** (what it can *use*). These exercises create each part by hand as an unprivileged user, so after them a container runtime is no longer magic. Every command is real; names and IDs are replaced where they identify your machine.

## Exercises

**1. Private hostname.** In a new UTS namespace change the hostname to `demo-box`. What do you see inside and outside?

**2. Private process tree.** Start a shell in a new PID namespace (with its own `/proc`). What PID does it have, what does `ps` show, and why does the shell behave like `init`?

**3. Private network.** Run `ip -br link` in a new network namespace. What is in it, in what state, and what do you have to do before even `127.0.0.1` works?

**4. Private user IDs.** With `unshare -Ur` read `/proc/self/uid_map`. How do you read its three columns, and what does it say about who you "really" are?

**5. Which namespaces are there?** List the namespace types a process belongs to, and name the seven you would combine to build a container.

**6. Limits with cgroups.** Start a service with `MemoryMax=64M`, `CPUQuota=25%` and `TasksMax=20` and read the corresponding files in `/sys/fs/cgroup`. Then run a busy loop with `CPUQuota=20%` for about four seconds and measure the CPU time it received. Which of the limits kills a process, and which only slows it?

## Answers

Try before opening.

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
$ hostname                  # on the host
HOST
$ unshare -Urm --uts bash -c 'hostname demo-box; hostname'
demo-box
$ hostname
HOST (unchanged)
```

(The real host name is replaced by `HOST`.) A **UTS namespace** holds a private hostname and domain name. The change is visible only to processes inside; the host never notices. `-Ur` creates a user namespace first (so an unprivileged user may create the others) and maps you to root inside it; `-m` adds a mount namespace; `--uts` the hostname one.

**2.**

```text
$ unshare -Ur --pid --fork --mount-proc bash -c 'echo "my pid inside: $$"; ps -o pid,ppid,comm | head -4'
my pid inside: 1
    PID    PPID COMMAND
      1       0 bash
      2       1 ps
      3       1 head
```

In a new **PID namespace** numbering starts again: the first process is **PID 1**, its parent is shown as 0 (the parent is outside the namespace and cannot be seen), and `ps` lists only processes of this namespace. That is possible because `--mount-proc` mounted a fresh `/proc` that reflects the namespace (without it `ps` would still read the host's `/proc`). `--fork` is required because the process that calls `unshare(CLONE_NEWPID)` is not itself moved; its *children* are. PID 1 is special: the kernel treats it as the namespace's `init`, so it adopts orphaned processes (it must reap them) and if it exits the kernel kills everything else in the namespace, which is why container entry points must handle signals and zombies properly (use a tiny init such as `tini`, or `docker run --init`).

**3.**

```text
$ unshare -Urn bash -c 'ip -br link; ip link set lo up; ip -br addr show lo'
lo               DOWN           00:00:00:00:00:00 <LOOPBACK>
gre0@NONE        DOWN           0.0.0.0 <NOARP>
gretap0@NONE     DOWN           00:00:00:00:00:00 <BROADCAST,MULTICAST>
erspan0@NONE     DOWN           00:00:00:00:00:00 <BROADCAST,MULTICAST>
tunl0@NONE       DOWN           0.0.0.0 <NOARP>
sit0@NONE        DOWN           0.0.0.0 <NOARP>
lo               UNKNOWN        127.0.0.1/8 ::1/128
```

A **network namespace** is a complete, separate network stack: its own interfaces, addresses, routing table, firewall rules and ports. The new one contains only a **loopback device that is down** (the other lines are placeholder tunnel devices that appear when those kernel modules are loaded; they are unused), so *nothing* works, not even `ping 127.0.0.1`, until you bring `lo` up: afterwards it has `127.0.0.1` and `::1`. To connect the namespace to the world a runtime creates a **veth pair** (a virtual cable) with one end inside and one outside, attached to a bridge, and adds NAT: exactly the labs in *Practice: A Static Routing Lab*. Two namespaces can both listen on port 80 without conflict because their stacks are separate.

**4.**

```text
$ unshare -Ur bash -c 'echo -n "uid_map: "; cat /proc/self/uid_map'
uid_map:          0       1000          1
```

Each line of `uid_map` is `ID-inside  ID-outside  length`: here **UID 0 inside maps to UID 1000 (your real UID; 1000 is typical for the first user, yours may differ) outside, for a range of 1**. "Root" inside is only an alias for your ordinary account: files you create appear owned by root inside and by you outside, and the kernel checks permissions against the *outside* IDs, which is why this root cannot read `/etc/shadow`. Runtimes with privileges map a larger range (for example 65,536 IDs from `/etc/subuid`) so that several users can exist inside a rootless container.

**5.**

```text
$ unshare -Ur --pid --fork --mount-proc --uts --net --ipc bash -c 'ls -l /proc/self/ns | awk "NR>1 {print \$9}" | tr "\n" " "; echo'
cgroup ipc mnt net pid pid_for_children time time_for_children user uts
```

Every process has one link per namespace type in `/proc/PID/ns`; two processes are in the same namespace when the link targets are equal. The types are **mnt** (mount table), **pid** (process IDs), **net** (network stack), **uts** (hostname), **ipc** (System V and POSIX message queues, shared memory), **user** (user and group IDs and capabilities), **cgroup** (the view of the cgroup tree) and, since Linux 5.6, **time** (clock offsets). A container normally combines the first seven (mnt, pid, net, uts, ipc, user, cgroup); `time` is rarely used. `lsns` lists namespaces on the system and `nsenter -t PID -n ip addr` runs a command inside another process's namespace, which is how you debug a container from the host.

**6.**

```text
$ systemd-run --user --unit=pr-lim2 --property=TasksMax=20 --property=CPUQuota=25% --property=MemoryMax=64M /bin/sleep 30
Running as unit: pr-lim2.service; invocation ID: ...
$ D=/sys/fs/cgroup$(systemctl --user show pr-lim2 -p ControlGroup --value)
$ for f in pids.max cpu.max memory.max; do printf "%s: " $f; cat $D/$f; done
pids.max: 20
cpu.max: 25000 100000
memory.max: 67108864
```

systemd created a **cgroup** for the unit and wrote the limits into the cgroup v2 control files. `cpu.max` reads *quota period* in microseconds: **25000 of every 100000 µs = 25% of one CPU**. `memory.max` is 64 MiB in bytes. `pids.max` caps the number of processes and threads (a defence against fork bombs). Containers get exactly these files, written by the runtime (`docker run --memory 64m --cpus 0.25 --pids-limit 20`). Now measure a quota:

```text
$ systemd-run --user --unit=pr-cpu2 --property=CPUQuota=20% /bin/sh -c 'while :; do :; done'
$ sleep 4; systemctl --user show pr-cpu2 -p CPUUsageNSec --value
CPU time used in ~4 s of wall time: 0.82 s (limit 20% => about 0.8 s)
```

(The line shown is what the unit consumed, converted from nanoseconds.) A loop that would use 4 CPU-seconds in 4 seconds was **throttled** to 0.82 CPU-seconds, exactly the 20% limit. The three limits behave differently: **`CPUQuota` throttles** (the process runs slower, never dies); **`TasksMax` makes `fork` and thread creation fail** with an error (`EAGAIN`) once the limit is reached; **`MemoryMax` is the dangerous one**: when the group exceeds it and nothing can be reclaimed, the kernel's OOM killer terminates a process *inside that cgroup* (and `memory.high`, the soft limit, throttles instead). That difference is why an application in a container with a too-small memory limit is "OOMKilled" while one with a too-small CPU limit is merely slow.

</details>

## Key takeaways

1. A container is a process in namespaces (visibility: mnt, pid, net, uts, ipc, user, cgroup) plus a cgroup (resource limits); `unshare` creates the first and systemd or a runtime the second.
2. A PID namespace starts numbering at 1 and its PID 1 must reap children and handle signals; a new network namespace has only a down loopback until you configure it.
3. A user namespace maps inside IDs to outside IDs, so container root is an unprivileged user on the host.
4. cgroup v2 limits are plain files (`cpu.max`, `memory.max`, `pids.max`); CPU is throttled, tasks fail to fork, and memory over the limit triggers the OOM killer.
