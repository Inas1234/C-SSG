#include "utils/cache.h"
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int needs_rebuild(const char* in_path, BuildCache* cache) {
    CacheEntry* entry = NULL;

    // HASH_FIND_STR is the uthash macro for a fast (O(1) average) lookup.
    // It searches the hash table pointed to by `*cache` for a key matching `in_path`.
    // If found, it sets `entry` to point to the found struct.
    HASH_FIND_STR(*cache, in_path, entry);

    // Case 1: Not in cache. Must be a new file, so rebuild.
    if (!entry) {
        return 1;
    }

    // Case 2: In cache. Check modification time.
    struct stat st;
    if (stat(in_path, &st) != 0) {
        // Source file was deleted. Don't try to build it.
        return 0;
    }
    if (st.st_mtime > entry->last_modified) {
        return 1; // Source file is newer than our cache record. Rebuild.
    }

    // Case 3: Check if the output file was deleted manually.
    if (access(entry->output_path, F_OK) != 0) {
        return 1; // Output is missing. Rebuild.
    }

    // If all checks pass, the file is up-to-date.
    return 0;
}



int needs_copy(const char* src, const char* dst) {
    struct stat src_stat, dst_stat;
    if (stat(src, &src_stat) != 0) return 0;
    return (stat(dst, &dst_stat) != 0 || src_stat.st_mtime > dst_stat.st_mtime);
}