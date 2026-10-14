---
title: "Processes, Monitoring and Scheduled Jobs"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 2
level: beginner
summary: "What a process is, listing and reading them with ps and top, background jobs, signals and kill, exit statuses, how to see CPU, memory and load, and running things later with cron and systemd timers, using captured output and clearly marked typical output."
prerequisites:
  - Linux Command Line Basics
sources:
  - label: "Shotts — The Linux Command Line, Ch. 10 (Processes) and Ch. 24 (Job control)"
    url: https://linuxcommand.org/tlcl.php
  - label: "man 1 ps, man 1 top, man 7 signal, man 1 kill, man 5 crontab"
    url: https://man7.org/linux/man-pages/man7/signal.7.html
  - label: "procps-ng documentation (ps, top, free, uptime)"
    url: https://gitlab.com/procps-ng/procps
  - label: "systemd.timer(5)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.timer.html
---

A **program** is a file on disk. A **process** is a running instance of it: the program's code plus its memory, open files and an identity. Every command you type starts at least one process, and a Linux machine runs hundreds at once. Knowing how to list them, watch their resource use, stop the ones that misbehave and schedule new ones is the heart of day-to-day administration. (The kernel side, `fork`, `exec` and the scheduler, is in *Process Lifecycle — fork, exec, wait* and *The Linux Scheduler*.)

## What identifies a process

- A **PID**, a number unique among running processes.
- A **PPID**, the PID of its **parent**: whichever process started it. Your shell is the parent of every command you run. PID 1 (`systemd` or `init`) is the ancestor of everything.
- An **owner** (the user it runs as) and an **exit status** when it ends: 0 means success, anything else means failure.

```text
$ ps -o pid,ppid,comm -p $$
    PID    PPID COMMAND
3960197 3960195 bash
```

`$$` is the shell's own PID. Here the shell has a PID and a parent (typically a terminal program or an SSH daemon).

## Listing processes with `ps`

`ps` prints a snapshot. The most useful forms: `ps aux` (every process, BSD style), `ps -ef` (every process, full format) and `ps -o` to choose your own columns:

```text
$ sleep 300 &
[1] 3960198
$ ps -o pid,ppid,stat,etime,cmd -p 3960198
    PID    PPID STAT     ELAPSED CMD
3960198 3960197 S          00:00 sleep 300
```

The **STAT** column is the state: `R` running or ready, `S` sleeping (waiting for something, the normal state of most processes), `D` uninterruptible sleep (usually waiting for disk or network I/O; cannot be killed until it returns), `T` stopped, `Z` zombie (finished, but its parent has not collected its exit status yet). Find processes by name with `pgrep -a name` rather than parsing `ps | grep`.

## Background jobs and job control

Adding `&` runs a command in the **background** so the shell prompt returns at once. The shell tracks these as numbered **jobs**:

```text
$ sleep 300 &
$ jobs
[1]+  Running                 sleep 300 &
```

- `Ctrl+C` interrupts the foreground job (sends SIGINT). `Ctrl+Z` suspends it (SIGTSTP).
- `bg %1` resumes a suspended job in the background; `fg %1` brings it back to the foreground.
- A background job still belongs to your terminal and is usually killed when the terminal closes. Prefix with `nohup` (`nohup long-task > out.log 2>&1 &`) or use `tmux`/`screen` to keep work running after you log out. For services that must always run, use systemd (*systemd — Units, Dependencies, Targets, Hardening and the Journal*).

## Signals and `kill`

A **signal** is a small message the kernel delivers to a process. Despite its name, `kill` just sends a signal; what happens next depends on the signal and on the process:

| Signal | Number | Meaning | Can be ignored? |
|---|---|---|---|
| `SIGTERM` | 15 | "please terminate" (the default for `kill`); lets the program clean up | yes |
| `SIGINT` | 2 | interrupt (`Ctrl+C`) | yes |
| `SIGHUP` | 1 | terminal closed; many daemons reload their configuration on it | yes |
| `SIGSTOP` / `SIGCONT` | 19 / 18 | pause / resume | **no** (SIGSTOP cannot be caught) |
| `SIGKILL` | 9 | terminate immediately, no cleanup | **no** |

```text
$ sleep 1000 &
$ kill -STOP 3960200; ps -o pid,stat,cmd -p 3960200
    PID STAT CMD
3960200 T    sleep 1000
$ kill -CONT 3960200; ps -o pid,stat,cmd -p 3960200
    PID STAT CMD
3960200 S    sleep 1000
$ kill -TERM 3960200; wait 3960200; echo "after TERM: $?"
after TERM: 143
```

`T` means stopped; after `SIGCONT` the process is back to `S`. The status 143 is **128 + 15**: the shell reports "killed by signal 15" as 128 plus the signal number. **Always try `kill PID` (SIGTERM) first and `kill -9 PID` only if the process ignores it**; SIGKILL denies the program the chance to save files and remove lock files. To signal by name use `pkill name`.

