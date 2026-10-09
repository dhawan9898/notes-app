---
title: The Shell — Parsing, Expansion, Redirection, Pipes and Job Control
track: linux
module: Shell and Text Processing
moduleOrder: 2
order: 1
level: intermediate
summary: What the shell actually does with a command line — the order of expansions, quoting and word splitting, redirection semantics, pipelines and subshells, exit statuses, startup files and job control.
sources:
  - label: "POSIX.1-2017 — Shell Command Language"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
  - label: "GNU Bash Reference Manual"
    url: https://www.gnu.org/software/bash/manual/bash.html
  - label: "man 7 signal, man 7 credentials (sessions and process groups), man 2 setsid"
    url: https://man7.org/linux/man-pages/man7/signal.7.html
  - label: "Greg's Wiki — Bash Pitfalls and BashFAQ"
    url: https://mywiki.wooledge.org/BashPitfalls
---

The shell is a programming language whose "functions" are processes. Most scripting bugs come from misunderstanding **when** and **how** a command line is transformed before any program runs. A program such as `ls` never sees `*.txt` or `$HOME`: the shell expands them first.

## From text to process

For each line the shell: **reads → tokenizes (words and operators) → parses (commands, pipelines, lists, compound commands) → expands → sets up redirections → executes** (a builtin directly, or `fork` + `execve`). Quoting controls which of these steps are applied.

### Quoting

