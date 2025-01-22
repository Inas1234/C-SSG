#include "utils/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

void batch_add(WriteBatch* batch, const char* path, const char* content, size_t size) {
    if (batch->count >= BATCH_SIZE) {
        batch_flush(batch);
    }
    
    // Store persistent copies
    batch->paths[batch->count] = strdup(path);
    batch->contents[batch->count] = malloc(size);
    memcpy(batch->contents[batch->count], content, size);
    batch->sizes[batch->count] = size;
    batch->count++;
}

void batch_flush(WriteBatch* batch) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < batch->count; i++) {
        FILE* f = fopen(batch->paths[i], "w");
        if (f) {
            fwrite(batch->contents[i], 1, batch->sizes[i], f);
            fclose(f);
        }
        free((void*)batch->paths[i]);
        free(batch->contents[i]);
    }
    batch->count = 0;
}