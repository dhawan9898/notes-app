static int by_start(const void *x, const void *y) {
    const Job *a = x, *b = y;
    return (a->start > b->start) - (a->start < b->start);
}

int max_jobs(Job *jobs, int n) {
    qsort(jobs, (size_t)n, sizeof jobs[0], by_start);    /* earliest start first: A (0-9) blocks everything */
    int count = 0, last_end = -2147483647 - 1;
    for (int i = 0; i < n; i++)
        if (jobs[i].start >= last_end) { last_end = jobs[i].end; count++; }
    return count;
}