| Form | Effect |
|---|---|
| `\c` | the next character is literal |
| `'...'` | everything literal (no escapes, cannot contain `'`) |
| `"..."` | literal **except** `$`, `` ` ``, `\`, and (history) `!`: variables, command and arithmetic substitution still happen, but **no word splitting or globbing** |
| `$'...'` (bash) | ANSI-C escapes: `$'a\tb\n'` |

The golden rule: **quote every expansion** (`"$var"`, `"$(cmd)"`, `"${arr[@]}"`) unless you explicitly want splitting and globbing.

## The order of expansions (bash)

1. **Brace expansion** `{a,b}`, `{1..5}` → creates words, purely textual (`file{1,2}.txt`).
2. **Tilde expansion** `~`, `~user`.
3. **Parameter and variable expansion** `$v`, `${v:-default}`; **command substitution** `$(cmd)`; **arithmetic** `$((1+2))`; **process substitution** `<(cmd)` (left-to-right, same stage).
4. **Word splitting**: the results of *unquoted* expansions are split on characters in `IFS` (default: space, tab, newline).
5. **Pathname expansion (globbing)**: `*`, `?`, `[...]` matched against the filesystem; no match → pattern left as is (unless `nullglob`/`failglob`).
6. **Quote removal**.

```bash
$ v="a  b   c"
$ printf '[%s]' $v;   echo        # unquoted: split into 3 words
[a][b][c]
$ printf '[%s]' "$v"; echo        # quoted: one word, spaces preserved
[a  b   c]
```

Because splitting happens **after** variable expansion, a filename with spaces held in `$f` becomes several arguments: the classic bug behind `rm $f` deleting the wrong things. Because globbing comes after splitting, `rm *` with a file called `-rf` becomes an option: use `rm -- *` or `./*`.

### `$@` versus `$*`

```bash
$ set -- "a b" c
$ printf '<%s>' "$@"; echo      # each positional parameter as its own word
<a b><c>
$ printf '<%s>' "$*"; echo      # one word joined by the first char of IFS
<a b c>
```

Use `"$@"` to forward arguments. Arrays: `"${arr[@]}"` expands each element as a word.

### Parameter expansion toolkit

| Syntax | Meaning |
|---|---|
| `${v:-d}` / `${v-d}` | value or default `d` if unset or empty / only if unset |
| `${v:=d}` | assign default |
| `${v:?msg}` | error and exit if unset or empty |
| `${v:+x}` | `x` if set and non-empty |
| `${#v}` | length |
| `${v#pat}` / `${v##pat}` | remove shortest / longest **prefix** matching a glob |
| `${v%pat}` / `${v%%pat}` | remove shortest / longest **suffix** |
| `${v/pat/rep}` / `${v//pat/rep}` | replace first / all |
| `${v:off:len}` | substring |
| `${v^^}` / `${v,,}` | upper / lower case (bash 4) |

```bash
$ f=/home/u/archive.tar.gz
$ echo "${f##*/} ${f%.gz} ${f%%.*} ${f/u/U} ${#f}"
archive.tar.gz /home/u/archive.tar /home/u/archive /home/U/archive.tar.gz 22
```

These avoid forking `basename`, `dirname`, `sed`.

## Variables and the environment

*Shell variables* exist only in the shell; **exported** variables are copied into the environment of child processes (`export VAR`, or `VAR=x cmd` for one command). Children cannot modify the parent's variables or `cd` for it, which is why `cd` and `source` are built-ins. `env`, `printenv`, `declare -p`, `set` list them.

Arrays: `a=(x y z)`, `${a[1]}`, `"${a[@]}"`, `${#a[@]}`; associative arrays (bash ≥ 4): `declare -A m; m[key]=val`.

## Redirection

Redirections are processed **left to right**, and each one rewires a numbered file descriptor (see the VFS note):

| Syntax | Meaning |
|---|---|
| `> f` / `>> f` / `< f` | fd 1 truncate / append / fd 0 from file |
| `2> f` | fd 2 to file |
| `2>&1` | make fd 2 a **duplicate of what fd 1 currently points to** |
| `&> f` (bash) | both stdout and stderr to file |
| `<<EOF ... EOF` / `<<-` / `<<'EOF'` | here-document (quote the delimiter to suppress expansion) |
| `<<< "text"` | here-string |
| `exec 3> log` | open fd 3 for the rest of the script; `exec 3>&-` closes |
| `/dev/tcp/host/port` (bash) | open a TCP socket as a file |

**Order matters:**

```bash
$ ls /nonexistent > out.txt 2>&1   # stdout -> file, THEN stderr -> where stdout points: both in the file
$ ls /nonexistent 2>&1 > out.txt   # stderr -> terminal/pipe (old stdout), THEN stdout -> file: error still on screen
```

`cmd > f` truncates `f` **before** the command starts, so `sort f > f` empties your file. Use a temporary file and `mv`, or `sponge`.

## Pipelines, subshells and exit status

`a | b | c` starts all stages **concurrently** (each as a separate process, in a new subshell), connecting stdout→stdin through kernel pipes with a 64 KiB buffer (by default); a writer blocks when the pipe is full, and writing to a pipe with no reader delivers `SIGPIPE` (so `yes | head` terminates).

Consequences:

```bash
n=0; printf 'x\ny\n' | while read l; do n=$((n+1)); done; echo $n     # prints 0 !
# the loop ran in a subshell; its variable changes are lost.
n=0; while read l; do n=$((n+1)); done < <(printf 'x\ny\n'); echo $n   # prints 2 (process substitution keeps the loop in the current shell)
```

(Alternatively `shopt -s lastpipe` with job control off, or a here-string/file.)

### Exit status

Each command returns 0–255; **0 means success**. `$?` holds the last status. Conventions: `1` general error, `2` misuse, **`126`** found but not executable, **`127`** not found, **`128+N`** killed by signal N (`137` = SIGKILL, `143` = SIGTERM, `130` = Ctrl-C).

A pipeline's status is that of the **last** command, unless `set -o pipefail` (then the rightmost non-zero):

```bash
$ false | true; echo $?          # 0
$ set -o pipefail; false | true; echo $?    # 1
```

`&&` and `||` short-circuit on status: `cmd1 && cmd2 || cmd3` is **not** if/else (`cmd3` also runs if `cmd2` fails).

### `set -e` and friends

| Option | Effect | Caveats |
|---|---|---|
| `set -e` (errexit) | exit when a command fails | ignored in `if`/`while` conditions, `&&`/`||` lists (except the last), `!`; behaves surprisingly in functions called in those contexts |
| `set -u` | error on unset variable | `"${arr[@]}"` on an empty array in old bash |
| `set -o pipefail` | pipeline status | `grep -q` closing the pipe early causes SIGPIPE status 141 |
| `set -x` | trace commands (to stderr) | |

And the notorious masking: `local x=$(false)` **returns the status of `local`** (0), not of the substitution: declare first, assign after (`local x; x=$(cmd)`).

## Grouping and subshells

- `( list )`: runs in a **subshell** (a forked copy): variable and `cd` changes don't leak out.
- `{ list; }`: runs in the current shell, to apply a redirection to several commands.
- `$(...)`: command substitution runs in a subshell and strips trailing newlines.
- `&`: run in the background; `wait` collects.

## Command lookup

For a command word the shell checks, in order: **alias** (interactive), **function**, **builtin**, then the hash table / `$PATH` search. A command containing `/` is executed directly. `type -a cmd` shows all matches; `command -v cmd` is the portable "does it exist" test; `hash -r` clears the cache after PATH changes. Beware of `.` in `PATH` and of relative entries.

## Startup files

| Shell kind | Files read (bash) |
|---|---|
| **Login** (ssh, `su -`, console) | `/etc/profile`, then the first of `~/.bash_profile`, `~/.bash_login`, `~/.profile` |
| **Interactive non-login** (new terminal window) | `~/.bashrc` (via `/etc/bash.bashrc`) |
| **Non-interactive** (scripts) | none, except `$BASH_ENV` if set |

Put environment (PATH, exports) in the login files and interactive niceties (aliases, prompt) in `.bashrc`; make `.bash_profile` source `.bashrc`. systemd services and cron **do not** read them, a frequent "works in my shell, not in cron" cause: cron's PATH is minimal.

## Job control, sessions and signals

The kernel organizes processes into **process groups** and **sessions**; a session may have a **controlling terminal** whose **foreground process group** receives terminal-generated signals:

| Key | Signal | Effect |
|---|---|---|
| Ctrl-C | `SIGINT` | interrupt foreground group |
| Ctrl-Z | `SIGTSTP` | stop (suspend) |
| Ctrl-\\ | `SIGQUIT` | quit with core dump |
| Ctrl-D | (EOF on input) | end of input; exits the shell at an empty prompt |
| terminal closes | `SIGHUP` | sent to the session; background jobs die unless `nohup`/`disown`/`setsid` |

Interactive shells put each pipeline in its own process group: `cmd &` (background), `jobs`, `fg %1`, `bg %1`, `kill %1`, `disown -h %1`. A background job reading the terminal gets `SIGTTIN` (stopped). To survive logout: `nohup cmd &` (ignores SIGHUP, writes `nohup.out`), `setsid cmd` (new session), or better a systemd unit/`tmux`/`screen`. Scripts normally don't have job control, so background children stay in the script's group.

`trap 'handler' SIGNAL...` runs handler on signals or on `EXIT`/`ERR`/`DEBUG`; `trap '' INT` ignores; `kill -l` lists signals.

## Globbing and patterns

`*` (any), `?` (one), `[a-z]`, `[!x]`; with `shopt -s extglob`: `?(p)`, `*(p)`, `+(p)`, `@(p)`, `!(p)`; `globstar` makes `**` recursive; `dotglob` includes dotfiles; `nullglob` expands to nothing instead of the literal pattern; `failglob` errors. Globs are **not** regular expressions (the `[[ $x =~ regex ]]` operator and `case` patterns are different again). `case` uses glob patterns: `case $x in *.gz) ...;; esac`.

## Bash vs. POSIX sh

`/bin/sh` is bash on some systems, **dash** on Debian/Ubuntu (faster, strictly POSIX): no `[[ ]]`, arrays, `<<<`, process substitution, `${v//}`, `local` is common but not POSIX. Choose the shebang accordingly: `#!/bin/sh` (portable) or `#!/usr/bin/env bash`. Test with `dash script.sh`, `shellcheck -s sh`.

## Key takeaways

- Expansion order: braces → tilde → parameter/command/arithmetic → word splitting → globbing → quote removal; quote to stop the last two.
- Redirections apply left-to-right; `2>&1 >f` and `>f 2>&1` differ.
- Pipeline stages are concurrent subshells; their variable changes vanish; status is the last command's unless `pipefail`.
- Exit codes: 126/127/128+N; `set -e` has many exceptions; `local x=$(cmd)` hides failures.
- Startup files differ for login, interactive and non-interactive shells; cron/systemd skip them.
- Job control rides on process groups, sessions and the controlling terminal; use `nohup`, `setsid` or systemd for persistence.
