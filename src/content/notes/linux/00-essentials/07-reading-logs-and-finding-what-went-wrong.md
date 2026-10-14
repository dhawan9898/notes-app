---
title: "Reading Logs and Finding What Went Wrong"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 7
level: beginner
summary: "Where Linux keeps logs, how to read them with journalctl, tail, grep and awk, a repeatable method for narrowing a failure to one cause using a captured sample log, and a first look at log rotation."
prerequisites:
  - Linux Command Line Basics
  - "Processes, Monitoring and Scheduled Jobs"
sources:
  - label: "man 1 journalctl, man 8 systemd-journald, man 5 journald.conf"
    url: https://www.freedesktop.org/software/systemd/man/latest/journalctl.html
  - label: "man 8 logrotate and man 5 logrotate.conf"
    url: https://man7.org/linux/man-pages/man8/logrotate.8.html
  - label: "RFC 5424 — The Syslog Protocol"
    url: https://www.rfc-editor.org/rfc/rfc5424
  - label: "Shotts — The Linux Command Line, Ch. 19-20 (Regular Expressions; Text Processing)"
    url: https://linuxcommand.org/tlcl.php
---

When something breaks on a Linux machine, the cause is almost always written down somewhere. The skill is knowing *where to look* and *how to read quickly*: a busy service writes thousands of lines a minute, and the three that matter are buried among them. This note covers the places logs live, the tools to search them, and a method that takes you from "it is broken" to "this is the line that says why".

## Where logs live

| Source | Where | Read with |
|---|---|---|
| Services managed by systemd, the kernel, and `logger` messages | the **journal** (binary, indexed) | `journalctl` |
| Kernel messages (hardware, drivers, out-of-memory kills) | the kernel ring buffer, also in the journal | `dmesg`, `journalctl -k` |
| Traditional text logs (`/var/log/syslog`, `auth.log`, `messages`, `nginx/`, `apache2/`, `mysql/`) | files under **`/var/log`** | `less`, `tail`, `grep` |
| Your own programs | wherever you send them: `stdout`/`stderr`, a file, or the journal | redirection or `journalctl` |

Which exist depends on the distribution: Debian and Ubuntu keep `/var/log/syslog` and `auth.log` (login and `sudo` events); Fedora and RHEL keep `/var/log/messages` and `/var/log/secure`; many minimal systems keep *only* the journal. Reading most system logs needs root or membership of the `adm` or `systemd-journal` group (hence `Permission denied` for a normal user).

## journalctl: the everyday commands

The journal stores each message with metadata (time, unit, priority, process ID), which lets you filter instead of grep. These are typical forms (they need access to the system journal, so run them as a privileged user):

```bash
journalctl -u nginx              # one service's messages
journalctl -u nginx -f           # follow live, like tail -f
journalctl -u nginx --since "10 min ago"          # a time window
journalctl --since "2026-10-14 09:00" --until "2026-10-14 09:30"
journalctl -p err -b             # priority "error" or worse since this boot
journalctl -b -1                 # the PREVIOUS boot (after a crash or reboot)
journalctl -k                    # kernel messages only
journalctl _PID=1234             # one process
journalctl -o cat -u myapp       # message text only, no timestamps
journalctl --disk-usage          # how much space the journal uses
```

Priorities run from `emerg` (0) through `alert`, `crit`, `err`, `warning`, `notice` and `info` to `debug` (7); `-p err` shows `err` and everything more severe. `journalctl -b -1` is the first command to run after an unexplained reboot: it shows the end of the previous boot's log, which usually contains the clue. Remember that journals are only kept across reboots if `/var/log/journal` exists or `Storage=persistent` is set in `journald.conf`.

## Working with plain log files

Everything you learned about pipelines applies. Here is a small web-server log (a sample created for this exercise):

```text
10:00:01 INFO  GET /index 200 12ms
10:00:02 INFO  GET /login 200 31ms
10:00:03 WARN  GET /report 200 1900ms slow request
10:00:04 ERROR GET /api/orders 500 db connection refused
10:00:04 ERROR GET /api/orders 500 db connection refused
10:00:05 INFO  GET /index 200 9ms
10:00:07 ERROR GET /api/users 500 db connection refused
10:00:09 INFO  GET /index 200 11ms
10:00:12 WARN  GET /report 200 2100ms slow request
10:00:15 INFO  GET /logout 302 4ms
```

**Look at the end** (the newest events), and **follow** new lines as they arrive:

```text
$ tail -n 3 web.log
10:00:09 INFO  GET /index 200 11ms
10:00:12 WARN  GET /report 200 2100ms slow request
10:00:15 INFO  GET /logout 302 4ms
$ tail -f web.log        # waits; press Ctrl+C to stop. After the application appends a line:
10:00:20 ERROR GET /api/orders 500 db connection refused
```

