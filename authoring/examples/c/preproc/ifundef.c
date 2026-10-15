// A misspelled macro name in #if is silently 0: the feature quietly disappears.
#include <stdio.h>
#define FEATURE_LOGGING 1

int main(void) {
#if FEATURE_LOGING                 /* typo: one G missing */
    puts("logging enabled");
#else
    puts("logging disabled");
#endif
    return 0;
}
