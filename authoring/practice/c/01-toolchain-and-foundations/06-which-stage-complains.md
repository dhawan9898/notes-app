---
kind: question
title: Which stage complains?
module: Toolchain and Language Foundations
difficulty: Medium
---

For each problem, name the build stage that reports it (preprocessor, compiler, assembler, linker or run time): (a) `#include <stdoi.h>` (b) calling `square(7)` without a prototype under `-std=c17` (c) calling a function that is declared but defined in no file (d) `int x = "hello";` (e) an off-by-one write past a local array.

## Explanation

(a) **Preprocessor**: "No such file or directory" for the header. (b) **Compiler**: "implicit declaration of function" is an error from C99 on (see *Functions, Headers and Multi-File Programs*). (c) **Linker**: "undefined reference to `square`". (d) **Compiler**: initialisation of `int` from `char *` (a diagnostic; many compilers treat it as an error by default now). (e) **Run time**: no build stage sees it; only a sanitizer or a crash reveals it.
