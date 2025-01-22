#include "utils/cache.h"
#include <sys/stat.h>
#include <time.h>
#include <string.h>

int needs_rebuild(const char* in_path, const char* out_path, const BuildCache cache) {
    struct stat in_stat, out_stat;
    
    if (stat(in_path, &in_stat) != 0) return 0;
    if (stat(out_path, &out_stat) != 0) return 1;
    
    // Check template dependency (add this later)
    // if (template_modified) return 1;
    
    for (size_t i = 0; i < cache.count; i++) {
        if (strcmp(cache.entries[i].input_path, in_path) == 0) {
            return (in_stat.st_mtime > cache.entries[i].last_modified) ||
                   (file_hash(in_path) != cache.entries[i].content_hash);
        }
    }
    return 1;
}

int needs_copy(const char* src, const char* dst) {
    struct stat src_stat, dst_stat;
    return (stat(src, &src_stat) == 0) && 
           (stat(dst, &dst_stat) != 0 || 
            src_stat.st_mtime > dst_stat.st_mtime);
}

