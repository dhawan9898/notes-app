---
title: Text Processing (grep, sed, awk) and Robust Shell Scripting
track: linux
module: Shell and Text Processing
moduleOrder: 2
order: 2
level: intermediate
summary: The Unix text toolbox applied to a real log, plus a hardened script template and the practices that keep production scripts safe, idempotent and debuggable.
sources:
  - label: "POSIX.1-2017 Utilities — grep, sed, awk, sort, cut, xargs, find"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/idx/utilities.html
  - label: "GNU grep, sed and gawk manuals"
    url: https://www.gnu.org/software/gawk/manual/gawk.html
  - label: "Aho, Kernighan, Weinberger — The AWK Programming Language (2nd ed.)"
  - label: "ShellCheck wiki"
    url: https://www.shellcheck.net/wiki/
  - label: "Google Shell Style Guide"
    url: https://google.github.io/styleguide/shellguide.html
---

Unix tools do one thing on **streams of lines** and compose through pipes. Fluency here is the fastest way to answer questions about logs, configuration and system state without writing a program. Examples use this web log:

```text
203.0.113.5 - - [08/Oct/2026:10:00:01 +0000] "GET /index.html HTTP/1.1" 200 5120 "-" "curl/8.0"
203.0.113.5 - - [08/Oct/2026:10:00:02 +0000] "GET /api/users HTTP/1.1" 500 120 "-" "curl/8.0"
198.51.100.7 - - [08/Oct/2026:10:00:03 +0000] "POST /api/login HTTP/1.1" 200 340 "-" "Mozilla/5.0"
198.51.100.7 - - [08/Oct/2026:10:00:04 +0000] "GET /api/users HTTP/1.1" 200 2048 "-" "Mozilla/5.0"
192.0.2.9 - - [08/Oct/2026:10:00:05 +0000] "GET /missing HTTP/1.1" 404 90 "-" "Googlebot"
203.0.113.5 - - [08/Oct/2026:10:00:06 +0000] "GET /api/users HTTP/1.1" 500 120 "-" "curl/8.0"
```

## grep: find lines

| Flag | Meaning |
|---|---|
| `-E` | extended regex (`+ ? | () {}`) (`egrep`) |
| `-F` | fixed strings (fast, no regex pitfalls) |
| `-P` | PCRE (lookaheads, `\d`): GNU only |
| `-i -v -w -x` | ignore case / invert / whole word / whole line |
| `-n -c -l -L -o` | line numbers / count / files with match / without / only the matching part |
| `-r -R --include='*.c'` | recursive search |
| `-A N -B N -C N` | context lines |
| `-q` | quiet, status only (for `if grep -q ...`) |
| `-z` | NUL-separated records (multi-line matches) |

Status: 0 match, 1 no match, 2 error. Alternatives: `rg` (ripgrep) for code trees.

```bash
$ grep -oE '"[A-Z]+ [^ ]+' access.log | head -3
"GET /index.html
"GET /api/users
"POST /api/login
```

## sed: stream editing

`sed [-n] [-E] 'ADDR cmd' file`; commands: `s/re/rep/flags` (flags `g`, `p`, `I`), `d` delete, `p` print, `i\`/`a\`/`c\` insert/append/change, `y/abc/xyz/`, `q` quit. Addresses: line numbers, `$` (last), `/re/`, ranges `2,5` or `/start/,/end/`. `-n` + `p` prints only selected lines. `-i[SUFFIX]` edits in place (GNU; BSD sed needs `-i ''`). `&` in the replacement is the whole match; `\1` captures (with `-E`, or `\(...\)` in basic regex).

```bash
$ sed -E 's/^([0-9.]+) .*\[([^]]+)\] "(GET|POST) ([^ ]+) HTTP\/[0-9.]+" ([0-9]{3}) .*/\1 \5 \4/' access.log | head -3
203.0.113.5 200 /index.html
203.0.113.5 500 /api/users
198.51.100.7 200 /api/login
$ sed -n '2,3p' access.log          # lines 2–3
$ sed -i.bak 's/^PermitRootLogin .*/PermitRootLogin no/' /etc/ssh/sshd_config
```

Regex tips: `.*` is **greedy**, so anchor on literal context (`HTTP/1.1" ` before the status) instead of a loose `([0-9]{3})`; use a different delimiter for paths (`s|/usr/local|/opt|g`). sed has a *hold space* for multi-line tricks, but if you need it, reach for awk or a real language.

## awk: columns and aggregation

An awk program is a list of `pattern { action }`. Records are lines, **fields** are `$1…$NF` (split by `FS`, default whitespace), `NR` is the record number, `NF` the field count, `$0` the whole line. `BEGIN{}` and `END{}` run before/after input. Associative arrays make grouping trivial; `printf` formats.

```bash
$ awk '{print $9}' access.log | sort | uniq -c | sort -rn       # status histogram
      3 200
      2 500
      1 404
$ awk '{b[$1]+=$10} END{for (i in b) printf "%s %d\n", i, b[i]}' access.log | sort -k2 -nr    # bytes per client
203.0.113.5 5360
198.51.100.7 2388
192.0.2.9 90
$ awk '$9 ~ /^5/ {print $7}' access.log | sort | uniq -c      # paths returning 5xx
      2 /api/users
$ awk -F'"' '{split($2,r," "); printf "%-5s %s\n", r[1], r[2]}' access.log | head -3
GET   /index.html
GET   /api/users
POST  /api/login
```

Useful: `-F:` field separator (`-F'[,;]'` regex), `-v var=val` pass variables, `NR==FNR` idiom to join two files, `length`, `substr`, `gsub`, `tolower`, `strftime` (gawk). Compare mawk (default on Debian, fast) with gawk (more features): use `gawk` if you need `--` option handling, `asort`, `strftime`.

