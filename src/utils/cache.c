#include "utils/cache.h"
#include "utils/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h> // For access()

/* =============================================================================
 *                      Binary Cache File Format
 * =============================================================================
 *
 * This implementation uses a simple and robust binary format for speed and
 * to avoid parsing complexities.
 *
 * [Header]
 * 8 bytes: magic number 0x5353474341434543 ("SSGCACHE")
 * 8 bytes: number of entries (uint64_t)
 *
 * [Entries] (repeated for each entry)
 * 8 bytes: length of input_path string (including null terminator)
 * N bytes: input_path string
 * 8 bytes: length of output_path string (including null terminator)
 * N bytes: output_path string
 * 8 bytes: last_modified timestamp (time_t)
 * 8 bytes: content_hash (uint64_t)
 *
 */
static const uint64_t CACHE_MAGIC = 0x5353474341434543; // "SSGCACHE"


void cache_update_entry(BuildCache* cache, const char* in_path,
                       const char* out_path, time_t mtime, uint64_t hash) {
    CacheEntry* entry = NULL;
    HASH_FIND_STR(*cache, in_path, entry);

    if (entry) {
        free(entry->output_path);
        entry->output_path = strdup(out_path);
        entry->last_modified = mtime;
        entry->content_hash = hash;
    } else {
        entry = malloc(sizeof(CacheEntry));
        entry->input_path = strdup(in_path);
        entry->output_path = strdup(out_path);
        entry->last_modified = mtime;
        entry->content_hash = hash;

        HASH_ADD_STR(*cache, input_path, entry);
    }
}

void cache_free(BuildCache* cache) {
    CacheEntry *current_entry, *tmp;

    HASH_ITER(hh, *cache, current_entry, tmp) {
        HASH_DEL(*cache, current_entry); 
        free(current_entry->input_path);
        free(current_entry->output_path);
        free(current_entry);
    }
}

void cache_purge_missing(BuildCache* cache) {
    CacheEntry *current_entry, *tmp;
    HASH_ITER(hh, *cache, current_entry, tmp) {
        if (access(current_entry->input_path, F_OK) != 0) {
            HASH_DEL(*cache, current_entry);
            free(current_entry->input_path);
            free(current_entry->output_path);
            free(current_entry);
        }
    }
}

int cache_save(const BuildCache* cache, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    fwrite(&CACHE_MAGIC, sizeof(CACHE_MAGIC), 1, f);
    const uint64_t count = HASH_COUNT(*cache); 
    fwrite(&count, sizeof(count), 1, f);

    CacheEntry *entry, *tmp;
    HASH_ITER(hh, *cache, entry, tmp) {
        const uint64_t in_len = strlen(entry->input_path) + 1;
        const uint64_t out_len = strlen(entry->output_path) + 1;

        fwrite(&in_len, sizeof(in_len), 1, f);
        fwrite(entry->input_path, 1, in_len, f);

        fwrite(&out_len, sizeof(out_len), 1, f);
        fwrite(entry->output_path, 1, out_len, f);

        fwrite(&entry->last_modified, sizeof(entry->last_modified), 1, f);
        fwrite(&entry->content_hash, sizeof(entry->content_hash), 1, f);
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

    for (uint64_t i = 0; i < count; i++) {
        uint64_t in_len, out_len;
        char in_buf[PATH_MAX], out_buf[PATH_MAX];
        time_t mtime;
        uint64_t hash;

        if (fread(&in_len, sizeof(in_len), 1, f) != 1) goto error;
        if (in_len > PATH_MAX || fread(in_buf, 1, in_len, f) != in_len) goto error;

        if (fread(&out_len, sizeof(out_len), 1, f) != 1) goto error;
        if (out_len > PATH_MAX || fread(out_buf, 1, out_len, f) != out_len) goto error;

        if (fread(&mtime, sizeof(mtime), 1, f) != 1) goto error;
        if (fread(&hash, sizeof(hash), 1, f) != 1) goto error;

        cache_update_entry(cache, in_buf, out_buf, mtime, hash);
    }

    fclose(f);
    return 1;

error:
    cache_free(cache);
    fclose(f);
    return 0;
}



uint64_t file_hash(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    uint64_t hash = 0xcbf29ce484222325; 
    const uint64_t prime = 0x100000001b3; 

    unsigned char buf[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), f))) {
        for (size_t i = 0; i < bytes_read; i++) {
            hash ^= buf[i];
            hash *= prime;
        }
    }
    fclose(f);
    return hash;
}

uint64_t hash_from_memory(const char* data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325; 
    const uint64_t prime = 0x100000001b3; 

    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= prime;
    }
    return hash;
}