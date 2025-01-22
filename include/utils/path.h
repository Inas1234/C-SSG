#ifndef PATH_H
#define PATH_H

#include <sys/stat.h>

#define PATH_MAX 4096

const char* strip_extension(const char* filename);
const char* get_filename(const char* path);
void mkpath(const char* path, mode_t mode);
void create_directory(const char* path);
const char* dirname(const char* path);
void copy_directory_structure(const char* input_dir, const char* output_dir);

#endif
