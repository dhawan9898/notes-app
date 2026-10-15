---
title: Pipelines, Text Tools and Safe Scripts
track: linux
module: Shell and Text Processing
moduleOrder: 1
order: 1
level: intermediate
minutes: 40
summary: "Six exercises on the shell module: a word-frequency pipeline, per-group averages with awk, filtering and rewriting a log with grep and sed, the unquoted-loop bug, the empty-variable rm trap and set -euo pipefail, with real captured output and explanations."
prerequisites:
  - "The Shell — Parsing, Expansion, Redirection, Pipes and Job Control"
  - "Text Processing (grep, sed, awk) and Robust Shell Scripting"
sources:
  - label: "POSIX.1-2017 Shell Command Language and Utilities (awk, sed, grep, tr, sort, uniq)"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/
  - label: "GNU Bash Reference Manual — Shell Expansions and The Set Builtin"
    url: https://www.gnu.org/software/bash/manual/bash.html
  - label: "Robbins — Effective awk Programming (GNU Awk User's Guide)"
    url: https://www.gnu.org/software/gawk/manual/gawk.html
---

Text-processing skill comes from building small pipelines until they feel natural. Create the three sample files first, then solve each exercise **before** opening the answers.

```bash
printf 'the quick brown fox jumps over the lazy dog\nThe dog barks; the fox runs away.\nA quick brown dog? No: the fox!\n' > story.txt
printf 'name,dept,salary\nada,eng,120\ngrace,eng,150\nlinus,ops,90\ndennis,ops,110\nbarbara,research,130\n' > staff.csv
printf '2026-10-01 INFO  started\n2026-10-01 WARN  disk 91%% full\n2026-10-02 ERROR timeout talking to db\n2026-10-02 INFO  retry ok\n2026-10-03 ERROR disk write failed\n' > app.log
```

## Exercises

**1. Word frequency.** Print the four most common words in `story.txt`, ignoring case and punctuation, as `count word` lines.

**2. Group statistics.** From `staff.csv`, print the average salary per department, sorted by department name. Then list only the names of people earning at least 120.

**3. Log triage.** From `app.log`, (a) count the `ERROR` lines, (b) show every line that is not `INFO`, (c) print just the date and level of each `WARN` or `ERROR` line, (d) print the errors with `ERROR` replaced by `FAILURE` without changing the file.

**4. Edit in place, safely.** Lower-case the word `INFO ` to `info ` inside `app.log` itself, keeping a backup, and show the differences.

**5. The unquoted loop.** A directory contains `my notes.txt` and `plain.txt`. Why does `for f in $(ls files); do echo "[$f]"; done` print three items, and what is the correct loop? How do you pass such names safely to another command (for example with `find` and `xargs`)?

**6. Make a script fail safely.** Explain what `rm -rf $dir/*` does when `dir` is empty, and write the guard-rail header for a script: what do `set -e`, `set -u` and `set -o pipefail` each protect against? Include `${1:?message}`, a directory check and cleanup of a temporary directory with `trap`.

## Answers

Try before opening.

<details>
<summary>Show answers and real output</summary>

**1.**

```text
$ tr -cs 'A-Za-z' '\n' < story.txt | tr 'A-Z' 'a-z' | sort | uniq -c | sort -rn | head -4
      5 the
      3 fox
      3 dog
      2 quick
```

The first `tr -cs 'A-Za-z' '\n'` replaces every run of non-letters (`-c` complement, `-s` squeeze) with one newline, giving one word per line; the second lower-cases. `sort` groups identical words so `uniq -c` can count adjacent duplicates (**`uniq` only compares neighbouring lines, so it needs sorted input**); `sort -rn` orders by count, highest first. The classic idiom works because each tool does one job and the pipe connects them.

**2.**

```text
$ awk -F, 'NR>1 {sum[$2]+=$3; n[$2]++} END {for (d in sum) printf "%s avg %.1f\n", d, sum[d]/n[d]}' staff.csv | sort
eng avg 135.0
ops avg 100.0
research avg 130.0
$ awk -F, 'NR>1 && $3>=120 {print $1}' staff.csv
ada
grace
barbara
```

`-F,` sets the field separator, `NR>1` skips the header line, and the associative arrays `sum` and `n` accumulate per department; the `END` block prints once at the end. awk's `for (d in sum)` visits keys in unspecified order, so the output is piped through `sort`. A condition before the braces (`NR>1 && $3>=120`) selects lines, like a filter. For CSV with quoted commas inside fields, awk is not enough; use a real CSV parser.

**3.**

