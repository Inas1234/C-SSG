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


/* =============================================================================
 *              High-Performance Check & Management Functions
 * =============================================================================
 */

/**
 * @brief Checks if a file needs to be rebuilt using a hash table lookup.
 * This is the core of the high-performance incremental build. It is O(1) on average.
 */

/**
 * @brief Adds a new entry or updates an existing one in the cache hash table.
 */
void cache_update_entry(BuildCache* cache, const char* in_path,
                       const char* out_path, time_t mtime, uint64_t hash) {
    CacheEntry* entry = NULL;
    HASH_FIND_STR(*cache, in_path, entry);

    if (entry) {
        // Entry already exists. Update its values in place.
        free(entry->output_path); // Free the old output path string.
        entry->output_path = strdup(out_path);
        entry->last_modified = mtime;
        entry->content_hash = hash;
    } else {
        // Entry does not exist. Allocate a new one and add it to the hash.
        entry = malloc(sizeof(CacheEntry));
        entry->input_path = strdup(in_path);
        entry->output_path = strdup(out_path);
        entry->last_modified = mtime;
        entry->content_hash = hash;

        // HASH_ADD_STR adds the new `entry` to the hash table `*cache`,
        // using the `input_path` field as the key.
        HASH_ADD_STR(*cache, input_path, entry);
    }
}

/**
 * @brief Frees all memory associated with the cache hash table.
 */
void cache_free(BuildCache* cache) {
    CacheEntry *current_entry, *tmp;

    // HASH_ITER is the safe iteration macro. It lets you delete while iterating.
    HASH_ITER(hh, *cache, current_entry, tmp) {
        HASH_DEL(*cache, current_entry); // Remove entry from the hash table.
        free(current_entry->input_path);
        free(current_entry->output_path);
        free(current_entry); // Free the struct itself.
    }
}

/**
 * @brief Removes entries from the cache if their source file no longer exists.
 */
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


/* =============================================================================
 *                      Cache Serialization (Load/Save)
 * =============================================================================
 */

int cache_save(const BuildCache* cache, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    // Write header
    fwrite(&CACHE_MAGIC, sizeof(CACHE_MAGIC), 1, f);
    const uint64_t count = HASH_COUNT(*cache); // HASH_COUNT gets table size.
    fwrite(&count, sizeof(count), 1, f);

    // Write entries by iterating through the hash table
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

        // Use our standard update function to build the hash table.
        cache_update_entry(cache, in_buf, out_buf, mtime, hash);
    }

    fclose(f);
    return 1;

error:
    cache_free(cache); // Clean up partially built hash table on error.
    fclose(f);
    return 0;
}



uint64_t file_hash(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    uint64_t hash = 0xcbf29ce484222325; // FNV_offset_basis
    const uint64_t prime = 0x100000001b3; // FNV_prime

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
    uint64_t hash = 0xcbf29ce484222325; // FNV_offset_basis
    const uint64_t prime = 0x100000001b3; // FNV_prime

    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= prime;
    }
    return hash;
}