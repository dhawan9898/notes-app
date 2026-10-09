---
title: A Linux Troubleshooting Playbook
track: linux
module: Performance and Observability
moduleOrder: 9
order: 3
level: advanced
summary: A structured approach and concrete command sequences for the failures seen most often in production — sick servers, hung and zombie processes, full disks (and inodes), resource-limit errors, network failures, kernel messages and boot problems — with what each error really means.
sources:
  - label: "man 3 errno, Linux kernel docs — Documentation/admin-guide (hung task, softlockup, sysctl)"
    url: https://docs.kernel.org/admin-guide/sysctl/kernel.html
  - label: "Gregg — Linux Performance Analysis in 60,000 Milliseconds; Systems Performance, Ch. 2 Methodologies"
    url: https://www.brendangregg.com/Articles/Netflix_Linux_Perf_Analysis_60s.pdf
  - label: "Google SRE Book — Effective Troubleshooting; Postmortem Culture"
    url: https://sre.google/sre-book/effective-troubleshooting/
  - label: "Red Hat — Troubleshooting Linux (kernel oops, sosreport); kernel.org — Bug hunting, Oops tracing"
    url: https://docs.kernel.org/admin-guide/bug-hunting.html
  - label: "man 8 ss, man 8 lsof, man 1 pidstat, man 8 sysctl, man 5 limits.conf"
    url: https://man7.org/linux/man-pages/man8/ss.8.html
---

Troubleshooting is applied scientific method: **observe → form a hypothesis → test it with the cheapest discriminating check → act → verify**. Panic and rituals ("just reboot") destroy evidence and teach nothing. This playbook gives the questions, the commands and the meaning of the usual errors.

## Principles

1. **Preserve evidence before changing anything.** Capture state (`ps`, `ss`, `dmesg`, `journalctl`, `/proc` snapshots, `top -b -n1`, `perf record` for 30 s, thread dumps, core dumps). A restart clears the leak, the stuck thread and the logs.
2. **Define the symptom precisely**: who, what, where (one host, one AZ, all), when it started, how it manifests (error text, latency, crash). Compare a failing and a healthy instance: **differences are the clues**.
3. **Ask what changed**: deploys, config, kernel/package updates (`/var/log/apt/history.log`, `rpm -qa --last`), traffic, data growth, certificates, cron jobs, DNS, time.
4. **Check the cheap, common causes first**: disk full, memory/OOM, a process down, limits, DNS, certificates, clock, then go deeper.
5. **One change at a time**, record it, and be able to roll back. Don't tune kernel parameters blindly.
6. **Read the error message literally** (and the one before it). Most failures print their cause somewhere: `journalctl -xe`, application logs, `dmesg`.
7. **Write it down** (timeline, commands, findings): the postmortem is half done, and the next person benefits.

## The first five minutes on a "sick" server

```bash
uptime ; who ; last -5                        # load trend, who is logged in, recent logins/reboots
dmesg -T | tail -50                           # kernel complaints: OOM, I/O errors, segfaults, NIC resets
journalctl -p err -b --no-pager | tail -50    # errors this boot
systemctl --failed ; systemctl list-jobs
df -hT ; df -i                                # space and inodes
free -m ; vmstat 1 5                          # memory pressure, swap, run queue
top -b -n1 -o %CPU | head -20 ; ps -eo stat,pid,ppid,wchan:24,cmd | awk '$1 ~ /^[DZ]/'   # stuck (D) or zombie (Z) tasks
ss -s ; ss -ltnp                              # sockets, listeners, who owns ports
iostat -xz 1 3 ; ip -s link                   # disk and NIC errors/drops
```

Each answers one USE question; the first anomaly guides the rest. Typical output fragments and meaning: `Tasks: 297 total, 1 running, 283 sleeping, 13 zombie` (zombies exist: find their parent), `load average: 6.2, 5.1, 4.7` (rising: check queue), `Swap: 355 total, 0.1 free` (swap exhausted: memory pressure).

## Symptom → investigation

### 1. High CPU / high load

1. `top`/`pidstat 1`/`mpstat -P ALL 1`: which process, user vs system, all CPUs or one?
2. User time: `perf top`/flame graph; kernel time: `perf trace -s`, `strace -c -p`.
3. Load high but CPU idle → blocked tasks: `ps` `D` state, `iostat`, NFS; `cat /proc/PID/wchan`.
4. Check throttling (`cpu.stat` for cgroups), runaway cron/jobs, `kswapd` (memory), `ksoftirqd` (network packet storm), `jbd2`/`kworker` (writeback).