```text
$ grep -c ERROR app.log
2
$ grep -v INFO app.log | cut -c1-30
2026-10-01 WARN  disk 91% full
2026-10-02 ERROR timeout talki
2026-10-03 ERROR disk write fa
$ grep -E 'ERROR|WARN' app.log | awk '{print $1, $2}'
2026-10-01 WARN
2026-10-02 ERROR
2026-10-03 ERROR
$ sed 's/ERROR/FAILURE/' app.log | grep FAILURE
2026-10-02 FAILURE timeout talking to db
2026-10-03 FAILURE disk write failed
```

`grep -c` counts matching lines (not occurrences), `-v` inverts, `-E` enables extended regular expressions (`|` for alternation). Without `-i`, `sed` and `grep` are case-sensitive. Without `-i` on `sed`, the original file is untouched; the output goes to the pipe only.

**4.**

```text
$ sed -i.orig 's/INFO /info /' app.log && ls app.log* && diff app.log.orig app.log
app.log
app.log.orig
1c1
< 2026-10-01 INFO  started
---
> 2026-10-01 info  started
4c4
< 2026-10-02 INFO  retry ok
---
> 2026-10-02 info  retry ok
```

`sed -i.orig` edits in place **and** keeps the original as `app.log.orig` (the suffix is appended to the file name). Plain `sed -i` leaves no backup, so one wrong regular expression is unrecoverable. `diff` shows exactly what changed: `<` is the old line, `>` the new one. For anything important, also use version control.

**5.**

```text
$ for f in $(ls files); do echo "[$f]"; done
[my]
[notes.txt]
[plain.txt]
$ for f in files/*; do echo "[$f]"; done
[files/my notes.txt]
[files/plain.txt]
$ find files -type f -name "*.txt" -print0 | xargs -0 -n1 echo file:
file: files/plain.txt
file: files/my notes.txt
```

`$(ls files)` produces text that the shell then **splits on whitespace**, so `my notes.txt` becomes two words. A glob (`files/*`) expands to one word per file name, spaces included, so the loop is correct as long as you also quote `"$f"` when you use it. When another program must receive the names, separate them with **NUL bytes** (`-print0` and `xargs -0`), the only character that cannot appear in a file name. (The order of `find` output is not guaranteed.) Never parse `ls`.

**6.**

```text
$ dir=""; echo rm -rf $dir/* | cut -c1-60
rm -rf /bin /boot /cscope.files /dev /etc /home /initrd.img
```

(`echo` shows what would run; nothing was deleted.) With `dir` empty, `$dir/*` becomes `/*`: *every top-level directory*. That is how a typo in one variable becomes a destroyed system. The header below guards against it:

```bash
#!/bin/bash
set -euo pipefail
dir=${1:?usage: safe.sh DIRECTORY}
[ -d "$dir" ] || { echo "not a directory: $dir" >&2; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
echo "working in $dir, scratch space is private and cleaned up on exit"
```

```text
$ ./safe.sh; echo "exit status $?"
./safe.sh: line 3: 1: usage: safe.sh DIRECTORY
exit status 1
$ ./safe.sh /nonexistent; echo "exit status $?"
not a directory: /nonexistent
exit status 1
$ ./safe.sh files; echo "exit status $?"
working in files, scratch space is private and cleaned up on exit
exit status 0
```

- `set -e` stops the script when a command fails, so one failure does not silently become ten.
- `set -u` turns the use of an unset variable into an error (so `rm -rf $dirr/*` with a typo fails instead of expanding to `/*`).
- `set -o pipefail` makes a pipeline fail if **any** stage fails, not only the last:

```text
$ bash -c 'set -o pipefail; false | true; echo "with pipefail: $?"'; bash -c 'false | true; echo "without pipefail: $?"'
with pipefail: 1
without pipefail: 0
```

- `${1:?message}` aborts with a usage message when the argument is missing, `[ -d ... ]` validates it, and `trap '...' EXIT` removes the temporary directory however the script ends (success, error or interrupt). Quote every `"$variable"`. These options reduce risk but do not replace testing: `set -e` has exceptions (for example inside `if` conditions), so check important commands explicitly.

</details>

## Key takeaways

1. Build pipelines from single-purpose tools (`tr`, `sort`, `uniq`, `awk`, `grep`, `sed`); remember `uniq` needs sorted input.
2. Use `sed -i.orig` (or version control) so an edit in place can be undone; `diff` shows what changed.
3. Never loop over `$(ls)`; use globs, and use `-print0`/`xargs -0` to pass arbitrary file names.
4. Start scripts with `set -euo pipefail`, validate arguments with `${1:?}` and `[ -d ]`, and clean up with `trap ... EXIT`.
