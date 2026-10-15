int max_of(const int *v, int count) {
    int best = v[0];
    for (int i = 1; i < count; i++)
        if (v[i] > best) best = v[i];
    return best;
}
