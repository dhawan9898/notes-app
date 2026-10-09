# Linux track — topic map

Status: [x] written, [ ] planned. Module order = `moduleOrder`.

## 1. Boot and Foundations
- [x] The boot process: firmware, bootloader, kernel, initramfs, init
- [x] Filesystem hierarchy, VFS, inodes, links and file descriptors
- [x] Users, permissions, ACLs and capabilities

## 2. Shell and Text Processing
- [x] The shell: parsing, expansion, redirection, pipes and job control
- [x] grep, sed, awk and friends; robust shell scripting

## 3. Processes
- [x] Process lifecycle: fork, exec, wait
- [x] The scheduler: CFS/EEVDF, priorities, real-time and CPU affinity
- [x] Signals in depth
- [x] IPC: pipes, FIFOs, shared memory, sockets, futexes
- [x] Threads and synchronization

## 4. Memory
- [x] Virtual memory, paging and the page cache
- [x] mmap, overcommit, the OOM killer, huge pages, NUMA, swap

## 5. Storage and Filesystems
- [x] The block layer, I/O schedulers and NVMe
- [x] ext4, XFS, Btrfs, journaling and durability (fsync)
- [x] LVM, software RAID, device-mapper and LUKS
- [x] Event-driven I/O: select/poll/epoll and io_uring

## 6. Kernel Interfaces
- [x] System calls, vDSO, strace, /proc and /sys
- [x] Kernel modules, devices and udev
- [x] eBPF

## 7. systemd and Services
- [x] systemd units, targets, dependencies and journald
- [x] Timers, cgroups v2 and resource control

## 8. Containers
- [x] Namespaces
- [x] cgroups v2, seccomp and capabilities in containers
- [x] Container runtimes and overlay filesystems

## 9. Performance and Observability
- [x] Performance methodology: USE, load average, CPU and memory analysis
- [x] perf, ftrace and bpftrace; flame graphs
- [x] A troubleshooting playbook

## 10. Network Stack
- [x] The packet path: NIC, NAPI, softirq, netfilter, qdisc
- [x] Sockets, TCP behavior and sysctl tuning

## 11. Security
- [x] SSH, PAM, sudo, auditd
- [x] SELinux, AppArmor, seccomp and hardening
