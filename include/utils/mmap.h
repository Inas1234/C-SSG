#ifndef MMAP_H
#define MMAP_H

#include <stddef.h>

typedef struct {
    const char* data;
    size_t size;
} MappedFile;

MappedFile mmap_file(const char* path);
void munmap_file(MappedFile mf);

#endif