### 2. Memory exhaustion / OOM

- `dmesg -T | grep -i -E 'killed process|out of memory'`; the OOM log includes the victim, its RSS, and for cgroups the path.
- `ps aux --sort=-rss | head`, `smem -tk`; per-cgroup `memory.current`.
- Distinguish leak (steady growth), burst (traffic), cache (grows to plateau), limit (cgroup `memory.max` < usage).
- Remedy: fix leak/limit, add swap/zram only as buffer, set `oom_score_adj` for critical daemons, `MemoryHigh` before `MemoryMax`.

### 3. Disk full: "No space left on device" (ENOSPC)

The message covers several different conditions:

| Check | Command | Cause / fix |
|---|---|---|
| Blocks | `df -h` | really full: `du -xh --max-depth=1 / \| sort -h`, `ncdu -x /` |
| **Inodes** | `df -i` | millions of tiny files: find with `find / -xdev -type d -size +1M`, `for d in /*; do echo $(find $d -xdev \| wc -l) $d; done \| sort -n` |
| **Deleted but open files** | `lsof +L1`, `lsof -nP \| grep '(deleted)'` | space not freed until the process closes or restarts; truncate via `: > /proc/PID/fd/N` |
| Reserved space | `tune2fs -l`, `df` shows 0 avail at ~95 % | ext4 5 % root reserve |
| tmpfs / ramfs full | `df -h /tmp /run /dev/shm` | memory-backed fs size limit |
| Quotas | `quota -v`, `repquota` | user/project quota |
| Btrfs/ZFS | `btrfs fi usage`, metadata exhausted | `btrfs balance -dusage=50`; snapshots holding space |
| Docker/journal/logs | `docker system df`, `journalctl --disk-usage`, `/var/log` | prune, vacuum, rotate |
| Thin provisioning | `lvs -o+data_percent` | thin pool full: extend pool |

### 4. "Too many open files" and other limit errors

| Error (errno) | Meaning | Check |
|---|---|---|
| `EMFILE` "Too many open files" | process hit `RLIMIT_NOFILE` | `cat /proc/PID/limits`, `ls /proc/PID/fd \| wc -l`; raise `LimitNOFILE=` (systemd) / `ulimit -n`; fix fd leaks (`lsof -p PID`) |
| `ENFILE` | system-wide file table full | `cat /proc/sys/fs/file-nr`; raise `fs.file-max` |
| `EAGAIN` "Resource temporarily unavailable" on `fork`/`pthread_create` | process/thread limit (`RLIMIT_NPROC`, cgroup `pids.max`, `kernel.pid_max`, `threads-max`) | `ulimit -u`, `cat /sys/fs/cgroup/.../pids.current` |
| `ENOMEM` "Cannot allocate memory" | memory, **or** `vm.max_map_count` (mmap count), overcommit policy, cgroup limit, `RLIMIT_AS` | `sysctl vm.max_map_count` (Elasticsearch needs 262144), `/proc/PID/maps \| wc -l` |
| `ENOSPC` on inotify "No space left" | `fs.inotify.max_user_watches`/`max_user_instances` exhausted | `sysctl fs.inotify` |
| `EADDRNOTAVAIL` "Cannot assign requested address" | ephemeral ports exhausted (outgoing connections from one source to the same destination:port) | `ss -tan state time-wait \| wc -l`, `sysctl net.ipv4.ip_local_port_range`; reuse connections/keepalive, widen range, `tcp_tw_reuse=1` for clients |
| `EADDRINUSE` | port already bound or TIME_WAIT without `SO_REUSEADDR` | `ss -ltnp 'sport = :8080'` |
| `EMLINK`, `ENAMETOOLONG`, `EFBIG` | link/ name/ file size limit | filesystem limits, `ulimit -f` |

Limits for a running service: `cat /proc/PID/limits`; for systemd services the unit's `LimitNOFILE=` applies (not `/etc/security/limits.conf`, which PAM applies only to login sessions).

### 5. Hung process / "it just stopped"

```bash
ps -o pid,stat,wchan:30,cmd -p PID           # state and the kernel function it sleeps in
cat /proc/PID/stack                          # kernel stack (root)
cat /proc/PID/syscall                        # current syscall + args
strace -f -p PID                             # blocked in read/futex/connect/epoll_wait?
gdb -p PID -batch -ex 'thread apply all bt' # all thread stacks (user space)
ls -l /proc/PID/fd ; cat /proc/PID/net/tcp   # waiting on which socket/file
```

