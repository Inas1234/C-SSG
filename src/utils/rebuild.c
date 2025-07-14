#include "utils/cache.h"
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int needs_rebuild(const char* input_path, BuildCache* cache) {
    // 1. Look up the file in the cache.
    CacheEntry* entry = cache_get(cache, input_path);

    // 2. Cache Miss: If it's not in the cache, it's new. Rebuild.
    if (!entry) {
        return 1;
    }

    // 3. Cache Hit: Get the file's current modification time.
    struct stat st;
    if (stat(input_path, &st) != 0) {
        // The file existed before but is now deleted. Don't build.
        // The cache_purge_missing() will clean this up later.
        return 0;
    }

    // 4. Compare modification times. If the file on disk is newer, rebuild.
    if (st.st_mtime > entry->last_modified) {
        return 1;
    }

    // 5. Check if the output file was deleted manually.
    if (access(entry->output_path, F_OK) != 0) {
        return 1; // Output is missing, rebuild.
    }

    // If we get here, the file is in the cache and is not outdated. Skip it.
    return 0;
}

int needs_copy(const char* src, const char* dst) {
    struct stat src_stat, dst_stat;
    return (stat(src, &src_stat) == 0) && 
           (stat(dst, &dst_stat) != 0 || 
            src_stat.st_mtime > dst_stat.st_mtime);
}

