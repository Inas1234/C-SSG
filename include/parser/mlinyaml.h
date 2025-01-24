#ifndef MLINYAML_H
#define MLINYAML_H

#include <stddef.h>

typedef struct {
    const char* input_dir;
    const char* output_dir;
    const char* tmpl;
} YamlConfig;

int parse_yaml(const char* filename, YamlConfig* config);

#endif
