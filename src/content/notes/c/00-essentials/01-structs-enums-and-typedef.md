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
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 6 (Structures)"
  - label: "King — C Programming, A Modern Approach, 2nd ed., Ch. 16 (Structures, Unions, and Enumerations)"
  - label: "ISO/IEC 9899:2018 §6.7.2.1 (Structure and union specifiers), §6.7.2.2 (Enumeration specifiers), §6.7.8 (Type definitions)"
  - label: "GCC manual — Designated Initializers"
    url: https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html
---

So far every variable held one value. Real programs deal with things that have several parts: a point has an x and a y, a student has a name and a mark, a network packet has a source and a destination. C lets you glue the parts into one **new type** with `struct`, and lets you give names to a fixed list of choices with `enum`. This note is the missing step between *C From Zero* and the later notes, which use structs on almost every page.

## A struct: several values that belong together

A `struct` declares a type made of named **members**. Declaring the type reserves no memory; creating a variable of that type does.

```c
#include <stdio.h>

struct point {            /* a new type: two ints that travel together */
    int x;
    int y;
};

static void move_wrong(struct point p) { p.x += 10; }          /* gets a COPY */
static void move_right(struct point *p) { p->x += 10; }        /* gets the address */

int main(void) {
    struct point a = {3, 4};                      /* members in declaration order */
    struct point b = {.y = 9, .x = 1};            /* designated initializers: by name */
    struct point c = a;                           /* assignment copies every member */

    c.x = 100;                                    /* '.' reaches a member of a struct variable */
    printf("a = (%d, %d)  b = (%d, %d)  c = (%d, %d)\n", a.x, a.y, b.x, b.y, c.x, c.y);

    move_wrong(a);
    printf("after move_wrong: a.x = %d\n", a.x);
    move_right(&a);                               /* '->' reaches a member through a pointer */
    printf("after move_right: a.x = %d\n", a.x);

    struct point path[3] = {{0, 0}, {1, 2}, {3, 4}};   /* arrays of structs work like any array */
    int total = 0;
    for (int i = 0; i < 3; i++) total += path[i].x + path[i].y;
    printf("sum of all coordinates = %d, sizeof(struct point) = %zu\n", total, sizeof(struct point));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o struct1 struct1.c && ./struct1
a = (3, 4)  b = (1, 9)  c = (100, 4)
after move_wrong: a.x = 3
after move_right: a.x = 13
sum of all coordinates = 10, sizeof(struct point) = 8
```

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

```c
#include <stdio.h>
#include <string.h>

typedef enum { RED, GREEN, BLUE } Color;        /* names for small whole numbers: RED=0, GREEN=1, BLUE=2 */

typedef struct {                                /* typedef: "Pixel" instead of "struct pixel" everywhere */
    int x, y;
    Color color;
} Pixel;

static const char *color_name(Color c) {
    switch (c) {                                /* the compiler warns if you forget an enum value */
        case RED:   return "red";
        case GREEN: return "green";
        case BLUE:  return "blue";
    }
    return "unknown";
}

int main(void) {
    Pixel p = {.x = 5, .y = 7, .color = GREEN};
    printf("pixel at (%d, %d) is %s (stored as the number %d)\n", p.x, p.y, color_name(p.color), p.color);

    Pixel screen[2] = {{0, 0, RED}, {1, 0, BLUE}};
    for (size_t i = 0; i < sizeof screen / sizeof screen[0]; i++)
        printf("screen[%zu] = %s\n", i, color_name(screen[i].color));

    Pixel copy;
    memcpy(&copy, &p, sizeof p);                /* a struct is plain bytes; memcpy copies all of them */
    copy.color = BLUE;
    printf("original is still %s, copy is %s\n", color_name(p.color), color_name(copy.color));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o enum1 enum1.c && ./enum1
pixel at (5, 7) is green (stored as the number 1)
screen[0] = red
screen[1] = blue
original is still green, copy is blue
```

Things worth noticing:

- Inside `color_name`, the `switch` handles every enum value. If you add `YELLOW` to the enum and forget the `case`, `gcc -Wall` warns "enumeration value not handled in switch". That warning is the main reason to prefer an enum over bare numbers.
- An enum value **is** an integer (`p.color` printed as `1`). C does not stop you from storing 42 in a `Color`, so an enum documents intent; it does not enforce it.
- `memcpy(&copy, &p, sizeof p)` copies the bytes of a whole struct. For plain structs like this it is equivalent to `copy = p;`, which is simpler. (`memcpy` is the general tool for copying raw bytes; *Strings and Buffer Safety* covers using it safely.)
- `typedef` does not create a new kind of thing; `Pixel` and `struct {...}` are the same type with a short name.

## A struct that points to a struct: your first linked list

A struct member may be a pointer, including a pointer to the **same kind** of struct. That single idea builds lists, trees and graphs, which the whole DSA track depends on.

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct node {                 /* a struct may point to another struct of its own kind */
    int value;
    struct node *next;                /* "struct node" is needed here: the typedef name is not ready yet */
} Node;

int main(void) {
    Node *head = NULL;
    for (int v = 3; v >= 1; v--) {    /* build 1 -> 2 -> 3 by inserting at the front */
        Node *n = malloc(sizeof *n);
        if (!n) return 1;
        n->value = v;
        n->next = head;
        head = n;
    }
    for (Node *p = head; p != NULL; p = p->next) printf("%d ", p->value);
    printf("\n");
    while (head) { Node *next = head->next; free(head); head = next; }   /* free every node */
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o struct_list struct_list.c && ./struct_list
1 2 3 
```

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
