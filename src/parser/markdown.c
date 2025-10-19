#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmark.h>
#include "parser/markdown.h"
#include "parser/mlindown_token.h"
#include "parser/mlindown_parser.h"
#include "parser/mlindown_render.h"
#include "utils/simd.h"
#include "arena.h"

#define FRONTMATTER_DELIMITER "---"

static int parse_frontmatter(char* content, FrontMatter* fm, Arena* arena) {
    char* start = simd_strstr(content, FRONTMATTER_DELIMITER);
    if (!start) return 0;
    
    char* end = simd_strstr(start + 3, FRONTMATTER_DELIMITER);
    if (!end) return 0;
    
    *end = '\0';
    char* yaml = start + 3;
    
    char* title_start = simd_strstr(yaml, "title:");
    if (title_start) {
        title_start += 6;
        while (*title_start == ' ' || *title_start == '"') title_start++;
        char* title_end = simd_strchr(title_start, '\n');
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

// MarkdownDoc parse_markdown(Arena* arena, const char* input, size_t len) {
//     MarkdownDoc doc = {0};
//     char* content = arena_alloc(arena, len + 1);
//     memcpy(content, input, len);
//     content[len] = '\0';

//     int frontmatter_size = parse_frontmatter(content, &doc.frontmatter, arena);
    
//     char* md_content = content + frontmatter_size;
//     cmark_node* node = cmark_parse_document(md_content, strlen(md_content), CMARK_OPT_DEFAULT);
    
//     doc.html = cmark_render_html(node, CMARK_OPT_DEFAULT);
//     cmark_node_free(node);
    
//     return doc;
// }

MarkdownDoc parse_markdown(Arena* arena, const char* input, size_t len) {
    MarkdownDoc doc = {0};
    char* content = arena_alloc(arena, len + 1);
    memcpy(content, input, len);
    content[len] = '\0';

    int frontmatter_size = parse_frontmatter(content, &doc.frontmatter, arena);
    char* md_content = content + frontmatter_size;

    TokenList toks = tokenize(md_content);
    Node *root = parse_tokens(&toks);

    char *html = render_html_str(root);
    if (html) {
        size_t hlen = strlen(html);
        doc.html = arena_alloc(arena, hlen + 1);
        memcpy(doc.html, html, hlen + 1);
        free(html);
    }

    node_free(root);
    free_tokens(&toks);

    return doc;
}