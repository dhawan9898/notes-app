---
kind: question
title: Stability
module: Searching and Sorting
difficulty: Hard
---

Sort the records `ana:2 bob:2 cy:1 dee:2 eli:1` (already in name order) by grade with selection sort and with insertion sort. Which preserves the name order among equal grades, and why does it matter?

## Explanation

```c
#include <stdio.h>
#include <string.h>

typedef struct { const char *name; int grade; } Student;

static void print(const char *label, const Student *s, int n) {
    printf("%-26s", label);
    for (int i = 0; i < n; i++) printf(" %s:%d", s[i].name, s[i].grade);
    printf("\n");
}

static void selection_by_grade(Student *s, int n) {            /* swaps over long distances: NOT stable */
    for (int i = 0; i < n - 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++) if (s[j].grade < s[m].grade) m = j;
        Student t = s[i]; s[i] = s[m]; s[m] = t;
    }
}
static void insertion_by_grade(Student *s, int n) {            /* moves items only past strictly larger ones: stable */
    for (int i = 1; i < n; i++) {
        Student key = s[i]; int j = i - 1;
        while (j >= 0 && s[j].grade > key.grade) { s[j + 1] = s[j]; j--; }
        s[j + 1] = key;
    }
}

int main(void) {
    const Student start[] = {{"ana", 2}, {"bob", 2}, {"cy", 1}, {"dee", 2}, {"eli", 1}};    /* already in name order */
    Student a[5], b[5];
    memcpy(a, start, sizeof start); memcpy(b, start, sizeof start);
    print("input (by name):", start, 5);
    selection_by_grade(a, 5); print("selection sort by grade:", a, 5);
    insertion_by_grade(b, 5); print("insertion sort by grade:", b, 5);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p3_stable p3_stable.c && ./p3_stable; rm -f p3_stable
input (by name):           ana:2 bob:2 cy:1 dee:2 eli:1
selection sort by grade:   cy:1 eli:1 ana:2 dee:2 bob:2
insertion sort by grade:   cy:1 eli:1 ana:2 bob:2 dee:2
```

A sort is **stable** if records with equal keys keep their original relative order. Insertion sort moves an item only past *strictly larger* ones, so `ana, bob, dee` stay in order. Selection sort swaps the smallest item with the one at position *i*, which can jump an item over equal ones: here the swap that brought `cy` to the front threw `ana` far to the right and finally put `dee` before `bob`. Stability matters when you sort by more than one key in passes ("sort by name, then by grade" should leave names sorted within each grade), for sorting records in user interfaces, and for radix sort, which *requires* stable passes. Merge sort and insertion sort are stable; selection sort, heapsort and typical quicksort are not (and `qsort` makes no guarantee). To make an unstable sort stable, add the original index as a tie-breaker.
