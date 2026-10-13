// A small C library designed to be called from other languages: opaque handle, plain structs, error codes,
// a callback with a user-data pointer, and explicit ownership rules for anything it allocates.
#ifndef ACC_H
#define ACC_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {                                   // C++ callers: do not mangle these names
#endif

#define ACC_API __attribute__((visibility("default")))

typedef struct acc acc_t;                      // opaque: callers never see the layout, so it can change without breaking the ABI

enum { ACC_OK = 0, ACC_ERR_NOMEM = -1, ACC_ERR_EMPTY = -2, ACC_ERR_ARG = -3 };

typedef struct {                               // plain data, fixed layout: safe to mirror in other languages
    double min, max, mean;
    size_t n;
} acc_stats;

ACC_API acc_t *acc_new(void);                                   // caller owns the result: release with acc_free
ACC_API void   acc_free(acc_t *a);                              // accepts NULL
ACC_API int    acc_add(acc_t *a, double x);                     // returns ACC_OK or a negative error
ACC_API int    acc_stats_get(const acc_t *a, acc_stats *out);   // out-parameter filled on success
ACC_API int    acc_foreach(const acc_t *a, void (*cb)(double value, void *user), void *user);   // callback + opaque user pointer
ACC_API char  *acc_describe(const acc_t *a);                    // returns malloc'd text: release with acc_string_free, not free()
ACC_API void   acc_string_free(char *s);
ACC_API const char *acc_strerror(int code);                     // static string, never freed

#ifdef __cplusplus
}
#endif
#endif
