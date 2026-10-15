---
title: Units, Limits and Timers
track: linux
module: systemd and Services
moduleOrder: 7
order: 1
level: advanced
minutes: 35
summary: "Five exercises on the systemd module that run as an ordinary user with systemd --user: validating unit files, running a transient service with a memory limit, reading a failed unit's result, calculating timer schedules, and writing a hardened service unit, with captured output."
prerequisites:
  - "systemd — Units, Dependencies, Targets, Hardening and the Journal"
  - "systemd Timers, cgroups v2 and Resource Control"
sources:
  - label: "systemd.service(5), systemd.exec(5), systemd.resource-control(5)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html
  - label: "systemd-analyze(1) and systemd-run(1)"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd-analyze.html
  - label: "systemd.time(7) — time and calendar expressions"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.time.html
---

You do not need root to practise systemd: every user can run a private manager with `systemctl --user` and start throwaway units with `systemd-run --user`. (This needs a normal login session; over plain SSH you may need `export XDG_RUNTIME_DIR=/run/user/$(id -u)`.) Predict each result, then run it.

## Exercises

**1. Catch mistakes before deploying.** A unit file contains the misspelt key `Restrat=on-failure` and an `ExecStart=` that points to a binary that does not exist. Which command finds both without starting anything?

**2. A transient service with a limit.** Start `sleep 60` as a user service named `pr-demo` with `MemoryMax=50M`. Show its state, its configured limit and its main PID, then stop it. Which kernel feature enforces `MemoryMax`?

**3. Why did it fail?** Run `sh -c 'echo working; exit 3'` as a transient service. Which properties tell you the unit failed and with what code? What do the restart policies `Restart=on-failure`, `always` and `no` each do with this unit?

**4. Read a schedule.** What does the calendar expression `Mon..Fri 09:00` normalise to, and when does it next elapse? How many microseconds is `1h 30min`? Which command answers these questions?

**5. A hardened unit.** Write a service unit for a report generator that runs as an unprivileged throwaway user, can never gain privileges, sees a read-only file system apart from its own private `/tmp`, cannot see `/home` and may use at most 200 MB of memory. Check it with the verifier.

## Answers

Try before opening.

<details>
<summary>Show answers and captured output</summary>

**1.** `systemd-analyze verify unit-file` parses the unit and checks it:

```text
$ systemd-analyze verify /tmp/bad.service
/tmp/bad.service:3: Unknown key 'Restrat' in section [Service], ignoring.
bad.service: Command /usr/bin/nonexistent-binary is not executable: No such file or directory
```

It reports the unknown key (with the line number; systemd would otherwise ignore the misspelling silently, leaving your restart policy off) and the missing executable. Run it on every unit before `daemon-reload`. A syntactically valid, complete unit produces no output and exit status 0 (see exercise 5).

**2.**

```text
$ systemd-run --user --unit=pr-demo --property=MemoryMax=50M /usr/bin/sleep 60
Running as unit: pr-demo.service; invocation ID: 9066ac21d80549d29c353e8ffdf91f5f
$ systemctl --user show pr-demo -p ActiveState -p SubState -p MemoryMax -p Restart
Restart=no
MemoryMax=52428800
ActiveState=active
SubState=running
$ systemctl --user status pr-demo --no-pager
● pr-demo.service - [systemd-run] /usr/bin/sleep 60
     Loaded: loaded (/run/user/.../systemd/transient/pr-demo.service; transient)
  Transient: yes
     Active: active (running) since Wed 2026-10-14 11:47:21 IST; 557ms ago
   Main PID: 3970917 (sleep)
      Tasks: 1 (limit: 23932)
     Memory: 204K (max: 50M, available: 49.8M, peak: 1.7M)
$ systemctl --user stop pr-demo
```

(The path and one line of the status are shortened.) `systemd-run` creates a **transient unit**: a real service that exists only until it stops or you reboot, a good way to experiment. `50M` is stored as 52,428,800 bytes. The limit is enforced by the kernel's **control groups (cgroups v2)**: systemd places the service in its own cgroup and sets `memory.max` there; the `Memory:` line shows the current use, the limit and the peak. If the service exceeded the limit, the kernel's OOM killer would terminate it within that cgroup, without affecting the rest of the system. The same properties can be written in a unit file as `MemoryMax=50M`.

