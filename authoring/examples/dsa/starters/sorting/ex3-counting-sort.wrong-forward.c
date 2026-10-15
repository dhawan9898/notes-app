void counting_sort(const Person *in, Person *out, int n) {
    int count[MAXV + 1] = {0};
    for (int i = 0; i < n; i++) count[in[i].age]++;
    for (int v = 1; v <= MAXV; v++) count[v] += count[v - 1];
    for (int i = 0; i < n; i++) out[--count[in[i].age]] = in[i];    /* forwards: equal ages come out reversed */
}
