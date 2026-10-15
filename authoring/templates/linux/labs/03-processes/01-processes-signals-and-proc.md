---
title: Processes, Signals and /proc
track: linux
module: Processes
moduleOrder: 3
order: 1
level: advanced
minutes: 40
env: machine
summary: "Six exercises on the processes module: reading parent-child relationships in a pipeline, producing and reaping a zombie, handling SIGTERM gracefully versus the uncatchable SIGKILL, decoding exit statuses above 128, adjusting priority, and inspecting a live process through /proc, all with captured output."
prerequisites:
  - "Process Lifecycle — fork, exec, wait"
  - "The Linux Scheduler: Classes, EEVDF/CFS, Priorities, Affinity and CPU Limits"
  - "Signals in Depth"
sources:
  - label: "man 7 signal, man 2 wait, man 5 proc, man 1 nice, man 1 taskset"
    url: https://man7.org/linux/man-pages/man7/signal.7.html
  - label: "POSIX.1-2017 — wait(), waitpid(), kill(), sigaction()"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/functions/waitpid.html
  - label: "Kerrisk — The Linux Programming Interface, Ch. 24-27 and 20-22"
---

Reading about processes is not the same as watching them. These exercises each take a minute to run; the point is to predict the output first. The output below comes from real runs on a Linux machine (process IDs differ every time).

## Exercises

**1. Who is whose parent?** Start `sleep 5 | sleep 6 &` in a script and list the shell's children with `ps -o pid,ppid,stat,comm --ppid $$`. Which process is the parent of both `sleep` commands, and why does `ps` itself appear in its own list?

**2. Make and reap a zombie.** Write a short program in which a child exits immediately while the parent has not yet called `wait`. Show the child in state `Z` with `ps`, explain why it still exists, and show that `waitpid` removes it. Can you kill a zombie with `kill -9`?

**3. Graceful shutdown.** Write a script that traps `SIGTERM`, prints a cleanup message and exits with status 0. Send it `SIGTERM`, then run it again and send `SIGKILL`. What status does the shell report in each case, and why can `SIGKILL` not be handled?

**4. Exit statuses.** What does the shell report after a process is killed by signal 9? After a segmentation fault? What is the rule?

**5. Priorities.** Start a command with `nice -n 10` and show its nice value (`NI`), then show the nice value of your own shell. Does a higher nice number mean higher or lower priority? Which commands show and change CPU affinity?

**6. Inspect a process through /proc.** For your shell, find its name, state, parent PID and thread count from `/proc/PID/status`, its executable from `/proc/PID/exe`, its command line from `/proc/PID/cmdline` and its open file descriptors from `/proc/PID/fd`.

## Answers

Try before opening.

<details>
<summary>Show answers and captured output</summary>

**1.**

```text
shell PID = 3970719
    PID    PPID STAT COMMAND
3970720 3970719 S    sleep
3970721 3970719 S    sleep
3970723 3970719 R    ps
```

Both `sleep` processes have the **shell** (3970719) as their parent: a pipeline is created by the shell forking once per stage, and the stages are siblings connected by a pipe, not parent and child. State `S` means sleeping (waiting for the timer). `ps` appears because it is also a child of the shell, and it is `R` (running) at the instant it takes its snapshot: observing the system changes it. `--ppid` filters by parent PID.

**2.**

{{include vm/zombie.c}}

{{vm :: ./zombie}}

(The program prints the state it reads from `/proc/PID/status`, and it runs in the Linux terminal of this app as `./zombie`; the process IDs differ on every run.) The child has finished, but the kernel keeps its **exit status and a small process-table entry** until the parent collects it with `wait`/`waitpid`, because the parent has a right to know how the child ended. That leftover entry is the zombie (`Z`, shown as `<defunct>` by some tools). It uses no CPU and almost no memory, but it holds a PID, so thousands of zombies can exhaust the PID table. **`kill -9` cannot remove it**: the process is already dead. The fix is to make the parent call `wait` (or ignore `SIGCHLD`), or to terminate the parent: orphaned zombies are re-parented to PID 1 (`init` or `systemd`), which reaps them.

