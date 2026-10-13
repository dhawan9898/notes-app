#!/bin/bash
set -e
gcc -O2 -Wall -Wextra -fPIC -shared -fvisibility=hidden acc.c -o libacc.so
echo '$ nm -D --defined-only libacc.so | grep acc_      (the exported C symbols: plain names, nothing mangled)'
nm -D --defined-only libacc.so | grep " T " | awk '{print "  " $3}'
echo
g++ -std=c++17 -O2 -Wall -Wextra use.cpp -L. -lacc -Wl,-rpath,'$ORIGIN' -o use_cpp
./use_cpp
echo
python3 use.py
echo
echo "--- the same calls with no ctypes prototypes:"
python3 use.py broken
echo
echo '--- name mangling: what the same function would be called if C++ had compiled it without extern "C"'
cat > /tmp/m.cpp <<'CPP'
int acc_add(void *a, double x);
namespace ns { int acc_add(void *a, double x) { return 0; } }
int acc_add(void *a, double x) { return 1; }
CPP
g++ -c /tmp/m.cpp -o /tmp/m.o && nm /tmp/m.o | grep " T " | awk '{print "  " $3}'; rm -f /tmp/m.cpp /tmp/m.o
