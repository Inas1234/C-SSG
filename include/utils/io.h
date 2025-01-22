#ifndef IO_H
#define IO_H

#include <stddef.h>

#define BATCH_SIZE 64

typedef struct {
    char* contents[BATCH_SIZE];
    const char* paths[BATCH_SIZE];
    size_t sizes[BATCH_SIZE];
    int count;
} WriteBatch;

void batch_add(WriteBatch* batch, const char* path, const char* content, size_t size);
void batch_flush(WriteBatch* batch);

#endif