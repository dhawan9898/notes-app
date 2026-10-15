---
title: Container Resource Limits, Seccomp, Capabilities and Hardening
track: linux
module: Containers
moduleOrder: 8
order: 2
level: advanced
summary: How Docker/Kubernetes limits translate to cgroup files, what OOMKilled and CPU throttling really mean, and how capabilities, seccomp, no-new-privileges, LSMs and user namespaces shrink the blast radius — with a real container's security state.
prerequisites:
  - Users, Permissions, ACLs and Capabilities
  - systemd Timers, cgroups v2 and Resource Control
  - Linux Namespaces — The Isolation Primitive Behind Containers
sources:
  - label: "Docker — Resource constraints; Runtime privilege and Linux capabilities; Seccomp security profiles"
    url: https://docs.docker.com/engine/security/seccomp/
  - label: "Kubernetes — Resource Management for Pods and Containers; Pod QoS; Pod Security Standards"
    url: https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
  - label: "man 2 seccomp, man 7 capabilities, man 2 prctl (PR_SET_NO_NEW_PRIVS)"
    url: https://man7.org/linux/man-pages/man2/seccomp.2.html
  - label: "OCI Runtime Specification — Linux container configuration"
    url: https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md
  - label: "NIST SP 800-190 — Application Container Security Guide"
    url: https://csrc.nist.gov/publications/detail/sp/800-190/final
  - label: "CIS Docker and Kubernetes Benchmarks"
    url: https://www.cisecurity.org/benchmark/docker
---

Namespaces decide what a container can *see*. This note covers what it can *use* (cgroups) and what it may *do* (capabilities, seccomp, MAC, privilege flags).

## Resource limits → cgroup v2

| Docker flag | Kubernetes | cgroup v2 file | Meaning |
|---|---|---|---|
| `--memory 512m` | `limits.memory` | `memory.max` | hard limit; exceeding it triggers cgroup OOM kill |
| `--memory-reservation` | `requests.memory` (via QoS/`memory.min`/`low` in newer versions) | `memory.low` | soft protection |
| `--memory-swap` | (swap off by default) | `memory.swap.max` | swap allowance |
| `--cpus 1.5` | `limits.cpu: 1500m` | `cpu.max` = `150000 100000` | quota 150 ms per 100 ms period |
| `--cpu-shares 512` | `requests.cpu` | `cpu.weight` | proportional share under contention (shares 1024 ≈ weight 100, converted) |
| `--cpuset-cpus 0-3` | CPU manager `static` policy (Guaranteed pods) | `cpuset.cpus` | pinning |
| `--pids-limit 200` | `podPidsLimit` | `pids.max` | fork-bomb protection |
| `--device-read-bps`, `--blkio-weight` | (CSI/IO controls) | `io.max`, `io.weight` | block I/O |
| `--ulimit nofile=65536` | | rlimits via the runtime | not cgroups |

Everything is a file under `/sys/fs/cgroup/<path>` (see the systemd/cgroups notes); the runtime creates a cgroup per container and writes these values.

### Kubernetes QoS and OOM

Pods are classified by their requests/limits:

| QoS class | Rule | `oom_score_adj` |
|---|---|---|
| **Guaranteed** | every container has requests == limits for CPU and memory | −997 |
| **Burstable** | at least one request or limit, not Guaranteed | 2…999 (scaled by memory request/node capacity) |
| **BestEffort** | none | 1000 |

Under node memory pressure the kubelet **evicts** BestEffort/Burstable pods first (by usage over requests); the kernel OOM killer prefers high `oom_score_adj`. **`OOMKilled` (exit code 137)** means the container's cgroup hit `memory.max` and the kernel killed a process in it: confirm with `kubectl describe pod` (Last State), node `dmesg` (`Memory cgroup out of memory`), and the cgroup's `memory.events` (`oom_kill`).

### Common surprises

- **Memory limit counts page cache and kernel memory**, not only heap. A service that logs or reads large files can approach the limit with reclaimable cache (reclaimed before OOM, but `memory.high` throttling may slow it). Watch *working set* (`container_memory_working_set_bytes`), which excludes inactive file cache.
- **Language runtimes must be container-aware**: JVM ≥ 10 (and 8u191+) reads cgroup limits (`-XX:MaxRAMPercentage=75`); Go ≥ 1.19 supports `GOMEMLIMIT`; `GOMAXPROCS` historically followed the *host* CPU count (use `automaxprocs` or Go 1.25+ cgroup-aware defaults); Node (`--max-old-space-size`), Python worker counts, nginx `worker_processes auto` read host CPUs from `/proc/cpuinfo` unless fixed. Result: too many threads, OOMs, throttling.
- **CPU limit throttling**: with `cpu.max` quota, a multithreaded service on a big host burns its 100 ms quota in a few milliseconds and then sleeps. Symptoms: latency spikes with low average CPU; counters `nr_throttled`/`throttled_usec` in `cpu.stat` (`container_cpu_cfs_throttled_periods_total`). Mitigate: raise limits or drop limits and rely on requests/weights, reduce thread counts, size by `ceil(peak parallelism)`.
- **/proc and /sys inside the container** still report host totals (`free`, `nproc`, `top` show the host). Tools that decide from them over-allocate. `lxcfs` or runtime-specific virtualization helps; prefer reading `/sys/fs/cgroup/memory.max` and `cpu.max`.
- **Ephemeral storage** (writable layer, logs, `emptyDir`) is limited separately by the kubelet (`limits.ephemeral-storage`), causing evictions.
- **Shared memory**: Docker's `/dev/shm` defaults to 64 MB, which breaks browsers/PyTorch dataloaders (`--shm-size`).

