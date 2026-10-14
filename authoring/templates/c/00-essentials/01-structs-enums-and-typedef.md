---
title: "Structs, Enums and Typedef"
track: c
module: C Essentials
moduleOrder: 0.5
order: 1
level: beginner
summary: "How to group related values into one type with struct, name a fixed set of choices with enum, shorten type names with typedef, and pass structs to functions by value or by pointer, shown with compiled programs and a first linked list."
prerequisites:
  - "C From Zero: Your First Programs"
  - Pointers for Beginners
sources:
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 6 (Structures)
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 16 (Structures, Unions, and Enumerations)
  - label: ISO/IEC 9899:2018 §6.7.2.1 (Structure and union specifiers), §6.7.2.2 (Enumeration specifiers), §6.7.8 (Type definitions)
  - label: GCC manual — Designated Initializers
    url: https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html
---

So far every variable held one value. Real programs deal with things that have several parts: a point has an x and a y, a student has a name and a mark, a network packet has a source and a destination. C lets you glue the parts into one **new type** with `struct`, and lets you give names to a fixed list of choices with `enum`. This note is the missing step between *C From Zero* and the later notes, which use structs on almost every page.

## A struct: several values that belong together

A `struct` declares a type made of named **members**. Declaring the type reserves no memory; creating a variable of that type does.

{{include c/essentials/struct1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o struct1 struct1.c && ./struct1}}

What to take from the program:

- `struct point a = {3, 4};` fills the members in the order they were declared. `{.y = 9, .x = 1}` fills them **by name** (a *designated initializer*), which is clearer and survives someone adding a member later. Members you do not mention are set to zero.
- `a.x` (dot) reaches a member of a struct **variable**. `p->x` (arrow) reaches a member **through a pointer**; it is shorthand for `(*p).x`.
- Assignment (`struct point c = a;`) **copies every member**. Changing `c` does not touch `a`.
- Passing a struct to a function also copies it, exactly like passing an `int`. That is why `move_wrong` changes nothing in the caller and `move_right`, which receives `&a`, does. This is the same rule you met in *Pointers for Beginners*; a struct is just a bigger value. For large structs, passing a pointer (often `const struct point *p` when the function only reads) also avoids copying many bytes.
- An array of structs is an ordinary array whose elements happen to be structs: `path[i].x`.
- `sizeof(struct point)` is 8 here: two 4-byte ints. A struct can be *larger* than the sum of its members because the compiler may add padding; the note *Structs, Alignment, Padding, Unions, Bit-fields and Endianness* explains why. Never assume the size; always write `sizeof`.

## enum: names for a fixed set of choices

Sometimes a variable can hold only one of a few things: a colour, a day of the week, a state such as `IDLE`, `RUNNING`, `DONE`. You could use the numbers 0, 1, 2, but then `if (state == 2)` means nothing to a reader. An `enum` gives each choice a name; the compiler numbers them from 0 unless you say otherwise (`enum { LOW = 10, HIGH = 20 }`).

## typedef: a shorter name for a type

`typedef` gives an existing type a new name. Writing `struct point` every time is tedious, so programs often write `typedef struct {...} Point;` and then use plain `Point`. The next program uses `typedef` for both an enum and a struct, and shows the most common use of an enum: choosing a branch with `switch`.

{{include c/essentials/enum1.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o enum1 enum1.c && ./enum1}}

Things worth noticing:

- Inside `color_name`, the `switch` handles every enum value. If you add `YELLOW` to the enum and forget the `case`, `gcc -Wall` warns "enumeration value not handled in switch". That warning is the main reason to prefer an enum over bare numbers.
- An enum value **is** an integer (`p.color` printed as `1`). C does not stop you from storing 42 in a `Color`, so an enum documents intent; it does not enforce it.
- `memcpy(&copy, &p, sizeof p)` copies the bytes of a whole struct. For plain structs like this it is equivalent to `copy = p;`, which is simpler. (`memcpy` is the general tool for copying raw bytes; *Strings and Buffer Safety* covers using it safely.)
- `typedef` does not create a new kind of thing; `Pixel` and `struct {...}` are the same type with a short name.

## A struct that points to a struct: your first linked list

A struct member may be a pointer, including a pointer to the **same kind** of struct. That single idea builds lists, trees and graphs, which the whole DSA track depends on.

{{include c/essentials/struct_list.c}}

{{run c/essentials :: gcc -std=c17 -Wall -Wextra -o struct_list struct_list.c && ./struct_list}}

Each `Node` holds a value and the address of the next node. The last node's `next` is `NULL`, the "nothing here" address, which is how the `for` loop knows to stop. Inside the struct you must write `struct node *next` rather than `Node *next`, because the name `Node` does not exist until the declaration ends. Every `malloc` needs a matching `free`; the `while` loop saves `head->next` *before* freeing, because reading a node after freeing it is one of the three classic pointer bugs.

## Pitfalls

- **Comparing structs with `==`** does not compile. Compare member by member (do not use `memcmp`, because padding bytes may differ).
- **Forgetting the semicolon after the closing brace** of a `struct` declaration produces confusing errors on the *next* line.
- **Using a struct before its type is declared.** The declaration must come above its first use, usually in a header file (see *Functions, Headers and Multi-File Programs*).
- **Reading uninitialised members.** `struct point p;` inside a function has garbage values; write `struct point p = {0};` to zero everything.
- **Treating an enum as checked.** Validate numbers that come from outside the program before storing them in an enum.

## Key takeaways

1. `struct` bundles named members into one type; `.` reads a member of a variable and `->` reads it through a pointer.
2. Assigning or passing a struct copies it; pass a pointer when the function must change the original or the struct is large.
3. `enum` gives readable names to a small fixed set of integers, and `switch` over an enum lets the compiler warn about missing cases.
4. `typedef` only renames a type. A struct that contains a pointer to its own kind is the foundation of linked lists and trees.
5. Use `= {0}` or designated initializers, never rely on uninitialised members, and always use `sizeof` instead of assuming sizes.
