#!/bin/bash
# Tiny regression test: run the program on known input and compare with the expected output.
set -u
actual=$(printf 'The cat and the hat. The CAT sat!\n' | ./wordfreq -n 3)
expected=$'   3  the\n   2  cat\n   1  and\n(5 distinct words)'
if [ "$actual" == "$expected" ]; then echo "PASS: counts and ordering"; else echo "FAIL"; echo "$actual"; exit 1; fi

./wordfreq /nonexistent 2>/dev/null; [ $? -eq 1 ] && echo "PASS: missing file gives exit status 1"
printf '' | ./wordfreq | grep -q "(0 distinct words)" && echo "PASS: empty input"
