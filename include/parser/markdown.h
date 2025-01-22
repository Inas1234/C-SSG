#ifndef MARKDOWN_H
#define MARKDOWN_H

#include <stddef.h>
#include "arena.h"

typedef struct {
    char* title;
} FrontMatter;

typedef struct {
    FrontMatter frontmatter;
    char* html;
} MarkdownDoc;

MarkdownDoc parse_markdown(Arena* arena, const char* input, size_t len);

#endif 