## Capabilities

Containers start with a *reduced* bounding set rather than full root power. Docker's default set (14): `CHOWN, DAC_OVERRIDE, FSETID, FOWNER, MKNOD, NET_RAW, SETGID, SETUID, SETFCAP, SETPCAP, NET_BIND_SERVICE, SYS_CHROOT, KILL, AUDIT_WRITE`; Kubernetes inherits the runtime default unless `securityContext.capabilities` changes it. Notably missing: `SYS_ADMIN`, `NET_ADMIN`, `SYS_MODULE`, `SYS_PTRACE`, `SYS_TIME`, `SYS_RAWIO`, `DAC_READ_SEARCH`.

A real rootless `runc` container started from its default generated spec (bounding set `CAP_AUDIT_WRITE, CAP_KILL, CAP_NET_BIND_SERVICE`), showing process status from the inside:

```text
CapEff:   0000000020000420        <- bits 5 (CAP_KILL), 10 (CAP_NET_BIND_SERVICE), 29 (CAP_AUDIT_WRITE)
CapBnd:   0000000020000420
NoNewPrivs:  1
Seccomp:  0                       <- this minimal spec has no seccomp profile (Docker applies one by default)
uid_map:  0 1000 1           <- root inside is an ordinary user outside (user namespace)
```

compared with an unconfined root process on the host: `CapBnd: 000001ffffffffff` (all 41 capabilities). Decode with `capsh --decode=20000420`.

Best practice: **drop everything, add back only what is needed**:

```bash
docker run --cap-drop=ALL --cap-add=NET_BIND_SERVICE --security-opt no-new-privileges:true \
           --read-only --tmpfs /tmp --user 10001:10001 myimage
```

```yaml
securityContext:           # Kubernetes
  runAsNonRoot: true
  runAsUser: 10001
  allowPrivilegeEscalation: false      # sets no_new_privs
  readOnlyRootFilesystem: true
  capabilities: { drop: ["ALL"], add: ["NET_BIND_SERVICE"] }
  seccompProfile: { type: RuntimeDefault }
```

**`--privileged`** (all capabilities, all devices, no seccomp/LSM confinement) essentially gives host root: avoid it. Dangerous grants: `CAP_SYS_ADMIN` (mount, namespaces, bpf, many ioctls), `CAP_SYS_PTRACE`, `CAP_NET_ADMIN`, `CAP_SYS_MODULE`, `CAP_DAC_READ_SEARCH` (open_by_handle escape), host namespaces (`--pid=host`, `--net=host`), host mounts (`/`, `/var/run/docker.sock`, `/proc`, `/sys`), `--device`.

## no_new_privs

`prctl(PR_SET_NO_NEW_PRIVS, 1)` is a one-way flag inherited by children: `execve` can no longer grant privileges (setuid bits, file capabilities, LSM transitions are ignored). It also allows unprivileged processes to install seccomp filters. Show with `grep NoNewPrivs /proc/PID/status`. Docker `no-new-privileges`, Kubernetes `allowPrivilegeEscalation: false`, systemd `NoNewPrivileges=yes`.

## Seccomp

