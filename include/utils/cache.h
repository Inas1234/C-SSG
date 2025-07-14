#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <time.h>

typedef struct {
    char* input_path;
    char* output_path;
    time_t last_modified;
    uint64_t content_hash;
} CacheEntry;

typedef struct {
    CacheEntry* entries;
    size_t count;
    size_t capacity;
} BuildCache;

typedef struct {
    size_t total_files;
    size_t built_files;
    size_t copied_files;
    double total_time;
} BuildMetrics;

int cache_save(const BuildCache* cache, const char* path);
int cache_load(BuildCache* cache, const char* path);

void cache_init(BuildCache* cache, size_t initial_capacity);
void cache_free(BuildCache* cache);
void cache_add_entry(BuildCache* cache, const char* in_path, 
                    const char* out_path, time_t mtime, uint64_t hash);

int cache_contains(const BuildCache* cache, const char* path);
uint64_t file_hash(const char* path);
void cache_purge_missing(BuildCache* cache);
void cache_update_entry(BuildCache* cache, const char* in_path,
                       const char* out_path, time_t mtime, uint64_t hash);

int needs_rebuild(const char* in_path, BuildCache* cache);
int needs_copy(const char* src, const char* dst);


CacheEntry* cache_get(BuildCache* cache, const char* input_path);

uint64_t file_hash(const char* path);
uint64_t hash_from_memory(const char* data, size_t size);


#endif
