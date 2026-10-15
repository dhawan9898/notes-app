void student_free(Student *s) {
    if (!s) return;
    free(s->name);
    free(s->scores);
    free(s);            /* last: s->name and s->scores are read through s above */
}
