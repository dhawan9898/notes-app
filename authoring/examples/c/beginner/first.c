#include <stdio.h>

int main(void) {
    int apples = 3;                 /* a whole number */
    double price = 0.5;             /* a number with a fractional part */
    char grade = 'A';               /* a single character */

    double total = apples * price;  /* an expression: the compiler works out the type */
    printf("I bought %d apples at %.2f each, total %.2f, grade %c\n", apples, price, total, grade);

    printf("sizes in bytes: int=%zu double=%zu char=%zu\n", sizeof(int), sizeof(double), sizeof(char));
    printf("7 / 2 = %d, 7 %% 2 = %d, 7 / 2.0 = %.1f\n", 7 / 2, 7 % 2, 7 / 2.0);   /* integer division drops the fraction */
    return 0;
}
