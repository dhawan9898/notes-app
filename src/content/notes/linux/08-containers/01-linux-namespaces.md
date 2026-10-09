---
title: Linux Namespaces — The Isolation Primitive Behind Containers
track: linux
module: Containers
moduleOrder: 8
order: 1
level: advanced
summary: What each of the eight namespace types isolates, how they are created and joined, user-namespace UID mapping and rootless containers, with real unshare experiments and the limits of namespace isolation.
sources:
  - label: "man 7 namespaces, man 7 user_namespaces, man 7 pid_namespaces, man 7 network_namespaces, man 7 mount_namespaces, man 7 cgroup_namespaces, man 7 time_namespaces"
    url: https://man7.org/linux/man-pages/man7/namespaces.7.html
  - label: "man 2 clone, man 2 unshare, man 2 setns, man 8 nsenter, man 1 unshare, man 8 lsns, man 8 ip-netns"
    url: https://man7.org/linux/man-pages/man2/unshare.2.html
  - label: "Kerrisk — Namespaces in operation (LWN series, 2013–2016)"
    url: https://lwn.net/Articles/531114/
  - label: "Linux kernel docs — Shared Subtrees (mount propagation)"
    url: https://docs.kernel.org/filesystems/sharedsubtree.html
  - label: "Rice — Container Security (O'Reilly, 2020)"
---

A **container** is not a kernel object. It is an ordinary process (tree) that the kernel is told to give a *restricted view of the system* — through **namespaces** (what you can see), **cgroups** (how much you can use) and security filters (**capabilities, seccomp, LSMs**: what you may do) — plus a root filesystem image. Namespaces are the first and most visible piece.

A **namespace** wraps a global kernel resource so that processes inside see their own isolated instance. Each process belongs to exactly one namespace of each type.

| Namespace | Flag | Isolates | Since |
|---|---|---|---|
| **Mount** (`mnt`) | `CLONE_NEWNS` | the mount table / filesystem view | 2.4.19 |
| **UTS** | `CLONE_NEWUTS` | hostname and NIS domain | 2.6.19 |
| **IPC** | `CLONE_NEWIPC` | SysV IPC objects, POSIX message queues | 2.6.19 |
| **PID** | `CLONE_NEWPID` | process ID numbers | 2.6.24 |
| **Network** (`net`) | `CLONE_NEWNET` | interfaces, IP addresses, routes, firewall rules, ports, sockets, `/proc/net` | 2.6.29 |
| **User** | `CLONE_NEWUSER` | UIDs/GIDs and capabilities | 3.8 |
| **Cgroup** | `CLONE_NEWCGROUP` | the cgroup root a process sees | 4.6 |
| **Time** | `CLONE_NEWTIME` | `CLOCK_MONOTONIC`/`BOOTTIME` offsets | 5.6 |

## Creating, joining and inspecting

- **`clone(flags)`** creates a child already in new namespaces; **`unshare(flags)`** moves the calling process into new ones; **`setns(fd, type)`** joins an existing namespace through an fd (`/proc/PID/ns/<type>`).
- Every process's namespaces are links in `/proc/PID/ns/`; two processes share a namespace iff the inode numbers match. A namespace lives while a process is in it, an open fd to it exists, or a bind mount of the `ns` file pins it (this is what `ip netns add` does with `/run/netns/NAME`).

```bash
$ ls -l /proc/self/ns
ipc  mnt  net  pid  pid_for_children  time  time_for_children  user  uts      # (cgroup too on newer kernels)
$ lsns                              # list namespaces and their member processes
        NS TYPE   NPROCS   PID USER    COMMAND
4026531836 pid        71  2733 user    /usr/lib/systemd/systemd --user
$ unshare --pid --fork --mount-proc --uts --user --map-root-user sh      # create several at once
$ nsenter -t 1234 -n -p ip addr     # enter the net+pid namespaces of process 1234 (debug a container)
```

## What each namespace does

### UTS

Separate hostname/domainname. Real experiment (new user + PID + UTS + mount namespaces):

