#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <time.h>

#include "uthash.h"


typedef struct {
    char* input_path;     
    char* output_path;
    time_t last_modified;
    uint64_t content_hash;
    UT_hash_handle hh;    
} CacheEntry;


typedef CacheEntry* BuildCache;



typedef struct {
    size_t total_files;
    size_t built_files;
    size_t copied_files;
    double total_time;
} BuildMetrics;




int cache_save(const BuildCache* cache, const char* path);
int cache_load(BuildCache* cache, const char* path);
void cache_free(BuildCache* cache);
void cache_update_entry(BuildCache* cache, const char* in_path,
                       const char* out_path, time_t mtime, uint64_t hash);
void cache_purge_missing(BuildCache* cache);

int needs_rebuild(const char* in_path, BuildCache* cache);
int needs_copy(const char* src, const char* dst);

uint64_t file_hash(const char* path);
uint64_t hash_from_memory(const char* data, size_t size);


#endif // CACHE_H