**3.**

```bash
#!/bin/bash
cleanup() { echo "caught SIGTERM: removing temp files, exiting cleanly"; exit 0; }
trap cleanup TERM
echo "worker running"
while true; do sleep 0.1; done
```

```text
worker running
caught SIGTERM: removing temp files, exiting cleanly
status after TERM: 0
worker running
status after KILL: 137 (128 + 9)
```

`trap cleanup TERM` registers a handler, so `SIGTERM` (the polite request sent by `kill PID` and by `systemctl stop`) runs the cleanup and exits with 0. `SIGKILL` and `SIGSTOP` are handled **by the kernel** and are delivered without involving the process at all, so no handler, ignore or block is possible; the process just ceases to exist, with no cleanup, no flushed buffers and no removed lock files. That is why the rule is *try SIGTERM first, SIGKILL last*. (Note: a bash `trap` runs only between commands, so a handler waits for the current `sleep 0.1` to finish.)

**4.**

```text
status after KILL: 137   (128 + 9)
segfault status: 139     (128 + 11)
```

When a process dies from a signal, the shell reports **128 + the signal number**: `SIGKILL` is 9, so 137; `SIGSEGV` (segmentation fault) is 11, so 139; `SIGTERM` is 15, so 143; `SIGINT` (Ctrl+C) is 2, so 130. The shell also prints `Segmentation fault (core dumped)` for the crash. A normal exit with a code stays in 0..255 and is chosen by the program; values above 128 are therefore a strong hint that a signal ended the process.

**5.**

```text
$ nice -n 10 sh -c 'ps -o ni=,comm= -p $$'
 10 sh
$ ps -o ni=,comm= -p $$
  0 bash
```

`nice` starts a command with an adjusted niceness: the scale runs from -20 to 19, and a **higher number is "nicer" to others**, i.e. *lower* priority. Ordinary users can only raise niceness (be nicer); lowering it needs root. `renice -n 5 -p PID` changes a running process. Niceness affects how the scheduler shares CPU time between competing processes (it does not give or limit absolute CPU). CPU affinity is shown and set with `taskset -cp PID` (for example `0-7` means all eight CPUs) and `taskset -c 0,1 command`, and cgroups limit CPU more strictly (see *systemd Timers, cgroups v2 and Resource Control*).

**6.**

```bash
echo "my pid: $$"
grep -E '^(Name|State|PPid|Threads)' /proc/$$/status
readlink /proc/$$/exe
tr '\0' ' ' < /proc/$$/cmdline; echo
ls /proc/$$/fd | head -3
```

```text
my pid: 3970702
Name:	bash
State:	S (sleeping)
PPid:	3970687
Threads:	1
/usr/bin/bash
bash t6.sh 
0
1
2
```

`/proc/PID` is a live window into the kernel's view of a process: `status` has the name, state, parent and thread count (and memory fields such as `VmRSS`); `exe` is a symbolic link to the running executable (even if the file was since deleted); `cmdline` holds the arguments **separated by NUL bytes**, hence the `tr`; and `fd/` has one entry per open file descriptor: 0, 1 and 2 are standard input, output and error, and `ls -l` shows what each points to (a terminal, a pipe or a file). Everything in `/proc` is generated on demand and costs nothing to read. This is where `ps`, `top` and `lsof` get their data.

</details>

## Key takeaways

1. The shell is the parent of every stage of a pipeline; stages are siblings. `ps --ppid` and `pstree` show the tree.
2. A zombie is a finished process whose parent has not called `wait`; it cannot be killed, only reaped by its parent or by PID 1 after re-parenting.
3. `SIGTERM` can be trapped for graceful shutdown; `SIGKILL` and `SIGSTOP` cannot. A status above 128 means "killed by signal status − 128".
4. A higher nice value means lower priority; use `taskset` for CPU affinity and cgroups for hard limits.
5. `/proc/PID` exposes status, executable, arguments and open files of any running process.
