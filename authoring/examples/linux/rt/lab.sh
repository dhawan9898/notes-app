#!/bin/bash
# Wake-up latency of a 1 ms periodic thread, measured four ways. Numbers depend on the machine; compare the lines with each other.
echo "== default timer slack: $(cat /proc/self/timerslack_ns) ns =="
echo "idle machine:";               ./jitter
echo "timer slack set to 1 ns:";    ./jitter --slack 1
echo "SCHED_FIFO request (needs root or CAP_SYS_NICE, or an RLIMIT_RTPRIO allowance):"; ./jitter --fifo --mlock
echo; echo "== eight CPU hogs competing on all CPUs =="
pids=""; for i in $(seq 1 $(( $(nproc) )) ); do (while :; do :; done) & pids="$pids $!"; done
sleep 0.5
echo "default scheduling, any CPU:";                 ./jitter
echo "timer slack 1 ns, any CPU:";                   ./jitter --slack 1
echo "CPU 0 shared with a hog, pinned to CPU 0:";    taskset -c 0 ./jitter --cpu 0
kill $pids 2>/dev/null; wait 2>/dev/null
echo; echo "== real-time scheduling limits of this account =="
ulimit -r | sed 's/^/RLIMIT_RTPRIO: /'
