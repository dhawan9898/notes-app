---
title: "Your First Shell Scripts"
track: linux
module: Linux Essentials
moduleOrder: 0.5
order: 5
level: beginner
summary: "Turning commands you already type into reusable bash scripts: the shebang and executable bit, variables, arguments, quoting, if and for, functions, exit statuses, set -euo pipefail, and how to debug with bash -x, using two complete scripts with captured output."
prerequisites:
  - Linux Command Line Basics
  - Users, Groups and Permissions in Practice
sources:
  - label: "Shotts — The Linux Command Line, Part 4 (Writing Shell Scripts)"
    url: https://linuxcommand.org/tlcl.php
  - label: "GNU Bash Reference Manual"
    url: https://www.gnu.org/software/bash/manual/bash.html
  - label: "POSIX.1-2017 Shell Command Language"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
  - label: "ShellCheck — static analysis for shell scripts"
    url: https://www.shellcheck.net/
---

A **shell script** is a text file of commands that the shell runs for you. Anything you type more than twice is worth a script, and scripts are how backups, deployments, build steps and logins are automated. This note takes you from the first script to a defensive one, using only constructs you will use every week. (The deeper rules of parsing and expansion are in *The Shell — Parsing, Expansion, Redirection, Pipes and Job Control*, and a longer treatment of robust scripting is in *Text Processing (grep, sed, awk) and Robust Shell Scripting*.)

## The anatomy of a script

1. The first line, the **shebang** `#!/bin/bash`, names the interpreter.
2. Make the file executable: `chmod +x script.sh` (see *Users, Groups and Permissions in Practice*).
3. Run it with a path: `./script.sh`. (Or run `bash script.sh` without the execute bit.)
4. Lines starting with `#` are comments.

Here is a complete, small script that greets everyone named on the command line:

```bash
#!/bin/bash
# greet.sh: say hello to each name given on the command line
name_count=0
for name in "$@"; do                       # "$@" = every argument, each kept intact
    echo "Hello, $name!"
    name_count=$((name_count + 1))         # arithmetic
done
if [ "$name_count" -eq 0 ]; then           # spaces inside [ ] are required
    echo "usage: $0 NAME..." >&2           # error messages go to stderr
    exit 2
fi
echo "greeted $name_count people"
```

```text
$ ./greet.sh Ada "Grace Hopper"
Hello, Ada!
Hello, Grace Hopper!
greeted 2 people
exit status: 0
$ ./greet.sh
usage: ./greet.sh NAME...
exit status: 2
```

What each piece does:

- **Variables**: `name_count=0` (no spaces around `=`), read with `$name_count` or `${name_count}`. Everything is a string; `$(( ... ))` does integer arithmetic.
- **Arguments**: `$1`, `$2`, ... are the words after the script name, `$#` is how many there are, `$0` is the script's own name, and `"$@"` expands to *all* arguments with each one kept as a single word. Here `"Grace Hopper"` stays one name.
- **`for ... in ...; do ... done`** loops over words. `while`, `case` and `until` exist too.
- **`if [ condition ]; then ... fi`**: `[` is actually a command, so the spaces matter. Numeric tests use `-eq -ne -lt -gt -le -ge`; string tests use `=` and `!=`; file tests include `-f` (is a file), `-d` (is a directory), `-e` (exists), `-r` (readable). Prefer `[[ ... ]]` in bash scripts: it is safer with empty values and supports `==` patterns and `&&`.
- **stderr and the exit status**: usage errors go to **standard error** (`>&2`) so they do not pollute output that other programs read, and `exit 2` reports failure to whoever ran the script. Success is `exit 0` (the default when the last command succeeds).

## Quoting: the number one source of bugs

The shell splits unquoted variables on whitespace and expands wildcards in them. A file called `my file.txt` shows the problem:

```text
$ f="my file.txt"; touch "$f"
$ ls $f
ls: cannot access 'my': No such file or directory
ls: cannot access 'file.txt': No such file or directory
$ ls "$f"
my file.txt
```

**Always quote variable expansions: `"$var"`, `"$@"`, `"$(command)"`.** The rare exception is when you *want* word splitting. Use single quotes `'...'` for text that must not be expanded at all.

## A real script: defensive by default

