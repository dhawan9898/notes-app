void counting_sort(const Person *in, Person *out, int n) {
    int count[MAXV + 1] = {0};
    for (int i = 0; i < n; i++) count[in[i].age]++;                 /* 1. histogram */
    for (int v = 1; v <= MAXV; v++) count[v] += count[v - 1];       /* 2. count[v] = how many ages are <= v */
    for (int i = n - 1; i >= 0; i--) out[--count[in[i].age]] = in[i];   /* 3. from the back: stable */
}
