#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmark.h>
#include "parser/markdown.h"
#include "arena.h"

#define FRONTMATTER_DELIMITER "---"

static int parse_frontmatter(char* content, FrontMatter* fm, Arena* arena) {
    char* start = strstr(content, FRONTMATTER_DELIMITER);
    if (!start) return 0;
    
    char* end = strstr(start + 3, FRONTMATTER_DELIMITER);
    if (!end) return 0;
    
    // Extract frontmatter block
    *end = '\0';
    char* yaml = start + 3;
    
    char* title_start = strstr(yaml, "title:");
    if (title_start) {
        title_start += 6;
        while (*title_start == ' ' || *title_start == '"') title_start++;
        char* title_end = strchr(title_start, '\n');
        if (title_end) {
            while (title_end > title_start && 
                (title_end[-1] == ' ' || title_end[-1] == '"')) {
                title_end--;
            }
            size_t len = title_end - title_start;
            fm->title = arena_alloc(arena, len + 1);
            strncpy(fm->title, title_start, len);
            fm->title[len] = '\0';
        }
    }    
    return end - content + 3;
}

MarkdownDoc parse_markdown(Arena* arena, const char* input, size_t len) {
    MarkdownDoc doc = {0};
    char* content = arena_alloc(arena, len + 1);
    memcpy(content, input, len);
    content[len] = '\0';

    // Parse frontmatter
    int frontmatter_size = parse_frontmatter(content, &doc.frontmatter, arena);
    
    // Process Markdown content
    char* md_content = content + frontmatter_size;
    cmark_node* node = cmark_parse_document(md_content, strlen(md_content), CMARK_OPT_DEFAULT);
    
    // Convert to HTML
    doc.html = cmark_render_html(node, CMARK_OPT_DEFAULT);
    cmark_node_free(node);
    
    return doc;
}
