#include <stdio.h>

/* A function: a named, reusable piece of code that takes inputs and returns a result. */
static int square(int x) { return x * x; }

static int sum_array(const int values[], int count) {
    int total = 0;
    for (int i = 0; i < count; i++) {          /* i = 0, 1, 2, ... count-1 */
        total += values[i];
    }
    return total;
}

int main(void) {
    for (int n = 1; n <= 15; n++) {
        if (n % 15 == 0)      printf("FizzBuzz ");
        else if (n % 3 == 0)  printf("Fizz ");
        else if (n % 5 == 0)  printf("Buzz ");
        else                  printf("%d ", n);
    }
    printf("\n");

    int scores[] = { 90, 72, 85, 60 };
    int count = (int)(sizeof scores / sizeof scores[0]);     /* number of elements = total bytes / bytes per element */
    printf("sum = %d, average = %.1f, square(9) = %d\n", sum_array(scores, count), (double)sum_array(scores, count) / count, square(9));

    int countdown = 3;
    while (countdown > 0) { printf("%d... ", countdown); countdown--; }
    printf("liftoff\n");

    char name[] = "Ada";                       /* a string is an array of chars ending with a zero byte */
    printf("%s has %zu letters; first letter %c; bytes in the array: %zu\n", name, sizeof name - 1, name[0], sizeof name);
    return 0;
}
