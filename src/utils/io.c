#include "utils/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <fcntl.h>
#include <unistd.h>


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
    for (int i = 0; i < batch->count; i++) {
        int fd = open(batch->paths[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1) {
            write(fd, batch->contents[i], batch->sizes[i]);
            close(fd);
        } else {
            fprintf(stderr, "Failed to open file for writing: %s\n", batch->paths[i]);
        }
        
    }
    batch->count = 0;
}
