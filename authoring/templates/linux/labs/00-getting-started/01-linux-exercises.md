---
title: Linux Exercises
track: linux
module: Getting Started
moduleOrder: 0
order: 1
level: beginner
minutes: 25
summary: "Ten command-line exercises on log analysis with grep, cut, sort, uniq, awk, sed, find, redirection and scripting, with a verified solution script, plus system-administration tasks."
prerequisites:
  - Linux Command Line Basics
  - Installing Software and Managing Services
sources:
  - label: Shotts — The Linux Command Line, Parts 2-4
    url: https://linuxcommand.org/tlcl.php
  - label: GNU Coreutils manual
    url: https://www.gnu.org/software/coreutils/manual/coreutils.html
---

Create a scratch directory (`mkdir ~/practice && cd ~/practice`) and this small web-server log, `access.log`. Fields are: client address, method, path, status code.

```text
10.0.0.5 GET /index.html 200
10.0.0.7 GET /about.html 200
10.0.0.5 POST /login 302
10.0.0.9 GET /missing 404
10.0.0.5 GET /index.html 200
10.0.0.7 GET /missing 404
10.0.0.9 GET /index.html 500
```

Do each task with a short command or pipeline, then compare with the answers.

## Exercises

1. How many lines does the file have?
2. How many requests returned status 404?
3. Which client address made the most requests, and how many?
4. Print the client, path and status of every request whose status is 400 or above.
5. Make a directory `backup` and copy the log into it as `access.log.bak`.
6. Find every file ending in `.log` or `.bak` below the current directory.
7. How many requests did **not** return 200?
8. Show the first two lines with `10.0.0.` replaced by `net-`.
9. Create a script `hi.sh` that prints `hello from a script`, make it executable and run it. Show its permissions.
10. For each of `access.log` and `hi.sh`, print "NAME has N lines" using a `for` loop.

**System tasks (on a VM, read-only unless you choose to change things).**

11. Which package owns `/usr/bin/grep`? (Debian: `dpkg -S`; Fedora: `rpm -qf`.)
12. Is the `cron` (or `crond`) service active? Is it enabled? Where is its unit file?
13. Find the last 20 lines of that service's journal.
14. Which five processes use the most memory? (`ps aux --sort=-%mem | head -6`.)
15. How much free space does each mounted filesystem have? (`df -h`.) Which directory under `/var` is largest? (`du -sh /var/* 2>/dev/null | sort -h | tail`.)

## Answers

<details>
<summary>Show answers (exercises 1-10 are verified by running the script below)</summary>

1. `wc -l < access.log` → 7.
2. `grep -c " 404$" access.log` → 2 (the `$` anchors the match to the end of the line, so a path containing "404" would not match).
3. `cut -d" " -f1 access.log | sort | uniq -c | sort -rn | head -1`: `cut` extracts field 1, `sort` groups equal lines together (needed because `uniq` only collapses *adjacent* duplicates), `uniq -c` counts, `sort -rn` puts the biggest first → `10.0.0.5` with 3.
4. `awk '$4 >= 400 {print $1, $3, $4}' access.log` (quote with single quotes in a real shell so `$4` is not expanded).
5. `mkdir backup && cp access.log backup/access.log.bak`.
6. `find . -name "*.log" -o -name "*.bak"`.
7. `grep -v " 200$" access.log | wc -l` → 4 (`-v` inverts the match).
8. `sed 's/10\.0\.0\./net-/' access.log | head -2` (escape the dots, which otherwise match any character).
9. `printf '#!/bin/bash\necho hello from a script\n' > hi.sh; chmod +x hi.sh; ./hi.sh; stat -c %A hi.sh`.
10. `for f in access.log hi.sh; do echo "$f has $(wc -l < "$f") lines"; done`.
11. `dpkg -S /usr/bin/grep` → `grep`. 12. `systemctl is-active cron`, `systemctl is-enabled cron`, `systemctl show cron -p FragmentPath`. 13. `journalctl -u cron -n 20`. 14. `ps aux --sort=-%mem | head -6`. 15. `df -h` and `du -sh /var/* | sort -h | tail`.

The script that solved exercises 1 to 10, with its real output:

{{include linux/beginner/answers.sh}}

{{run linux/beginner :: bash answers.sh}}

</details>

## Key takeaways

1. Most log questions are a pipeline: select (`grep`, `awk`), extract (`cut`), group (`sort | uniq -c`), rank (`sort -rn`), limit (`head`).
2. Anchor patterns (`$`, `^`) and quote them to avoid matching the wrong thing or letting the shell expand them.
3. `uniq` needs sorted input.
4. Always check results on a small sample before running on production data.

**Next:** *Users, Permissions, ACLs and Capabilities*.
