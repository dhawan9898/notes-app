#include <stdio.h>
#if defined(__has_include)
#  if __has_include(<threads.h>)
#    include <threads.h>
#    define HAVE_THREADS_H 1
#  endif
#endif
#ifndef HAVE_THREADS_H
#  define HAVE_THREADS_H 0
#endif

#if defined(__linux__)
#  define OS "linux"
#elif defined(__APPLE__)
#  define OS "macos"
#else
#  define OS "other"
#endif

#if __STDC_VERSION__ >= 202000L        /* GCC < 15 reports 202000L for -std=c2x/c23 */
#  define STD "C23"
#elif __STDC_VERSION__ >= 201710L
#  define STD "C17"
#elif __STDC_VERSION__ >= 201112L
#  define STD "C11"
#else
#  define STD "older"
#endif

#ifndef BUILD_ID
#  define BUILD_ID "dev"
#endif

int main(void) {
    printf("os=%s std=%s have_threads_h=%d build=%s gcc=%d.%d\n", OS, STD, HAVE_THREADS_H, BUILD_ID, __GNUC__, __GNUC_MINOR__);
    printf("compiled %s %s from %s line %d\n", __DATE__, "", __FILE__, __LINE__);
    return 0;
}
