#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; int *scores; int n; } Student;

static Student *student_new(const char *name, int n) {
    Student *s = malloc(sizeof *s);
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->scores = calloc((size_t)n, sizeof(int));
    s->n = n;
    return s;
}

static void student_free(Student *s) {
    free(s->name);                    /* BUG in the first version: forgot s->scores and s itself */
    free(s->scores);
    free(s);
}

int main(void) {
    Student *s = student_new("Ada", 3);
    s->scores[0] = 90;
    printf("%s has %d scores, first = %d\n", s->name, s->n, s->scores[0]);
    student_free(s);
    return 0;
}
