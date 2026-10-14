#!/bin/bash
# usage: ./san.sh file.c   builds with AddressSanitizer + UBSan, runs, and prints the lines of the report that matter
f=$1
gcc -std=c17 -g -fsanitize=address,undefined -o san_prog "$f" 2>&1 | grep -E "warning" | sed 's/^[^ ]* //' | head -2
./san_prog 2>&1 | grep -E "ERROR|runtime error|WRITE of|READ of|is located|freed by|previously allocated|SUMMARY" \
  | sed -e 's/==[0-9]*==//' -e 's/ (pc .*//' -e 's/ on address 0x[0-9a-f]*//' -e 's#/[^ ]*/##g' -e 's/0x[0-9a-f]\{6,\}/ADDR/g' | head -7
rm -f san_prog
