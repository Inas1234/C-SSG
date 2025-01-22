#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

typedef struct {
    char** items;
    size_t count;
    size_t capacity;
} FileVector;

void vec_init(FileVector* vec);
void vec_push(FileVector* vec, const char* item);
void vec_free(FileVector* vec);

#endif
