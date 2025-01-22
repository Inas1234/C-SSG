#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

#define ARENA_ALIGNMENT 64

typedef struct ArenaBlock ArenaBlock;

struct ArenaBlock {
    ArenaBlock* next;
    size_t capacity;
    size_t used;
    alignas(ARENA_ALIGNMENT) char data[];
};

typedef struct {
    ArenaBlock* current;
    ArenaBlock* head;
    size_t default_block_size;
} Arena;

void arena_init(Arena* arena, size_t initial_size);
void arena_free(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);
void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment);
void arena_reset(Arena* arena);

#define ARENA_ALLOC(arena, type) (type*)arena_alloc((arena), sizeof(type))
#define ARENA_ALLOC_ARRAY(arena, type, count) (type*)arena_alloc((arena), sizeof(type) * (count))

#endif // ARENA_H
