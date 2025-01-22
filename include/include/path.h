#ifndef PATH_H
#define PATH_H

#include <libgen.h>
#include <string.h>

static inline const char* get_filename(const char* path) {
    const char* sep = strrchr(path, '/');
    return sep ? sep + 1 : path;
}

static inline void get_dirname(const char* path, char* output) {
    char* tmp = strdup(path);
    strcpy(output, dirname(tmp));
    free(tmp);
}

#endif