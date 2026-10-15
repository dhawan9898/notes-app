---
kind: code
title: Comparators for qsort
module: Advanced Language
difficulty: Medium
tags:
  - qsort
  - callbacks
  - overflow
starter: c/starters/callbacks/ex3-comparators.c
examples:
  - input: ages {Linus 55, Ada 36, Big 2000000000, Small -2000000000}
    output: Small Ada Linus Big (by age)
  - input: names {Linus, Ada, Dennis}
    output: Ada Dennis Linus (by name)
constraints:
  - The comparator receives `const void *` pointing at the **elements** (here, pointers to `Person`).
  - Ages can be anywhere in the `int` range.
hints:
  - 'Cast: `const Person *a = x;`.'
  - Compare with `(a->age > b->age) - (a->age < b->age)`.
  - For names, `strcmp(a->name, b->name)` already has the right contract.
---

Write two `qsort` comparators for a `Person` (a name and an `int` age): `by_age` and `by_name`. Each returns a negative number, zero or a positive number, like `strcmp`.

The obvious `return a->age - b->age;` is **wrong**: the subtraction can overflow for extreme ages and then returns the wrong sign.

## Explanation

The same program sorts with both comparators (the by-age line, then the by-name line above). `qsort` calls the comparator with `const void *`, so the first line of each comparator casts to the real type. The comparator must return negative, zero or positive; `(a > b) - (a < b)` does that for integers without overflow, whereas `a->age - b->age` can overflow for large values (and for unsigned types never goes negative). Use `strcmp` for strings.
