#include "utils/path.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

const char* strip_extension(const char* filename) {
    char* copy = strdup(filename);
    char* dot = strrchr(copy, '.');
    if (dot) *dot = '\0';
    return copy;
}

const char* get_filename(const char* path) {
    const char* sep = strrchr(path, '/');
    return sep ? sep + 1 : path;
}

void create_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

void mkpath(const char* path, mode_t mode) {
    char* path_copy = strdup(path);
    char* p = path_copy;
    
    while (*p != '\0') {
        if (*p == '/') {
            *p = '\0';
            mkdir(path_copy, mode);
            *p = '/';
        }
        p++;
    }
    free(path_copy);
}

const char* dirname(const char* path) {
    char* copy = strdup(path);
    char* last_slash = strrchr(copy, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        copy[0] = '.';
        copy[1] = '\0';
    }
    return copy;
}

void copy_directory_structure(const char* input_dir, const char* output_dir) {
    DIR* dir = opendir(input_dir);
    if (!dir) return;

    // Create output directory if it doesn't exist
    create_directory(output_dir);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char in_path[PATH_MAX];
        char out_path[PATH_MAX];
        snprintf(in_path, sizeof(in_path), "%s/%s", input_dir, entry->d_name);
        snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, entry->d_name);

        struct stat st;
        if (lstat(in_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            create_directory(out_path);
            copy_directory_structure(in_path, out_path);
        }
    }
    closedir(dir);
}
