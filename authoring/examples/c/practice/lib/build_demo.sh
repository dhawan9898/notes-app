#!/bin/bash
# Build the same library as a static archive and as a shared object, and compare how programs use them.
set -u
cleanup() { rm -f ./*.o ./*.a ./*.so app_static app_shared app_rpath bad ok2; }
cleanup; trap cleanup EXIT

echo '$ gcc -c -fPIC mathx.c && ar rcs libmathx.a mathx.o && gcc -o app_static app.c -L. -lmathx'
gcc -Wall -Wextra -c -fPIC mathx.c && ar rcs libmathx.a mathx.o && gcc -Wall -Wextra -o app_static app.c -L. -lmathx && ./app_static

echo; echo '$ nm libmathx.a        (t = private to the object, T = exported)'
nm libmathx.a | grep -v '^$'

echo; echo '$ gcc -shared -o libmathx.so mathx.o && gcc -o app_shared app.c -L. -lmathx'
gcc -shared -o libmathx.so mathx.o && gcc -o app_shared app.c -L. -lmathx
echo '$ ./app_shared'; ./app_shared 2>&1 | head -1
echo '$ ldd ./app_shared | grep mathx'; ldd ./app_shared | grep mathx
echo '$ LD_LIBRARY_PATH=. ./app_shared'; LD_LIBRARY_PATH=. ./app_shared

echo; echo '$ gcc -o app_rpath app.c -L. -lmathx -Wl,-rpath,"$ORIGIN" && ./app_rpath'
gcc -o app_rpath app.c -L. -lmathx -Wl,-rpath,'$ORIGIN' && ./app_rpath

echo; echo '$ nm -D libmathx.so | grep " T "'; nm -D libmathx.so | grep ' T '
echo '$ gcc -shared -fvisibility=hidden -o libmathx.so mathx.c && nm -D libmathx.so | grep -c " T "'
gcc -shared -fvisibility=hidden -o libmathx.so mathx.c && nm -D libmathx.so | grep -c ' T '

echo; echo '$ gcc -o bad -lmathx -L. app.c      (library named BEFORE the code that needs it, static archive only)'
rm -f libmathx.so; gcc -c -fPIC mathx.c && ar rcs libmathx.a mathx.o
gcc -o bad -L. -lmathx app.c 2>&1 | sed 's#/tmp/[^ ]*/##g' | grep -E "undefined reference" | head -2
echo '$ gcc -o ok2 -L. app.c -lmathx'; gcc -o ok2 -L. app.c -lmathx && ./ok2
