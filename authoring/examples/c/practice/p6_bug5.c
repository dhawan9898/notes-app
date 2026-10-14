#include <stdio.h>

static int find_index(const int *a, int n, int target) {
    for (int i = 0; i <= n; i++)
        if (a[i] == target) return i;
    return -1;
}

int main(void) {
    int data[4] = {3, 8, 15, 42};
    printf("index of 99: %d\n", find_index(data, 4, 99));
    return 0;
}
