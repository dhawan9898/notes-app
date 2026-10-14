#include <stdio.h>

static const char *day_kind(int d) {          /* 1 = Monday ... 7 = Sunday */
    switch (d) {
        case 6:
        case 7:                               /* two labels, one body: weekend */
            return "weekend";
        case 1: case 2: case 3: case 4: case 5:
            return "weekday";
        default:                              /* anything else */
            return "not a day";
    }
}

int main(void) {
    for (int d = 0; d <= 7; d += 3) printf("day %d is %s\n", d, day_kind(d));

    int n = 0, sum = 0;
    do {                                      /* body runs first, condition is checked after */
        n++;
        if (n % 2 == 0) continue;             /* skip the rest of this round */
        if (n > 9) break;                     /* leave the loop completely */
        sum += n;
    } while (n < 100);
    printf("sum of odd numbers up to 9 = %d (loop stopped at n = %d)\n", sum, n);

    int grade = 2;
    switch (grade) {                          /* the classic bug: no break means "fall through" */
        case 1: printf("one ");
        case 2: printf("two ");
        case 3: printf("three ");
        default: printf("(fell through to the end)\n");
    }
    return 0;
}
