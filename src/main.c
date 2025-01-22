#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "arena.h"
#include "parser/markdown.h"

#define TEMPLATE_PATH "templates/default.html"
#define PATH_MAX 4096


static void process_directory(Arena* arena, const char* input_dir, const char* output_dir);
static void process_entry(Arena* arena, const char* base_in, const char* base_out, const char* entry_name);
static void process_file(Arena* arena, const char* input_path, const char* output_dir);


const char* strip_extension(const char* filename) {
    char* dot = strrchr(filename, '.');
    if (!dot) return filename;
    *dot = '\0';
    return filename;
}

void copy_file(const char* src, const char* dst) {
    int in_fd = open(src, O_RDONLY);
    if (in_fd < 0) return;

    int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        close(in_fd);
        return;
    }

    char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(in_fd, buf, sizeof(buf))) > 0) {
        write(out_fd, buf, bytes_read);
    }

    close(in_fd);
    close(out_fd);
}

static void create_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

static void process_entry(Arena* arena, const char* base_in, const char* base_out, const char* entry_name) {
    char in_path[PATH_MAX];
    char out_path[PATH_MAX];
    
    snprintf(in_path, sizeof(in_path), "%s/%s", base_in, entry_name);
    snprintf(out_path, sizeof(out_path), "%s/%s", base_out, entry_name);

    struct stat st;
    if (lstat(in_path, &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        process_directory(arena, in_path, out_path);
    } else if (S_ISREG(st.st_mode)) {
        const char* ext = strrchr(entry_name, '.');
        if (ext && strcmp(ext, ".md") == 0) {
            char out_file[PATH_MAX];
            snprintf(out_file, sizeof(out_file), "%s/%s.html", 
                    base_out, strip_extension(entry_name));
            
            process_file(arena, in_path, out_file);
        } else {
            // Copy non-Markdown files
            copy_file(in_path, out_path);
        }
    }
}

static void process_directory(Arena* arena, const char* input_dir, const char* output_dir) {
    create_directory(output_dir);
    
    DIR* dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Error opening directory: %s\n", input_dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // Skip hidden files
        
        process_entry(arena, input_dir, output_dir, entry->d_name);
    }
    
    closedir(dir);
}



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

static void process_file(Arena* arena, const char* input_path, const char* output_path) {
    // Read and parse Markdown
    size_t md_len;
    char* md_content = read_entire_file(input_path, &md_len);
    if (!md_content) {
        fprintf(stderr, "Error reading %s\n", input_path);
        return;
    }

    MarkdownDoc doc = parse_markdown(arena, md_content, md_len);
    free(md_content);

    // Read template
    size_t template_len;
    char* template = read_entire_file(TEMPLATE_PATH, &template_len);
    if (!template) {
        fprintf(stderr, "Template not found: %s\n", TEMPLATE_PATH);
        return;
    }

    // Render HTML
    char* html = render_template(arena, template, &doc.frontmatter, doc.html);
    free(template);

    // Create output directory
    char* dir = strdup(output_path);
    char* parent_dir = dirname(dir);
    create_directory(parent_dir);
    free(dir);

    // Write file
    FILE* f = fopen(output_path, "w");
    if (f) {
        fputs(html, f);
        fclose(f);
    } else {
        fprintf(stderr, "Error writing %s\n", output_path);
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

    // process_file(&arena, argv[1], argv[2]);

    process_directory(&arena, argv[1], argv[2]);

    arena_free(&arena);
    return 0;
}