```text
$ unshare -Urpf --mount-proc --uts sh -c 'hostname demo-container; echo "hostname: $(hostname)"; echo "pid: $$"; ps -eo pid,user,comm'
hostname: demo-container
pid: 1
    PID USER     COMMAND
      1 root     sh
      9 root     ps
$ hostname        # in the host, unchanged
dhawan-vm.example.com
```

### PID

Processes in a PID namespace get their own numbering starting at 1; the first process is **PID 1 of that namespace**. It cannot see processes outside, though the parent namespace sees all of them under different (real) PIDs. Properties:

- PID 1 is special: it **reaps orphans**, ignores signals without handlers, and if it dies the kernel kills every other process in the namespace.
- Namespaces nest; `pid_for_children` is the namespace future children will join (after `unshare(CLONE_NEWPID)`, the caller itself stays in the old one, hence `unshare --fork`).
- `ps`, `top` read `/proc`, which shows the **PID namespace of the mounting process**: a new PID namespace needs a fresh `/proc` mount (`--mount-proc`), otherwise tools show host processes.
- Signals between namespaces: the parent can kill children, children cannot signal outside.

### Mount

Each mount namespace has its own mount table, so mounts/unmounts inside are invisible outside (if propagation allows). A container starts from a **new mount namespace**, mounts its image rootfs and `/proc`, `/sys`, `/dev`, then `pivot_root`s into it (`chroot` is weaker: processes with `CAP_SYS_CHROOT` can escape; `pivot_root` removes the old root).

