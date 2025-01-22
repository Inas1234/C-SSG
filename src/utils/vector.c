#include "utils/vector.h"
#include <stdlib.h>
#include <string.h>

void vec_init(FileVector* vec) {
    vec->items = malloc(sizeof(char*) * 128);
    vec->count = 0;
    vec->capacity = 128;
}

void vec_push(FileVector* vec, const char* item) {
    if (vec->count >= vec->capacity) {
        vec->capacity *= 2;
        vec->items = realloc(vec->items, sizeof(char*) * vec->capacity);
    }
    vec->items[vec->count++] = strdup(item);
}

void vec_free(FileVector* vec) {
    for (size_t i = 0; i < vec->count; i++) {
        free(vec->items[i]);
    }
    free(vec->items);
    vec->count = vec->capacity = 0;
}
