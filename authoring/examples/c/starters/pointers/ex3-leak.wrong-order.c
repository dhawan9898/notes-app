void student_free(Student *s) {
    free(s->name);
    free(s);                        /* forgets s->scores */
}