**Propagation** (`man 7 mount_namespaces`) decides whether mounts leak between namespaces: `shared` (events propagate both ways), `private`, `slave` (receive but don't send), `unbindable`. systemd makes `/` shared by default, so a runtime's first step is `mount --make-rprivate /`. Bind mounts of host paths into containers (volumes) with `rshared`/`rslave` allow e.g. a CSI driver's mounts to appear in pods.

### Network

A network namespace has its own network devices, routing tables, ARP/neighbor tables, netfilter rules, sockets and port space, `/proc/net`, `/sys/class/net`. A new one contains only a down loopback; real devices reach it via **veth pairs** (one end in each namespace), moved physical NICs, macvlan/ipvlan, or bridges.

```text
$ unshare -Urn sh -c 'ip -br link'
lo      DOWN   00:00:00:00:00:00 <LOOPBACK>
gre0@NONE  DOWN ...                              # default tunnel devices appear when modules are loaded
```

Container networking typically: create netns → veth pair → host end attached to a bridge (`docker0`, `cni0`) → NAT/routing for egress, DNAT for published ports. Kubernetes pods share **one** network namespace between their containers (the "pause" container holds it), so containers in a pod reach each other on `localhost`. See the Linux labs note for building this by hand (`ip netns`, `veth`).

### User

The most powerful and security-relevant. A user namespace has its own UID/GID numbering **mapped** onto the parent's; a process can be **root (UID 0) inside** while being an unprivileged user outside. Capabilities are checked **relative to the namespace that owns the resource**: root in a user namespace has all capabilities *over resources owned by that namespace* (it can create further namespaces, mount tmpfs/overlay/proc, set the hostname of its UTS namespace), but none over host-owned resources (it can't load modules or read host files it couldn't before).

```text
$ unshare -Urpf --mount-proc sh -c 'id -u; cat /proc/self/uid_map'
0
         0  1000          1            # inside UID 0 = outside UID 1000, a range of length 1
$ unshare -U sh -c id                        # without a mapping
uid=65534(nobody) gid=65534(nogroup) groups=65534(nogroup)     # the "overflow" ID
```

`/proc/PID/uid_map` lines are `inside-start outside-start length`. Writing them for ranges beyond your own UID requires privileged helpers `newuidmap`/`newgidmap` driven by `/etc/subuid` and `/etc/subgid` (this is how **rootless Podman/Docker** give containers 65,536 IDs). File ownership on disk follows the *outside* IDs; images with files owned by many UIDs need these ranges or an ID-mapped mount.

Security note: user namespaces enable unprivileged users to reach large kernel attack surfaces (netfilter, mount, overlayfs as root-in-ns), which led to many CVEs. Mitigations: `user.max_user_namespaces=0` (breaks rootless containers, Chrome sandbox), Debian's `kernel.unprivileged_userns_clone`, Ubuntu's AppArmor restriction of unprivileged userns (23.10+), and LSM hooks.

### IPC, cgroup, time

- **IPC**: separate SysV shared memory, semaphores, message queues, and `/dev/mqueue`; stops containers from seeing each other's `ipcs -m`.
- **Cgroup**: the process sees its own cgroup as `/`, hiding the host path hierarchy (`/proc/self/cgroup` shows `0::/`) and giving per-container views of `/sys/fs/cgroup`.
- **Time**: per-namespace offsets to monotonic and boot-time clocks (checkpoint/restore, migrating a container while preserving uptime). Wall-clock time is *not* namespaced.

## Building a container by hand

```bash
mkdir -p rootfs/{bin,proc,dev,tmp}; cp /bin/busybox rootfs/bin; # + libs if dynamic, + ln -s busybox for applets
sudo unshare --mount --uts --ipc --net --pid --fork --cgroup \
     sh -c 'mount --make-rprivate /; mount --bind rootfs rootfs; mount -t proc proc rootfs/proc; \
            hostname box; exec chroot rootfs /bin/busybox sh'
```

Add a cgroup for limits, drop capabilities, apply seccomp, and attach a veth: that list is exactly what `runc` does (next notes).

## What namespaces do not do

1. **They are not a security boundary on their own.** Containers share the host kernel: a kernel bug, an excess capability, a writable `/proc`, or a mounted `docker.sock` breaks isolation. Layer capabilities, seccomp, LSM, user namespaces, read-only roots, or use microVMs (Kata, Firecracker) / user-space kernels (gVisor) for hostile code.
2. Not everything is namespaced: the **kernel keyring**, `/proc/sys` tunables (except per-ns net/ipc/uts keys), system time, `dmesg`, kernel modules, `/sys` details, and **resource accounting/limits** (that's cgroups). `/proc/meminfo`, `/proc/cpuinfo`, `uptime` show host values unless virtualized (lxcfs): why some applications miscompute available memory or CPUs in containers.
3. Device access is a separate control (device cgroup, `/dev` contents).
4. Namespaces give isolation but not performance isolation: noisy neighbors need cgroups, CPU pinning, and I/O control.

## Debugging recipes

| Task | Command |
|---|---|
| Which namespaces is a process in? | `ls -l /proc/PID/ns`, `lsns -p PID` |
| Run a host tool inside a container's network | `nsenter -t PID -n ss -tulpn`, or `ip netns exec` after linking `/run/netns/x` to `/proc/PID/ns/net` |
| Inspect a container's filesystem from the host | `ls /proc/PID/root/`, `nsenter -t PID -m` |
| Find a container's PID | `docker inspect -f '{{.State.Pid}}' name`, `crictl inspect` |
| Is something sharing the host namespace? | compare `readlink /proc/PID/ns/net` with `/proc/1/ns/net` |
| Why can't I mount inside? | missing `CAP_SYS_ADMIN` in the *owning user namespace*, or propagation/`nosuid` flags |

## Key takeaways

- A container = namespaces (view) + cgroups (limits) + security filters + an image; namespaces are just per-process pointers to isolated kernel resources.
- The eight types: mnt, uts, ipc, pid, net, user, cgroup, time; created with `clone`/`unshare`, joined with `setns`, visible in `/proc/PID/ns`.
- PID 1 semantics and a fresh `/proc` matter; veth/bridges connect network namespaces; user namespaces map UIDs and scope capabilities, enabling rootless containers.
- Namespaces share the host kernel and do not limit resources or protect against kernel bugs; combine them with the controls in the next notes.