## The supporting cast

| Tool | Use |
|---|---|
| `sort -k2,2 -n -r -u -t:` | sort by key; `-u` unique; `-V` version sort; `-S`/`-T` memory & temp dir for huge inputs; set `LC_ALL=C` for speed and bytewise order |
| `uniq -c` | count adjacent duplicates (sort first) |
| `cut -d: -f1,3` / `-c1-10` | columns by delimiter or char |
| `tr 'a-z' 'A-Z'`, `tr -d '\r'`, `tr -s ' '` | translate / delete / squeeze characters |
| `wc -l -w -c`, `head`, `tail -f -n`, `tee`, `paste`, `join`, `comm`, `diff`, `column -t` | counting, slicing, merging |
| `xargs -0 -n1 -P4` | build commands from stdin; `-P` parallel; pair with `find -print0` |
| `find . -type f -name '*.log' -mtime +7 -size +1M -exec rm {} +` | file selection |
| `jq '.items[] | select(.ok) | .name'` | JSON; `yq` for YAML; `csvkit`/`miller` for CSV |

```bash
$ printf 'a b\0c\0' | xargs -0 -n1 printf '[%s]\n'     # NUL-safe: filenames with spaces
[a b]
[c]
```

Pipelines to memorize: top talkers `awk '{print $1}' log | sort | uniq -c | sort -rn | head`; count per minute `cut -c14-18`; rare values `sort | uniq -c | sort -n | head`; set difference `comm -23 <(sort a) <(sort b)`.

Locale matters: `[a-z]` and sort order depend on `LANG`/`LC_ALL`. Use `LC_ALL=C` for deterministic, faster results in scripts.

## Writing robust scripts

### A template

```bash
#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'                                  # safer word splitting
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT  # always clean up
trap 'echo "error on line $LINENO" >&2' ERR

usage() { echo "usage: ${0##*/} [-n count] file" >&2; exit 2; }
count=3
while getopts ':n:h' opt; do
  case $opt in n) count=$OPTARG ;; h|*) usage ;; esac
done
shift $((OPTIND - 1))
[[ $# -eq 1 && -r $1 ]] || usage

awk '{print $1}' "$1" | sort | uniq -c | sort -rn | head -n "$count" > "$tmp/top"
mv "$tmp/top" "$tmp/final" && cat "$tmp/final"
```

Run on the sample log:

```text
$ ./robust.sh -n 2 access.log
      3 203.0.113.5
      2 198.51.100.7
$ ./robust.sh
error on line 13
usage: robust.sh [-n count] file        (exit status 2)
```

### Practices that prevent incidents

1. **Quote** every expansion; use arrays for argument lists; use `--` to end options before untrusted names (`rm -- "$f"`; `grep -e "$pat" -- "$file"`; note mawk's `awk` rejects `--` before the filename, an example of why you test).
2. **Never parse `ls`.** Iterate with globs, or with `find … -print0 | while IFS= read -r -d '' f; do …; done`.
3. **Use `[[ ]]`** in bash (no word splitting, pattern/regex matching); `[ ]` with quoted operands in POSIX sh.
4. **Fail fast but understand `set -e`:** it doesn't fire in conditions or after `||`; check critical commands explicitly (`cmd || die "…"`). Declare then assign: `local out; out=$(cmd)`.
5. **Temporary files:** `mktemp`, trap cleanup, never fixed names in `/tmp` (symlink attacks); write results to a temp file in the *same directory* then `mv` for **atomic** replacement.
6. **Locking:** `flock -n /var/lock/job.lock cmd` or `exec 9>/var/lock/job; flock -n 9 || exit 0` to avoid overlapping cron runs.
7. **Idempotence:** make re-runs safe (`mkdir -p`, `ln -sfn`, `grep -q … || append`), so retries and failures mid-way are harmless.
8. **Use absolute paths and explicit `PATH`** in cron/systemd; log with timestamps to stderr or `logger -t name`; use meaningful exit codes.
9. **Validate input** (`[[ $x =~ ^[0-9]+$ ]]`), never `eval` untrusted data, avoid building commands as strings; for SQL, JSON, YAML use proper tools (`jq`, parameterized clients).
10. **`printf` over `echo`** for arbitrary data (`echo -e`/`-n` portability); `read -r` always; `IFS= read -r line` to keep whitespace.
11. **Trap signals** to clean up; propagate exit statuses; use `wait` and `$!` for background jobs, `kill -- -PGID` for groups.
12. **Test and lint:** `shellcheck script.sh` (e.g. SC2086 unquoted variable, SC2046 unquoted command substitution, SC2164 `cd` without `|| exit`), `bash -n` syntax check, `set -x` or `PS4='+ ${BASH_SOURCE}:${LINENO}: '` tracing, `bats` for tests.
13. **Know when to stop:** past ~100 lines, complex data structures, or heavy parsing, switch to Python/Go; keep shell for orchestration.

### Concurrency with xargs / parallel

```bash
find . -name '*.png' -print0 | xargs -0 -n1 -P"$(nproc)" convert -resize 50% 
```

`-P` runs jobs in parallel; make each invocation independent (separate output files), or use GNU `parallel` for ordered output and retries.

## Key takeaways

- `grep` selects lines, `sed` transforms them, `awk` splits fields and aggregates; `sort | uniq -c | sort -rn` is the universal histogram.
- Prefer `-0`/`-print0` pipelines for filenames, `LC_ALL=C` for speed and determinism, and anchored regexes to avoid greedy surprises.
- A robust script: strict mode, traps, `mktemp`, quoted expansions, atomic writes, locking, idempotence — and `shellcheck`.
- Use the right tool for the size of the job: shell for glue, a real language for logic.