**3.**

```text
$ systemd-run --user --unit=pr-fail sh -c 'echo working; exit 3'
Running as unit: pr-fail.service; invocation ID: 47b36e0baf2c470aac5c63694227334a
$ systemctl --user show pr-fail -p Result -p ExecMainStatus -p ActiveState
Result=exit-code
ExecMainStatus=3
ActiveState=failed
```

`ActiveState=failed` says the unit failed, `Result=exit-code` says why (a non-zero exit; other results are `signal`, `timeout`, `core-dump` or `oom-kill`), and `ExecMainStatus=3` is the exit code. Clear a failed unit with `systemctl reset-failed`. The restart policies: `no` (the default) leaves it failed; `on-failure` restarts after a non-zero exit, a signal death or a timeout, but not after a clean exit 0; `always` restarts regardless of how it ended, even a clean exit. Add `RestartSec=` to pause between attempts, and mind `StartLimitBurst`/`StartLimitIntervalSec`, which stop a crash loop after too many starts. For the logs of a service, `journalctl -u name` (a user needs to be in the `systemd-journal` or `adm` group to read the system journal).

**4.**

```text
$ systemd-analyze calendar "Mon..Fri 09:00"
  Original form: Mon..Fri 09:00
Normalized form: Mon..Fri *-*-* 09:00:00
    Next elapse: Thu 2026-10-15 09:00:00 IST
       (in UTC): Thu 2026-10-15 03:30:00 UTC
       From now: 21h left
$ systemd-analyze timespan "1h 30min"
Original: 1h 30min
      μs: 5400000000
   Human: 1h 30min
```

The normalised form spells out the full pattern `weekday year-month-day hour:minute:second`, with `*` for "every": every weekday, any date, at 09:00:00. `systemd-analyze calendar` shows when it will **actually next fire**, in local time and UTC, which catches time zone mistakes before they reach production. `1h 30min` is 5,400,000,000 microseconds (`5400 s × 10⁶`). Use these checks when writing `OnCalendar=` and `OnUnitActiveSec=` in a timer (see *systemd Timers, cgroups v2 and Resource Control*); add `Persistent=true` so a timer that was missed while the machine was off runs once at the next boot.

**5.**

```ini
[Unit]
Description=Example report generator
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep 30
Restart=on-failure
RestartSec=5
DynamicUser=yes
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
MemoryMax=200M

[Install]
WantedBy=multi-user.target
```

```text
$ systemd-analyze verify /tmp/hardened.service
$ echo $?
0
```

(`ExecStart=/usr/bin/sleep 30` stands in for your real program.) The hardening lines each remove an ability: `DynamicUser=yes` runs the service as a temporary, unprivileged user that systemd allocates for it (and implies several of the protections below), which is preferred over `User=nobody`, a shared account that the verifier warns about ("Special user nobody configured, this is not safe!"); `NoNewPrivileges=yes` stops the process and its children gaining privilege through `setuid` programs; `ProtectSystem=strict` mounts the whole file system read-only for the service (grant exceptions with `ReadWritePaths=`); `ProtectHome=yes` hides `/home`, `/root` and `/run/user`; `PrivateTmp=yes` gives it a private `/tmp` that other programs cannot read or tamper with; `MemoryMax=200M` bounds its memory. `After=`/`Wants=network-online.target` orders the start after the network is configured. Run `systemd-analyze security servicename` on an installed unit to get an **exposure score** with a line for each missing restriction, and tighten step by step while testing that the service still works.

</details>

## Key takeaways

1. `systemd-analyze verify` catches misspelt keys and missing executables before deployment; run it on every unit.
2. `systemd-run --user` creates transient units (and timers) without root; limits such as `MemoryMax` are enforced through cgroups v2.
3. `Result=`, `ExecMainStatus=` and `ActiveState=` explain a failure; choose `Restart=` deliberately and cap crash loops.
4. `systemd-analyze calendar` and `timespan` show real next-run times and durations; use `Persistent=true` for timers that must not be missed.
5. Hardening directives (`NoNewPrivileges`, `ProtectSystem`, `ProtectHome`, `PrivateTmp`, resource limits) reduce what a compromised service can do.
