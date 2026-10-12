---
title: Linux Command Line Basics
track: linux
module: Start Here
moduleOrder: 0
order: 2
level: beginner
summary: "The terminal and shell, the file tree and paths, the dozen commands you use every day (ls, cd, cp, mv, rm, cat, grep, find, sort), redirection and pipes, permissions in practice, variables, exit statuses and how to get help, all run in a sandbox with real output."
sources:
  - label: Shotts — The Linux Command Line (free book), Parts 1-2
    url: https://linuxcommand.org/tlcl.php
  - label: POSIX.1-2017 Shell Command Language and Utilities
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: GNU Coreutils manual
    url: https://www.gnu.org/software/coreutils/manual/coreutils.html
  - label: Bash Reference Manual
    url: https://www.gnu.org/software/bash/manual/bash.html
---

The **terminal** is a window where you type commands; the **shell** (usually `bash`) is the program inside it that reads each line, runs the commands and shows the output. Graphical tools hide what is happening; the command line shows it, lets you combine small tools into larger ones, and is what you will use on every server.

A command has the form `command [options] [arguments]`, for example `ls -l /etc`: the command is `ls`, the option `-l` changes its behavior, and `/etc` is what to act on. The shell shows a **prompt** (`$` for an ordinary user, `#` for root) where you type.

## The file tree and paths

Linux has one tree of directories starting at the **root**, `/`. There are no drive letters. Common places: `/home/you` (your files), `/etc` (configuration), `/var/log` (logs), `/tmp` (temporary files), `/usr/bin` (programs). *Filesystem Hierarchy, VFS, Inodes, Links and File Descriptors* explains the rest.

- An **absolute path** starts with `/` and means the same thing everywhere: `/etc/hostname`.
- A **relative path** starts from your **current directory**: `docs/note.txt`. `.` means the current directory, `..` its parent, and `~` your home directory.
- File names are **case-sensitive** (`Note.txt` and `note.txt` differ) and a name starting with `.` is hidden (shown by `ls -a`).
- Use `pwd` to see where you are and `cd` to move: `cd /tmp`, `cd ..`, `cd` (home), `cd -` (back).

## A guided tour

The script below runs a series of the most useful commands in a throwaway directory. Read the commands and the output together. (It is a normal bash script; saving it and running `bash basics.sh` reproduces everything on your own machine.)

{{include linux/beginner/basics.sh}}

{{run linux/beginner :: bash basics.sh}}

### What each group did

**Creating and organising.** `mkdir -p` makes a directory (and parents); `echo "..." > file` writes text into a file (creating or **overwriting** it); `printf` is like `echo` with precise formatting; `cp` copies; `mv` moves **or renames**; `rm` deletes (**there is no recycle bin**); `rm -r` deletes a directory and everything in it; `ls` lists (add `-l` for details, `-a` for hidden files, `-h` for readable sizes).

**Looking at files.** `cat` prints a file; `wc -l` counts lines; `stat` shows details: the first column in `-rw-r--r--` is the file type (`-` file, `d` directory) followed by permissions; `grep -n beta file` prints the lines containing a word with their line numbers; `find project -type f -name "*.txt"` searches a tree by name, type, size, date; `less file` (not shown, it is interactive) scrolls a long file, `q` to quit; `head` and `tail` show the first and last lines.

**Pipes and redirection.** The shell connects programs:

- `>` writes output to a file (overwriting), `>>` appends, `<` reads input from a file.
- `2>` redirects **error messages** (a second output stream, "standard error"), so errors can be kept apart from normal output: the demonstration shows `ls` failing with *exit status 2* and its message going into `errors.log`.
- `|` (the **pipe**) feeds the output of one command to the input of the next: `sort words.txt | uniq -c | sort -rn` sorts the words, counts consecutive duplicates, and sorts by count: *the most frequent word is beta (3)*. This is the core Unix idea: **small tools, each doing one thing, joined by pipes** (see *Text Processing (grep, sed, awk) and Robust Shell Scripting*).

**Permissions.** `-rw-r--r--` is three triples for **owner**, **group** and **others**, each `r`ead, `w`rite, e`x`ecute. `chmod 600 file` sets owner read+write only (numbers: r=4, w=2, x=1, so 6 = rw, 0 = nothing); `chmod u+x,go+r` adds execute for the owner (`u`) and read for group and others (`g`,`o`). A script must have the execute bit to run as `./script`. (Directory permissions mean list/enter; see *Users, Permissions, ACLs and Capabilities*.)

**Variables and substitution.** `name=World` sets a variable (no spaces around `=`), `$name` uses it, and `$(command)` is replaced by the command's output. Quote variables (`"$name"`) so that spaces in values do not split them into several words: forgetting quotes is the classic shell bug.

## Exit status: did it work?

Every command ends with a number: **0 means success, non-zero means failure**. The shell keeps the last one in `$?`. Scripts and `&&` / `||` use it: `make && ./run` runs the second command only if the first succeeded; `cmd1 || cmd2` runs the second only on failure.

## Getting help

- `man ls` opens the manual page (`q` quits, `/word` searches). Sections matter: `man 2 open` is the system call, `man 1 open`/`man 3 printf` other kinds.
- `ls --help` prints a short summary. `type cmd` or `which cmd` shows what will run; `apropos keyword` searches manual pages.
- Tab completes file and command names; the **up arrow** and `Ctrl-R` search your history; `Ctrl-C` interrupts a running command; `Ctrl-D` ends input.

## Habits that prevent disasters

- **Look before you delete**: run `ls pattern` before `rm pattern`. Never run `rm -rf` with a variable you have not checked (`rm -rf "$dir/"` with an empty `$dir` is `rm -rf /`).
- Do not paste commands you do not understand, especially with `sudo`.
- `>` **silently overwrites**: `echo hi > important.conf` destroys the old content. Use `>>` to append, and keep backups.
- Quote file names that may contain spaces.
- Use `sudo` for the one command that needs it, not a root shell.

## Pitfalls

- Spaces: `name = value` is an error; `rm my file` is two arguments.
- Relative paths depend on the current directory; check with `pwd`.
- Case sensitivity.
- Forgetting that `*` is expanded by the **shell** before the command sees it.
- Assuming a command's silence means failure (many succeed silently; check `$?`).
- Hidden files are not matched by `*`.

## Key takeaways

1. The shell reads a command, runs it, and shows output; commands are `command options arguments`.
2. One tree starting at `/`; absolute and relative paths; `.`, `..`, `~`.
3. `ls cd pwd cp mv rm mkdir cat grep find sort wc` cover most daily work.
4. `>`, `>>`, `2>` and `|` connect commands; small tools plus pipes are the Unix style.
5. Permissions are owner/group/others × read/write/execute; `chmod` changes them.
6. Exit status 0 means success; use `man` and `--help`; look before you delete.

**Next:** *Installing Software and Managing Services*, then *Practice: Linux Exercises*.
