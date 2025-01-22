#include "utils/cache.h"
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

int needs_rebuild(const char* in_path, const char* out_path, BuildCache* cache) {
    struct stat in_stat, out_stat;
    
    // Input file check
    if (stat(in_path, &in_stat) != 0) {
        fprintf(stderr, "Missing input file: %s\n", in_path);
        return 0;
    }

    // Output file existence check
    const int output_exists = (stat(out_path, &out_stat) == 0);

    // Check cache entry
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].input_path, in_path) == 0) {
            // Validate against current state
            const int content_changed = (cache->entries[i].content_hash != file_hash(in_path));
            const int output_missing = !output_exists;
            const int outdated = output_exists && (in_stat.st_mtime > out_stat.st_mtime);
            
            return content_changed || output_missing || outdated;
        }
    }

    // No cache entry - needs rebuild
    return 1;
}

int needs_copy(const char* src, const char* dst) {
    struct stat src_stat, dst_stat;
    return (stat(src, &src_stat) == 0) && 
           (stat(dst, &dst_stat) != 0 || 
            src_stat.st_mtime > dst_stat.st_mtime);
}