- State `D` (uninterruptible): in the kernel waiting for I/O: NFS/iSCSI server gone, failing disk, stuck FUSE. Cannot be killed; fix the underlying device/mount (`umount -f -l`, restore the server). `dmesg` shows `INFO: task X:123 blocked for more than 120 seconds` (hung_task).
- `S` + `futex` wait: deadlock or waiting on a lock/condition; `S` + `epoll_wait` idle is normal; `connect()` blocked: network/peer; `read` on a socket with no timeout: add timeouts.
- `Z` zombie: parent not calling `wait`: `ps -eo pid,ppid,stat,cmd | awk '$3 ~ /^Z/'`, then restart/fix the **parent** (killing the zombie does nothing; orphaned zombies are reaped by init).
- `T`: stopped (`SIGSTOP`/Ctrl-Z/ptrace); `kill -CONT`.
- Capture a core for post-mortem: `gcore PID` or `kill -ABRT PID` (if core dumps are configured).

### 6. Service won't start

```bash
systemctl status svc -l ; journalctl -xeu svc --no-pager | tail -50
systemd-analyze verify /etc/systemd/system/svc.service
```

Frequent causes: exit status `203/EXEC` (binary missing/not executable/wrong interpreter, SELinux `noexec`), `200/CHDIR`, `217/USER` (user doesn't exist), `226/NAMESPACE` (sandbox path missing), `address already in use` (`ss -ltnp`), permission denied on a file or socket (`namei -l /path`, SELinux AVC in `ausearch -m avc -ts recent`, AppArmor `DENIED` in `dmesg`), missing config/env file, dependency not ready (use `After=`/retry), `start request repeated too quickly` (start-limit hit: `systemctl reset-failed`), timeouts (`Type=notify` service never signals readiness), read-only filesystem due to `ProtectSystem=strict` without `ReadWritePaths=`. Run the command manually as the service user with the same environment (`sudo -u svcuser env -i … /usr/bin/app`) or with `systemd-run --pty -p User=svcuser …` to reproduce.

### 7. Network problems

Work up the layers:

```bash
ip -br link ; ip -br addr ; ip route get <dst>          # L1-L3 state and chosen route
ping -c3 gw ; ping -c3 8.8.8.8 ; ping -c3 example.com   # gateway -> internet -> DNS
resolvectl query example.com / dig +trace / getent hosts name     # DNS (what libc really uses is getent)
ss -tnp state established '( dport = :443 )' ; ss -ltnp           # sockets
curl -v --connect-timeout 5 https://host/ ; openssl s_client -connect host:443 -servername host    # app/TLS
mtr -n host ; traceroute -T -p 443 host ; tcpdump -ni any host X and port Y -w /tmp/cap.pcap
nft list ruleset ; iptables-save ; conntrack -S ; ip -s link ; ethtool -S eth0 | grep -i -E 'err|drop|miss'
nstat -az | grep -i -E 'Retrans|Drops|Overflow|Timeout'
```

What the failures mean:

| Symptom | Meaning |
|---|---|
| `Connection refused` (RST) | host reachable, nothing listening on that port / firewall REJECT |
| `Connection timed out` (no reply) | packets dropped silently: firewall DROP, routing, security group, down host, asymmetric path |
| `No route to host` / `Network is unreachable` | no route (`ip route`), or ARP failing (`ip neigh`: `FAILED`) |
| `Connection reset by peer` | peer (or middlebox, load balancer idle timeout) sent RST mid-stream |
| `Name or service not known` / slow first request | DNS failure/timeout (check `/etc/resolv.conf`, `nsswitch.conf`, search domains, IPv6 AAAA fallbacks, `ndots:5` in Kubernetes) |
| TLS handshake failure | certificate expired/untrusted/hostname mismatch/clock skew/protocol or cipher mismatch/SNI |
| Works for small, hangs for large transfers | MTU / PMTUD black hole (ICMP blocked) |
| Intermittent, retransmits rising | congestion, bufferbloat, bad NIC/cable, conntrack table full (`dmesg: nf_conntrack: table full`), listen-queue overflow (`ListenDrops`) |
| `SYN_RECV` pile-up | SYN flood or full accept queue (`ss -ltn` Recv-Q > Send-Q on listener) |

### 8. Time, certificates, DNS: the quiet killers

- **Clock skew** breaks TLS, Kerberos, TOTP, cluster protocols, log correlation: `timedatectl`, `chronyc tracking`.
- **Expired certificates**: `openssl x509 -noout -enddate -in cert.pem`, monitor expiry.
- **DNS**: resolver timeouts add 5 s delays; check `time getent hosts name`, `resolvectl statistics`, negative caching, split-horizon.

### 9. Kernel messages worth recognizing

| Message | Meaning |
|---|---|
| `Out of memory: Killed process …` | OOM killer; see memory notes |
| `INFO: task … blocked for more than 120 seconds` | hung task: usually stalled storage/NFS/lock |
| `BUG: soft lockup - CPU#N stuck for 22s!` | a CPU in kernel code without scheduling (driver/bug, VM pauses) |
| `rcu: INFO: rcu_sched self-detected stall` | CPU stuck/overloaded, often under virtualization |
| `EXT4-fs error`, `XFS: Metadata corruption`, `I/O error, dev sdX` | storage/filesystem trouble: stop writing, check hardware (`smartctl`), fsck offline |
| `nf_conntrack: table full, dropping packet` | raise `nf_conntrack_max`, shorten timeouts |
| `TCP: request_sock_TCP: Possible SYN flooding` | SYN backlog overflow/attack |
| `segfault at … ip … in libfoo.so` | application crashed; core dump / `addr2line` / `coredumpctl` |
| `Kernel panic - not syncing`, `Oops`, `general protection fault` | kernel bug or hardware: capture with kdump/pstore, check taint flag, update kernel |
| `Machine check events logged`, `EDAC … CE/UE` | hardware memory/CPU errors: replace DIMM |
| `thermal throttling`, `Package temperature above threshold` | cooling problem |

### 10. Boot problems

See the boot note: use the GRUB editor (`systemd.unit=rescue.target`, `rd.break`), check `/etc/fstab` (use `nofail`), initramfs drivers, `journalctl -b -1` for the prior boot, `systemd-analyze blame`/`critical-chain` for slowness.

## Capturing evidence

```bash
mkdir /root/incident-$(date +%F-%H%M) && cd $_
dmesg -T > dmesg.txt ; journalctl --since "-2h" > journal.txt
ps auxwwf > ps.txt ; ss -tanp > ss.txt ; lsof -nP > lsof.txt 2>/dev/null
top -b -n1 -H > top.txt ; vmstat 1 10 > vmstat.txt ; iostat -xz 1 10 > iostat.txt
cp /proc/meminfo /proc/vmstat /proc/net/snmp /proc/interrupts .
perf record -F 99 -a -g -o perf.data -- sleep 20       # CPU profile (if the box is slow, not dead)
sosreport / sos report   # RHEL/Ubuntu: collects config and logs for support
```

Also: heap/thread dumps (`jcmd PID Thread.print`, `kill -3`, `py-spy dump`), tcpdump of the failing flow, and the exact error and time.

## Things not to do

- Don't run `echo 3 > /proc/sys/vm/drop_caches` as a "fix" (it throws away useful cache and masks the real issue).
- Don't `kill -9` a database or a process in `D` state without understanding consequences (corruption, no effect).
- Don't `rm` large log files being written (space isn't freed; truncate instead: `: > file`).
- Don't disable SELinux/AppArmor/firewall as the fix; find the denial (`ausearch`, `audit2allow` review) or rule.
- Don't change many sysctls at once or paste "performance tuning" lists from the internet.
- Don't reboot as the first step on a mystery problem: you lose volatile evidence (but do restore service if the SLA demands, after capturing).

## After the incident

Write a blameless post-incident review: timeline, impact, root cause(s) and contributing factors, detection (how long until noticed?), what worked, action items (monitoring for the missing signal, alert on saturation not just failure, runbook, automation, capacity limits). Convert the commands you wished you had into dashboards and alerts.

## Key takeaways

- Follow a method: precise symptom → what changed → cheap discriminating checks → one change at a time, preserving evidence first.
- The first-five-minutes sweep (dmesg, journal, df -h/-i, free/vmstat, D/Z processes, ss, iostat) finds most problems.
- Learn what errors really mean: ENOSPC may be blocks, inodes, deleted-open files, tmpfs or quota; EMFILE vs ENFILE; "refused" vs "timed out" vs "reset"; D vs Z states.
- systemd service failures usually have a status code and journal line; reproduce as the same user and environment.
- Document and automate: every incident should improve monitoring and runbooks.
