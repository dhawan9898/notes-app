#!/bin/bash
# Minimal hardening report for an ELF binary (a tiny subset of the real `checksec`).
f=$1
type=$(readelf -h "$f" | awk '/Type:/ {print $2}')
pie=$([ "$type" = DYN ] && echo yes || echo no)
relro=none
if readelf -lW "$f" | grep -q GNU_RELRO; then relro=partial; readelf -dW "$f" | grep -qE 'BIND_NOW|FLAGS.*NOW' && relro=full; fi
canary=$(readelf -sW "$f" | grep -q __stack_chk_fail && echo yes || echo no)
nx=$(readelf -lW "$f" | awk '/GNU_STACK/ {print ($0 ~ /RWE/) ? "no (executable stack)" : "yes"}')
fortify=$(readelf -sW "$f" | grep -qE '__(str|mem)[a-z]*_chk|__printf_chk|__snprintf_chk' && echo yes || echo no)
printf "%-14s PIE=%-3s RELRO=%-7s canary=%-3s NX=%-4s fortify=%s\n" "$(basename "$f")" "$pie" "$relro" "$canary" "$nx" "$fortify"