```bash
#!/bin/bash
set -euo pipefail                          # stop on errors, unset variables and failed pipes
src=${1:?usage: backup.sh SOURCE_DIR}      # abort with a message if no argument
dest="backup-$(date +%Y%m%d).tar.gz"       # command substitution

log() { echo "[$(date +%H:%M:%S)] $*"; }   # a function

[ -d "$src" ] || { log "no such directory: $src" >&2; exit 1; }
log "archiving $src"
tar -czf "$dest" "$src"
log "wrote $dest ($(du -h "$dest" | cut -f1))"
```

```text
$ ./backup.sh docs
[12:00:00] archiving docs
[12:00:00] wrote backup-DATE.tar.gz (4.0K)
$ ./backup.sh nothing
[10:48:54] no such directory: nothing
exit status: 1
$ ./backup.sh
./backup.sh: line 3: 1: usage: backup.sh SOURCE_DIR
exit status: 1
```

(The time and date in the first run are masked here.) The new ideas:

- **`set -euo pipefail`** turns bash from "carry on whatever happens" into "stop at the first problem": `-e` exits when a command fails, `-u` treats use of an unset variable as an error, and `-o pipefail` makes a pipeline fail if *any* stage fails (otherwise only the last command counts). It is not magic (there are cases where `-e` does not trigger, such as inside `if` conditions), but it prevents the worst silent failures, like `rm -rf "$dir/"*` running with an empty `$dir`.
- **`${1:?message}`** takes the first argument or aborts with the message when it is missing (the third run).
- **`$(command)`** captures a command's output (`$(date +%Y%m%d)` gives `20261014`). Avoid the old backtick form.
- **Functions**: `log() { ...; }` groups commands; call it like a command. Inside, `$*` is the function's arguments.
- **`cmd || { ...; exit 1; }`** runs the block only if `cmd` fails; `&&` runs the next command only if the previous one succeeds. This is how scripts check results.

## Useful patterns to copy

```bash
# repeat for every file matching a pattern (the quotes protect names with spaces)
for f in *.log; do gzip -k "$f"; done

# read a file line by line
while IFS= read -r line; do echo "got: $line"; done < input.txt

# decide from a command's output or status
if grep -q "ERROR" app.log; then echo "errors found"; fi

# clean up temporary files even if the script is interrupted
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# default value for an optional setting
out=${OUTPUT_DIR:-/tmp}
```

`mktemp` creates a unique temporary file or directory safely, and `trap ... EXIT` runs your cleanup however the script ends. Using a predictable name like `/tmp/data` is a classic security bug.

## Debugging

- `bash -n script.sh` checks syntax without running anything.
- `bash -x script.sh` prints each command after expansion, prefixed with `+`:

```text
$ bash -x ./greet.sh Ada
+ name_count=0
+ for name in "$@"
+ echo 'Hello, Ada!'
Hello, Ada!
+ name_count=1
+ '[' 1 -eq 0 ']'
```

- Use `set -x` / `set +x` inside a script to trace only part of it.
- Run **ShellCheck** (`shellcheck script.sh`) if you can: it flags missing quotes, unsafe patterns and portability problems and teaches as it goes.

## When not to use shell

A script is the right tool for gluing commands together. Switch to Python or another language when you need real data structures, complex string handling, JSON parsing, or more than a few hundred lines; bash is a poor place to implement logic.

## Pitfalls

- **Unquoted variables** (`rm $file`) with spaces or wildcards in the value.
- **Spaces around `=`** in assignments (`x = 1` runs a command called `x`) and missing spaces inside `[ ]`.
- **Assuming the script runs in your interactive environment**: `PATH`, aliases and the working directory may differ under cron or systemd; use full paths and `cd` explicitly.
- **Parsing `ls` output**; use globs (`for f in *.txt`) or `find -print0` with `xargs -0`.
- **Ignoring exit statuses**: add `set -euo pipefail` and check critical commands.
- **Putting secrets on command lines** or in scripts committed to version control.

## Key takeaways

1. A script is a text file with a shebang and the execute bit; run it with `./script.sh`.
2. Quote every expansion (`"$var"`, `"$@"`), keep errors on stderr, and return meaningful exit statuses.
3. `if`, `for`, `while`, functions and `$(...)` cover most needs; `&&` and `||` chain commands on success or failure.
4. Start scripts with `set -euo pipefail`, create temporary files with `mktemp` and clean up with `trap`.
5. Debug with `bash -n`, `bash -x` and ShellCheck, and switch languages when logic gets complicated.
