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
