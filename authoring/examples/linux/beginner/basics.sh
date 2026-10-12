#!/bin/bash
# A guided tour of everyday commands in a throwaway directory. Every line that starts with "$ " is shown with its real output.
set -u
work=$(mktemp -d); cd "$work"
run() { echo "\$ $*"; eval "$@" 2>&1; }

run 'pwd | sed "s#^/tmp/[^/]*#/tmp/SANDBOX#"'
run 'mkdir -p project/src project/docs'
run 'ls project'
run 'echo "hello linux" > project/docs/note.txt'
run 'printf "alpha\nbeta\ngamma\nbeta\nalpha\nbeta\n" > project/src/words.txt'
run 'cat project/docs/note.txt'
run 'cp project/docs/note.txt project/docs/copy.txt'
run 'mv project/docs/copy.txt project/docs/renamed.txt'
run 'ls project/docs'
run 'stat -c "%A %s bytes  %n" project/docs/note.txt'
run 'cd project/src && pwd | sed "s#^/tmp/[^/]*#/tmp/SANDBOX#" && cd ../..'
run 'wc -l project/src/words.txt'
run 'grep -n beta project/src/words.txt'
run 'sort project/src/words.txt | uniq -c | sort -rn'
run 'find project -type f -name "*.txt" | sort'
run 'echo "appended" >> project/docs/note.txt; cat project/docs/note.txt'
run 'ls project/nothing-here'
run 'ls project/nothing-here 2> errors.log; echo "exit status: $?"; cat errors.log'
run 'chmod 600 project/docs/note.txt; stat -c "%A %n" project/docs/note.txt'
run 'chmod u+x,go+r project/docs/note.txt; stat -c "%A %n" project/docs/note.txt'
run 'name=World; echo "Hello, $name"; echo "files here: $(ls project | wc -l)"'
run 'rm project/docs/renamed.txt; rm -r project/docs; ls project'
cd /; rm -rf "$work"
