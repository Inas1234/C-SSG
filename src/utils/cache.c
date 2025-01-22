#include "utils/cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// File format:
// [Header]
// 8 bytes: magic number 0xSS47CACE
// 8 bytes: entry count
// 
// [Entries]
// 8 bytes: path length
// N bytes: path string
// 8 bytes: output path length
// N bytes: output path string
// 8 bytes: last modified time
// 8 bytes: content hash

static const uint64_t CACHE_MAGIC = 0x5353474341434543; // "SSGCACHE"

void cache_init(BuildCache* cache, size_t initial_capacity) {
    cache->entries = malloc(initial_capacity * sizeof(CacheEntry));
    cache->count = 0;
    cache->capacity = initial_capacity;
}

void cache_free(BuildCache* cache) {
    for (size_t i = 0; i < cache->count; i++) {
        free(cache->entries[i].input_path);
        free(cache->entries[i].output_path);
    }
    free(cache->entries);
    memset(cache, 0, sizeof(*cache));
}

int cache_save(const BuildCache* cache, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    // Write header
    fwrite(&CACHE_MAGIC, sizeof(CACHE_MAGIC), 1, f);
    const uint64_t count = cache->count;
    fwrite(&count, sizeof(count), 1, f);

    // Write entries
    for (size_t i = 0; i < cache->count; i++) {
        const CacheEntry* e = &cache->entries[i];
        const uint64_t in_len = strlen(e->input_path) + 1;
        const uint64_t out_len = strlen(e->output_path) + 1;

        fwrite(&in_len, sizeof(in_len), 1, f);
        fwrite(e->input_path, 1, in_len, f);
        
        fwrite(&out_len, sizeof(out_len), 1, f);
        fwrite(e->output_path, 1, out_len, f);
        
        fwrite(&e->last_modified, sizeof(e->last_modified), 1, f);
        fwrite(&e->content_hash, sizeof(e->content_hash), 1, f);
    }

    fclose(f);
    return 1;
}

int cache_load(BuildCache* cache, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    uint64_t magic;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != CACHE_MAGIC) {
        fclose(f);
        return 0;
    }

    uint64_t count;
    if (fread(&count, sizeof(count), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    cache->entries = realloc(cache->entries, count * sizeof(CacheEntry));
    cache->capacity = count;
    
    for (uint64_t i = 0; i < count; i++) {
        CacheEntry* e = &cache->entries[i];
        uint64_t in_len, out_len;

        // Read input path
        if (fread(&in_len, sizeof(in_len), 1, f) != 1) goto error;
        e->input_path = malloc(in_len);
        if (fread(e->input_path, 1, in_len, f) != in_len) goto error;
        
        // Read output path
        if (fread(&out_len, sizeof(out_len), 1, f) != 1) goto error;
        e->output_path = malloc(out_len);
        if (fread(e->output_path, 1, out_len, f) != out_len) goto error;
        
        // Read metadata
        if (fread(&e->last_modified, sizeof(e->last_modified), 1, f) != 1) goto error;
        if (fread(&e->content_hash, sizeof(e->content_hash), 1, f) != 1) goto error;
    }

    cache->count = count;
    fclose(f);
    return 1;

error:
    cache_free(cache);
    fclose(f);
    return 0;
}

void cache_add_entry(BuildCache* cache, const char* in_path, 
                    const char* out_path, time_t mtime, uint64_t hash) {
    if (cache->count >= cache->capacity) {
        cache->capacity *= 2;
        cache->entries = realloc(cache->entries, cache->capacity * sizeof(CacheEntry));
    }

    CacheEntry* e = &cache->entries[cache->count++];
    e->input_path = strdup(in_path);
    e->output_path = strdup(out_path);
    e->last_modified = mtime;
    e->content_hash = hash;
}

int cache_contains(const BuildCache* cache, const char* path) {
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].input_path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

uint64_t file_hash(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    
    return (st.st_size << 32) | (st.st_mtime & 0xFFFFFFFF);
}


// Add these functions after cache_load
void cache_purge_missing(BuildCache* cache) {
    size_t new_count = 0;
    for (size_t i = 0; i < cache->count; i++) {
        struct stat st;
        if (stat(cache->entries[i].input_path, &st) == 0) {
            cache->entries[new_count++] = cache->entries[i];
        } else {
            free(cache->entries[i].input_path);
            free(cache->entries[i].output_path);
        }
    }
    cache->count = new_count;
}

void cache_update_entry(BuildCache* cache, const char* in_path,
                       const char* out_path, time_t mtime, uint64_t hash) {
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].input_path, in_path) == 0) {
            free(cache->entries[i].output_path);
            cache->entries[i].output_path = strdup(out_path);
            cache->entries[i].last_modified = mtime;
            cache->entries[i].content_hash = hash;
            return;
        }
    }
    cache_add_entry(cache, in_path, out_path, mtime, hash);
}