Seccomp-BPF attaches a filter (classic BPF program) to a process that runs on **every syscall** with the syscall number, architecture and arguments (not pointers' contents), returning an action:

| Action | Effect |
|---|---|
| `SECCOMP_RET_ALLOW` | permitted |
| `SECCOMP_RET_ERRNO` | fail with a chosen errno (`EPERM`): apps handle gracefully |
| `SECCOMP_RET_KILL_PROCESS` / `KILL_THREAD` | terminate (SIGSYS) |
| `SECCOMP_RET_TRAP` | deliver `SIGSYS` to the process |
| `SECCOMP_RET_LOG` | allow but log (audit) |
| `SECCOMP_RET_USER_NOTIF` | forward to a supervisor process (e.g. emulate `mount` for unprivileged containers) |
| `SECCOMP_RET_TRACE` | tell a ptracer |

Filters stack (the most restrictive result wins), cannot be removed, and are inherited across `fork`/`execve`. Mode in `/proc/PID/status`: `Seccomp: 0` none, `1` strict (only read/write/_exit/sigreturn), `2` filter.

**Docker's default profile** is an allow-list that blocks ~44 syscalls of 300+, including `kexec_load`, `reboot`, `init_module`/`finit_module`, `mount`/`umount2` (without CAP_SYS_ADMIN), `swapon`, `bpf`, `perf_event_open`, `ptrace` (older kernels), `keyctl`, `add_key`, `unshare`/`clone` with new namespace flags, `open_by_handle_at`, `settimeofday`. Kubernetes `seccompProfile: RuntimeDefault` applies the runtime's profile (not default in older clusters; set it explicitly). Custom profiles are JSON:

```json
{ "defaultAction": "SCMP_ACT_ERRNO",
  "architectures": ["SCMP_ARCH_X86_64"],
  "syscalls": [ { "names": ["read","write","openat","close","futex","epoll_wait","accept4"], "action": "SCMP_ACT_ALLOW" } ] }
```

Generate profiles by recording what an app uses (`strace -c -f`, `oci-seccomp-bpf-hook`, `inspektor-gadget`'s seccomp advisor, `syscall2seccomp`) in tests that cover all code paths, then enforce with `SCMP_ACT_ERRNO` for unknown calls and review failures. Seccomp reduces kernel attack surface (a large fraction of kernel CVEs are unreachable if the syscall is blocked) but doesn't inspect pointer arguments (time-of-check issues), so it complements rather than replaces other controls.

## LSM: AppArmor and SELinux

Mandatory access control policies restrict container processes even as root inside: Docker's `docker-default` AppArmor profile (denies writes to `/proc/sys`, mounts, ptrace of others) on Ubuntu/Debian; **SELinux** `container_t` type with per-container MCS labels (`c123,c456`) on RHEL/Fedora prevents containers from reading each other's files or host files (volume mounts need `:z`/`:Z` relabeling). Do not disable them (`--security-opt apparmor=unconfined`, `label=disable`) to "fix" permission errors; fix the policy or the label.

## Other hardening controls

- **Run as non-root** inside the image (`USER 10001`), no setuid binaries; combine with **user namespaces** (`userns-remap`, rootless Podman/Docker, Kubernetes `hostUsers: false`) so that even in-container root is unprivileged on the host.
- **Read-only root filesystem** + explicit writable `tmpfs`/volumes; `masked` and `readonly` paths (`/proc/kcore`, `/proc/sys`, `/sys/firmware`) are set by the runtime.
- **Device cgroup** whitelists device nodes (`/dev/null`, `/dev/zero`, `/dev/urandom`, `/dev/tty`…).
- **Network policy** (Kubernetes `NetworkPolicy`, Cilium), no host networking, egress control.
- **Image hygiene**: minimal/distroless bases, pin digests, scan (Trivy/Grype), sign (cosign), SBOMs, no secrets in layers, rebuild for CVEs.
- **Kubernetes Pod Security Standards**: *privileged*, *baseline* (blocks known escalations: hostPath, hostNetwork, privileged, dangerous capabilities), *restricted* (non-root, drop ALL, seccomp RuntimeDefault, no privilege escalation): enforce at least baseline via Pod Security Admission.
- **Stronger isolation** for untrusted tenants: gVisor (`runsc`, user-space kernel intercepting syscalls), Kata Containers / Firecracker (each pod in a lightweight VM), confidential-VM runtimes.
- **Runtime detection**: Falco/Tetragon (eBPF) alert on unexpected exec, sensitive file reads, namespace changes.

## Escape classes to know

| Class | Example | Defense |
|---|---|---|
| Mounted Docker/containerd socket | anyone with `/var/run/docker.sock` is host root (start a privileged container) | never mount; use rootless sidecars, socket proxies |
| Excess capabilities / privileged | `CAP_SYS_ADMIN` + cgroup `release_agent` (CVE-2022-0492), mounting host devices | drop caps, seccomp, AppArmor/SELinux |
| Kernel vulnerabilities | Dirty COW (2016), Dirty Pipe (2022), overlayfs CVE-2021-3493/2023-0386 | patch kernels, seccomp, user namespaces limits, sandboxed runtimes |
| Runtime vulnerabilities | runc CVE-2019-5736 (overwrite host runc via `/proc/self/exe`) | update runtimes, read-only/immutable binaries, user namespaces |
| Host namespace/path sharing | `--pid=host`, `hostPath: /`, `--net=host` | policy to forbid |
| Shared-kernel info leaks | `/proc`, `/sys`, dmesg, speculative execution | masked paths, `kernel.dmesg_restrict`, patches |

## Key takeaways

- Docker/K8s limits are cgroup files: memory → `memory.max` (hard, OOMKilled/137), CPU limit → `cpu.max` (throttling), requests → weights/QoS.
- Make runtimes container-aware (JVM, Go, Node); `/proc/meminfo` and `nproc` show the host.
- Defaults are not hardening: drop all capabilities, set `no-new-privileges`, use a seccomp profile and AppArmor/SELinux, run as non-root, read-only root, avoid `--privileged` and host mounts.
- A real container showed `CapEff 0x20000420` (3 capabilities), `NoNewPrivs 1`, and root mapped to an unprivileged host UID.
- For hostile multi-tenant code use user-space kernels or microVMs; namespaces plus cgroups are not enough.