### Exit statuses

Every command leaves a status in `$?`: `0` is success, anything else is failure (`1` is general failure, `2` often means misuse, `126` "found but not executable", `127` "command not found", `128+N` killed by signal N). Shell scripts and `&&` / `||` rely on this:

```text
$ false; echo "false gave $?"
false gave 1
$ true; echo "true gave $?"
true gave 0
```

## Watching the machine: `top`, load and memory

`top` is a live view, refreshed every few seconds (`q` quits, `P` sorts by CPU, `M` by memory, `k` kills a process). Its header answers "is this machine healthy?". Typical output (yours will differ):

```text
top - 10:46:51 up 14 days, 14:50,  1 user,  load average: 0.08, 0.24, 0.28
Tasks: 355 total,   1 running, 318 sleeping,   0 stopped,  36 zombie
%Cpu(s):  2.2 us,  0.0 sy,  0.0 ni, 96.7 id,  0.0 wa,  0.0 hi,  0.0 si,  1.1 st
MiB Mem :  20006.2 total,   2086.9 free,   7731.2 used,  11563.2 buff/cache
MiB Swap:    355.0 total,    121.0 free,    234.0 used.  12275.0 avail Mem
```

How to read it:

- **Load average** (1, 5 and 15 minutes) is the average number of processes running or waiting to run. Compare it with the number of CPUs (`nproc`): a load of 8 on an 8-CPU machine means it is fully busy; far above that means work is queueing.
- **%Cpu(s)**: `us` user code, `sy` kernel code, `id` idle, `wa` waiting for disk or network. A high `wa` points at I/O, not CPU.
- **Memory**: do not worry that "free" is small. Linux uses spare RAM to cache files (`buff/cache`) and gives it back when programs need it. The number that matters is **avail Mem** (here about 12 GB).
- **Zombies** (`Z`) are finished processes waiting for their parent to collect their status. A few are harmless; thousands indicate a buggy parent.

Handy one-liners:

```bash
uptime                                    # load averages in one line
free -h                                   # memory in human-readable units
ps -eo pid,ppid,%cpu,%mem,comm --sort=-%mem | head    # biggest memory users
df -h                                     # disk space (see the next note on disks)
```

For deeper analysis, see *Performance Methodology: USE, Load, CPU and Memory Analysis*.

## Running things later: cron and systemd timers

**cron** runs commands on a schedule. Edit your own table with `crontab -e`; each line has five time fields and a command:

```text
# m   h   dom mon dow   command
30    2   *   *   *     /home/alice/backup.sh            # every day at 02:30
*/15  *   *   *   *     curl -fsS https://example.com/ping > /dev/null   # every 15 minutes
0     9   *   *   1-5   /home/alice/report.sh            # 09:00, Monday to Friday
```

The fields are minute (0-59), hour (0-23), day of month, month and day of week (0-7, 0 and 7 are Sunday); `*` means "every", `*/15` "every 15", `1-5` a range. Notes for reliable jobs:

- cron uses a **minimal environment** and a short `PATH`: use full paths to commands, and test with `env -i /bin/sh -c 'your command'`.
- Output is mailed (if mail is set up) or lost: redirect it to a log (`>> /tmp/job.log 2>&1`).
- Use `crontab -l` to list the table and `crontab -r` to remove *all* entries (careful: it does not ask).
- Jobs that overlap (the next run starts before the last finished) need a lock: `flock -n /tmp/job.lock command`.

Modern systems also offer **systemd timers**, which add logging in the journal, dependency handling and catch-up after downtime (`Persistent=true`); see *systemd Timers, cgroups v2 and Resource Control*. List them with `systemctl list-timers`. For a one-off job use `at` (`echo "command" | at 17:30`).

## Troubleshooting checklist

1. Machine feels slow: `uptime` (load), `top` (who is using CPU and memory, is `wa` high?), `free -h` (is available memory low or swap heavy?).
2. A program hangs: `ps -o pid,stat,cmd -p PID`. State `D` means stuck in I/O; `S` means waiting on something (a socket, another process). Try `kill PID`, wait a few seconds, then `kill -9 PID`.
3. A cron job "does nothing": check full paths, the environment, a log redirect, and `grep CRON /var/log/syslog` (or `journalctl -u cron`).
4. A background job died when you logged out: use `nohup`, `tmux` or a systemd service.

## Key takeaways

1. A process is a running program with a PID, a parent (PPID), an owner and an exit status; `ps` and `top` show them.
2. `&`, `jobs`, `fg`, `bg` and `Ctrl+Z` manage shell jobs; use `nohup`, `tmux` or systemd for work that must survive logout.
3. `kill` sends signals: try `SIGTERM` first, use `SIGKILL` only as a last resort; an exit status above 128 means "killed by signal status − 128".
4. Judge machine health with load average versus `nproc`, `wa` for I/O waits and *available* (not free) memory.
5. cron and systemd timers run jobs later: use full paths, log output and guard against overlapping runs.
