#ifndef TABLE_H
#define TABLE_H

#include <stddef.h>

/* A word -> count table (separate chaining). The table owns copies of the words. */
typedef struct Table Table;

Table *table_new(size_t buckets);
int    table_add(Table *t, const char *word);              /* returns 0 on success, -1 if out of memory */
size_t table_size(const Table *t);                         /* number of distinct words */

typedef struct { const char *word; unsigned count; } Entry;
/* Copies all entries into a newly allocated array (size table_size). The words stay owned by the table. */
Entry *table_entries(const Table *t);

void   table_free(Table *t);

#endif
