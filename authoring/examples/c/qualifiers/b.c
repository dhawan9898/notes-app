int counter;                      /* ANOTHER definition in b.c: legal in old GCC (-fcommon), an error since GCC 10 */
static int hidden = 100;          /* a different variable with the same name */
void bump(void) { counter++; hidden++; }
