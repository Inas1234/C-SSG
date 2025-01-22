#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef ARENA_BLOCK_OVERHEAD
#define ARENA_BLOCK_OVERHEAD (offsetof(ArenaBlock, data))
#endif

static ArenaBlock* create_block(size_t capacity) {
    if (capacity < ARENA_ALIGNMENT) {
        capacity = ARENA_ALIGNMENT;
    }
    
    ArenaBlock* block = malloc(ARENA_BLOCK_OVERHEAD + capacity);
    if (!block) return NULL;
    
    block->capacity = capacity;
    block->used = 0;
    block->next = NULL;
    return block;
}

void arena_init(Arena* arena, size_t initial_size) {
    assert(initial_size > 0);
    ArenaBlock* block = create_block(initial_size);
    arena->head = arena->current = block;
    arena->default_block_size = initial_size;
}

void arena_free(Arena* arena) {
    ArenaBlock* block = arena->head;
    while (block) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    memset(arena, 0, sizeof(*arena));
}

static inline size_t align_forward(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    if (size == 0) return NULL;
    
    ArenaBlock* block = arena->current;
    size_t aligned_size = align_forward(size, alignment);
    
    if (block->used + aligned_size > block->capacity) {
        // Check if we need a new block
        size_t new_size = (aligned_size > arena->default_block_size) 
            ? aligned_size * 2 
            : arena->default_block_size;
        
        ArenaBlock* new_block = create_block(new_size);
        if (!new_block) return NULL;
        
        block->next = new_block;
        arena->current = new_block;
        block = new_block;
    }
    
    void* ptr = block->data + block->used;
    block->used += aligned_size;
    return ptr;
}

void* arena_alloc(Arena* arena, size_t size) {
    return arena_alloc_aligned(arena, size, ARENA_ALIGNMENT);
}

void arena_reset(Arena* arena) {
    ArenaBlock* block = arena->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    arena->current = arena->head;
}