**Find the problems and their context.** `-n` adds line numbers, and `-B`/`-A` show lines Before/After a match, because the cause often sits just above the error:

```text
$ grep -n ERROR web.log | head -2
4:10:00:04 ERROR GET /api/orders 500 db connection refused
5:10:00:04 ERROR GET /api/orders 500 db connection refused
$ grep -B1 -A1 "10:00:07" web.log
10:00:05 INFO  GET /index 200 9ms
10:00:07 ERROR GET /api/users 500 db connection refused
10:00:09 INFO  GET /index 200 11ms
```

**Count and rank.** Ten thousand lines are unreadable; a ranking is not:

```text
$ grep -o "^[0-9:]* [A-Z]*" web.log | awk '{print $2}' | sort | uniq -c | sort -rn
      5 INFO
      3 ERROR
      2 WARN
$ grep ERROR web.log | cut -d' ' -f6- | sort | uniq -c | sort -rn
      3 db connection refused
```

All three errors share one message, so there is likely **one cause**, not three problems: the database connection. That is the habit that saves hours: group by message and fix the biggest group first.

**Slice by time and by value.** Compare the zero-padded timestamps as strings (it works because `HH:MM:SS` sorts alphabetically):

```text
$ awk '$1>="10:00:04" && $1<="10:00:07"' web.log | wc -l
4
$ grep -E '[0-9]{4}ms' web.log | awk '{print $4, $6}'
/report 1900ms
/report 2100ms
```

The second command finds requests that took four or more digits of milliseconds, here two slow calls to `/report`. Use `awk '$2=="ERROR" {t=$1} END {print t}'` to print the time of the *last* error (10:00:07) and see whether the problem is still happening.

## A method for finding the cause

1. **State the symptom and the time.** "The site returned errors around 10:00." Check the clock and time zone on the machine and in the log.
2. **Look at the log around that time**, starting with errors and warnings: `journalctl -u service --since ... -p warning`, or `grep -n ERROR` and context.
3. **Find the *first* error, not the loudest.** Later errors are often consequences. The first `ERROR` at 10:00:04 is the earliest sign of trouble.
4. **Group repeated messages** and find the dominant one.
5. **Follow the chain across components.** "db connection refused" points at the database: look at *its* log and `systemctl status`, check the port with `ss -tln`, then network and disk space (`df -h`), memory (`journalctl -k | grep -i "out of memory"`) and recent changes.
6. **Confirm the fix by watching** (`tail -f`, `journalctl -f`) that the errors stop, and note what you changed.

Do not skip step 3: restarting a service because of the *last* error message often hides the first, real one.

## Log rotation: logs must not fill the disk

A log that grows forever eventually fills the filesystem. **logrotate** (run daily by cron or a systemd timer) renames old logs, compresses them and deletes the oldest. A typical rule:

```text
/var/log/myapp/*.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
    copytruncate
}
```

This keeps seven compressed daily files, ignores a missing or empty log and uses `copytruncate` (copy the log, then empty the original) for programs that cannot reopen their log file on a signal. Check a rule without changing anything: `logrotate --debug /etc/logrotate.d/myapp`. The journal has its own limits: see `SystemMaxUse=` in `journald.conf` and `journalctl --vacuum-size=500M` to trim it. When a disk fills up unexpectedly, big logs are the first suspect (`du -h --max-depth=1 /var/log | sort -h`, see *Disks, Mounts, Archives and Backups*).

## Pitfalls

- **Reading only the last lines**; the first error is usually earlier.
- **Mixed time zones**: the journal shows local time by default, application logs may use UTC. `journalctl --utc` helps.
- **Logs that are not there**: a service writing to `stdout` under systemd goes to the journal, not to `/var/log`.
- **Truncated or rotated logs**: the answer may be in `web.log.1` or `web.log.2.gz` (`zgrep` searches compressed files).
- **Secrets in logs**: never log passwords, tokens or full card numbers, and take care when pasting logs into tickets.
- **Debug logging left on in production**: it fills disks and slows the service.

## Key takeaways

1. Services and the kernel log to the journal (`journalctl`); many programs also write text files under `/var/log`.
2. Use `-u`, `--since`, `-p` and `-b -1` in `journalctl`; use `tail -f`, `grep -B/-A`, `awk` and `sort | uniq -c` for files.
3. Find the *first* error, group identical messages, and follow the chain from the symptom to the failing component.
4. Rotate and limit logs (logrotate, journald limits) so they cannot fill the disk.
