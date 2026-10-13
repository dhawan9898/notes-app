// Time and locale: struct tm, timegm vs mktime, strftime, and how setlocale changes number parsing.
#define _GNU_SOURCE
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    time_t t = 1700000000;                                          // a fixed instant: 2023-11-14 22:13:20 UTC
    char buf[64];
    struct tm utc; gmtime_r(&t, &utc);                              // thread-safe variant; gmtime() returns a shared static buffer
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &utc);
    printf("time_t %ld = %s\n", (long)t, buf);

    setenv("TZ", "Asia/Kolkata", 1); tzset();
    struct tm local; localtime_r(&t, &local);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %Z (%z)", &local);
    printf("same instant in Asia/Kolkata: %s\n", buf);
    printf("timegm(&utc) == t: %s;   mktime(&utc) == t: %s  (mktime interprets the fields as LOCAL time)\n", timegm(&utc) == t ? "yes" : "no", mktime(&utc) == t ? "yes" : "no");

    struct tm feb = {0}; feb.tm_year = 2024 - 1900; feb.tm_mon = 1; feb.tm_mday = 30; feb.tm_hour = 12;       // 30 February
    mktime(&feb); strftime(buf, sizeof buf, "%Y-%m-%d", &feb);
    printf("mktime normalizes 2024-02-30 to %s\n", buf);

    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); clock_gettime(CLOCK_MONOTONIC, &b);
    printf("CLOCK_MONOTONIC never goes backwards (diff >= 0: %s); CLOCK_REALTIME can jump when the clock is set\n", (b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec) >= 0 ? "yes" : "no");

    printf("strtod(\"3.14\") in the C locale: %g\n", strtod("3.14", NULL));
    if (setlocale(LC_NUMERIC, "de_DE.UTF-8")) printf("after setlocale(de_DE): strtod(\"3.14\") = %g   (the decimal separator is now a comma)\n", strtod("3.14", NULL));
    else printf("de_DE.UTF-8 locale is not installed here; with it, strtod(\"3.14\") would stop at the '.' and return 3\n");
    return 0;
}
