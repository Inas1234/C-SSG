#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "arena.h"
#include "parser/markdown.h"

#define TEMPLATE_PATH "templates/default.html"

char* render_template(Arena* arena, const char* template, const FrontMatter* fm, const char* content) {
    // Replace title
    char* current = strstr(template, "{{title}}");
    if (!current) return NULL;
    
    size_t before_title = current - template;
    size_t title_len = fm->title ? strlen(fm->title) : 0;
    
    // Replace content
    char* content_placeholder = strstr(current + 9, "{{content}}"); // 8 = strlen("{{title}}")
    if (!content_placeholder) return NULL;
    
    size_t between_len = content_placeholder - (current + 9);
    size_t content_len = content ? strlen(content) : 0;
    size_t after_content = strlen(content_placeholder + 11); // 10 = strlen("{{content}}")

    // Calculate total size
    size_t total_size = before_title + title_len + between_len + content_len + after_content + 1;
    char* output = arena_alloc(arena, total_size);

    // Build output
    char* ptr = output;
    
    // Before title
    memcpy(ptr, template, before_title);
    ptr += before_title;
    
    // Title
    if (fm->title) {
        memcpy(ptr, fm->title, title_len);
        ptr += title_len;
    }
    
    // Between placeholders
    memcpy(ptr, current + 9, between_len);
    ptr += between_len;
    
    // Content
    if (content) {
        memcpy(ptr, content, content_len);
        ptr += content_len;
    }
    
    // After content
    memcpy(ptr, content_placeholder + 11, after_content);
    
    return output;
}


static char* read_entire_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    
    fclose(f);
    *len = size;
    return buffer;
}

static void process_file(Arena* arena, const char* input_path, const char* output_dir) {
    size_t md_len;
    char* md_content = read_entire_file(input_path, &md_len);
    if (!md_content) {
        fprintf(stderr, "Error reading %s\n", input_path);
        return;
    }

    MarkdownDoc doc = parse_markdown(arena, md_content, md_len);
    free(md_content);

    size_t template_len;
    char* template = read_entire_file(TEMPLATE_PATH, &template_len);
    if (!template) {
        fprintf(stderr, "Template not found: %s\n", TEMPLATE_PATH);
        return;
    }

    char* html = render_template(arena, template, &doc.frontmatter, doc.html);
    free(template);

    char output_path[256];
    snprintf(output_path, sizeof(output_path), "%s/output.html", output_dir);
    
    FILE* f = fopen(output_path, "w");
    if (f) {
        fputs(html, f);
        fclose(f);
    }
    
    arena_reset(arena);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <input-dir> <output-dir>\n", argv[0]);
        return 1;
    }

    Arena arena;
    arena_init(&arena, 1024 * 1024); 

    process_file(&arena, argv[1], argv[2]);

    arena_free(&arena);
    return 0;
}
