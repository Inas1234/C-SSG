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
#include "utils/cache.h"

#define TEMPLATE_PATH "templates/default.html"
#define PATH_MAX 4096


static const char* CACHE_FILE = ".cssg_cache";
static BuildCache cache;

static void process_directory(Arena* arena, const char* input_dir, const char* output_dir, BuildMetrics* metrics);
static void process_entry(Arena* arena, const char* base_in, const char* base_out, const char* entry_name, BuildMetrics* metrics);
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

static void process_entry(Arena* arena, const char* base_in, 
                         const char* base_out, const char* entry_name,
                         BuildMetrics* metrics) {  // Add metrics parameter
    char in_path[PATH_MAX];
    char out_path[PATH_MAX];
    
    snprintf(in_path, sizeof(in_path), "%s/%s", base_in, entry_name);
    snprintf(out_path, sizeof(out_path), "%s/%s", base_out, entry_name);

    struct stat st;
    if (lstat(in_path, &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        process_directory(arena, in_path, out_path, metrics);
    } else if (S_ISREG(st.st_mode)) {
        const char* ext = strrchr(entry_name, '.');  // Declare ext here
        char out_file[PATH_MAX];  // Declare out_file in this scope

        metrics->total_files++;
        
        if (ext && strcmp(ext, ".md") == 0) {
            // Generate output path
            snprintf(out_file, sizeof(out_file), "%s/%s.html", 
                    base_out, strip_extension(entry_name));
            
            if (needs_rebuild(in_path, out_file, cache)) {
                process_file(arena, in_path, out_file);
                metrics->built_files++;
            }
        } else {
            if (needs_copy(in_path, out_path)) {
                copy_file(in_path, out_path);
                metrics->copied_files++;
            }
        }
    }
}
static void process_directory(Arena* arena, const char* input_dir, const char* output_dir, BuildMetrics* metrics) {
    create_directory(output_dir);
    
    DIR* dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Error opening directory: %s\n", input_dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // Skip hidden files
        
        process_entry(arena, input_dir, output_dir, entry->d_name, metrics);
    }
    
    closedir(dir);
}



char* render_template(Arena* arena, const char* template, const FrontMatter* fm, const char* content) {
    char* current = strstr(template, "{{title}}");
    if (!current) return NULL;
    
    size_t before_title = current - template;
    size_t title_len = fm->title ? strlen(fm->title) : 0;
    
    char* content_placeholder = strstr(current + 9, "{{content}}"); 
    if (!content_placeholder) return NULL;
    
    size_t between_len = content_placeholder - (current + 9);
    size_t content_len = content ? strlen(content) : 0;
    size_t after_content = strlen(content_placeholder + 11);

    size_t total_size = before_title + title_len + between_len + content_len + after_content + 1;
    char* output = arena_alloc(arena, total_size);

    char* ptr = output;
    
    memcpy(ptr, template, before_title);
    ptr += before_title;
    
    if (fm->title) {
        memcpy(ptr, fm->title, title_len);
        ptr += title_len;
    }
    
    memcpy(ptr, current + 9, between_len);
    ptr += between_len;
    
    if (content) {
        memcpy(ptr, content, content_len);
        ptr += content_len;
    }
    
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
    struct stat st;
    if (stat(input_path, &st) != 0) {
        fprintf(stderr, "Error reading %s\n", input_path);
        return;
    }

    uint64_t current_hash = file_hash(input_path);
    if (cache_contains(&cache, input_path)){
        for (size_t i = 0; i < cache.count; i++) {
            CacheEntry* e = &cache.entries[i];

            if (strcmp(e->input_path, input_path) == 0) {
                if (e->last_modified == st.st_mtimespec.tv_sec && e->content_hash == current_hash) {
                    return;
                }
                break;
            }
        }
    }


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

    char* dir = strdup(output_path);
    char* parent_dir = dirname(dir);
    create_directory(parent_dir);
    free(dir);

    FILE* f = fopen(output_path, "w");
    if (f) {
        fputs(html, f);
        fclose(f);
    } else {
        fprintf(stderr, "Error writing %s\n", output_path);
    }
    
    arena_reset(arena);

    cache_add_entry(&cache, input_path, output_path, st.st_mtimespec.tv_sec, current_hash);
}

void log_metrics(const BuildMetrics* metrics) {
    printf("\nBuild Report:\n"
           "  Total files:   %zu\n"
           "  Rebuilt:       %zu\n"
           "  Copied:        %zu\n"
           "  Time elapsed:  %.2fms\n\n",
           metrics->total_files, 
           metrics->built_files,
           metrics->copied_files,
           metrics->total_time * 1000);
}




int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <input-dir> <output-dir>\n", argv[0]);
        return 1;
    }

    Arena arena;
    arena_init(&arena, 1024 * 1024); 
    cache_init(&cache, 128);
    cache_load(&cache, CACHE_FILE);
    BuildMetrics metrics = {0};
    clock_t start = clock();

    process_directory(&arena, argv[1], argv[2], &metrics);


    cache_purge_missing(&cache);
    cache_save(&cache, CACHE_FILE);
    cache_free(&cache);

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    metrics.total_time = elapsed;
    log_metrics(&metrics);

    arena_free(&arena);
    return 0;
}
