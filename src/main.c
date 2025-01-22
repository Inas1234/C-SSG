#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <omp.h>

#include "arena.h"
#include "parser/markdown.h"
#include "utils/cache.h"
#include "utils/vector.h"
#include "utils/path.h"

#define TEMPLATE_PATH "templates/default.html"
#define CACHE_FILE ".cssg_cache"

static void collect_markdown_files(const char* input_dir, FileVector* files);
static void process_files_parallel(FileVector* files, const char* output_dir, 
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base);
static void process_file(Arena* arena, const char* input_path, const char* output_path, BuildCache* cache);
static char* render_template(Arena* arena, const char* template, const FrontMatter* fm, const char* content);
static char* read_entire_file(const char* path, size_t* len);
static void log_metrics(const BuildMetrics* metrics);

// Global thread-local cache
static BuildCache thread_cache;
#pragma omp threadprivate(thread_cache)

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input-dir> <output-dir>\n", argv[0]);
        return 1;
    }

    omp_set_num_threads(omp_get_max_threads());

    Arena arena;
    BuildCache global_cache;
    FileVector files;
    BuildMetrics metrics = {0};

    arena_init(&arena, 1024 * 1024);
    cache_init(&global_cache, 128);
    vec_init(&files);

    cache_load(&global_cache, CACHE_FILE);
    collect_markdown_files(argv[1], &files);
    create_directory(argv[2]); 
    copy_directory_structure(argv[1], argv[2]);

    clock_t start = clock();
    process_files_parallel(&files, argv[2], &global_cache, &metrics, argv[1]);
    metrics.total_time = (double)(clock() - start) / CLOCKS_PER_SEC;

    metrics.total_files = files.count;
    log_metrics(&metrics);

    cache_purge_missing(&global_cache);
    cache_save(&global_cache, CACHE_FILE);
    
    vec_free(&files);
    cache_free(&global_cache);
    arena_free(&arena);

    return 0;
}

// Directory traversal implementation
static void collect_markdown_files(const char* input_dir, FileVector* files) {
    DIR* dir = opendir(input_dir);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);

        struct stat st;
        if (lstat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            collect_markdown_files(path, files);
        } else if (S_ISREG(st.st_mode)) {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".md") == 0) {
                vec_push(files, path);
            }
        }
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

static void process_files_parallel(FileVector* files, const char* output_dir,
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base) {
    #pragma omp parallel
    {
        Arena thread_arena;
        BuildCache thread_cache;
        arena_init(&thread_arena, 1024 * 1024);
        cache_init(&thread_cache, 64);
        size_t local_built = 0;  // Thread-local counter

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < files->count; i++) {
            const char* input_path = files->items[i];
            
            // Calculate relative path
            const char* relative_path = input_path + strlen(input_base);
            if (*relative_path == '/') relative_path++;

            // Build output path
            char output_path[PATH_MAX];
            snprintf(output_path, sizeof(output_path), "%s/%.*s.html",
                    output_dir,
                    (int)(strlen(relative_path) - 3),  // Remove .md extension
                    relative_path);

            // Check rebuild needs using global cache
            int should_rebuild = 0;
            #pragma omp critical
            {
                should_rebuild = needs_rebuild(input_path, output_path, global_cache);
            }

            if (should_rebuild) {
                process_file(&thread_arena, input_path, output_path, &thread_cache);
                local_built++;
            }
        }

        // Update metrics atomically
        #pragma omp atomic
        metrics->built_files += local_built;

        // Merge caches
        #pragma omp critical
        {
            for (size_t i = 0; i < thread_cache.count; i++) {
                cache_update_entry(global_cache,
                                 thread_cache.entries[i].input_path,
                                 thread_cache.entries[i].output_path,
                                 thread_cache.entries[i].last_modified,
                                 thread_cache.entries[i].content_hash);
            }
        }

        arena_free(&thread_arena);
        cache_free(&thread_cache);
    }
}
static void process_file(Arena* arena, const char* input_path, const char* output_path, BuildCache* cache) {
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

    FILE* f = fopen(output_path, "w");
    if (f) {
        fputs(html, f);
        fclose(f);
    } else {
        perror("Failed to write output file");
    }
    // Update cache
    struct stat st;
    if (stat(input_path, &st) == 0) {
        cache_add_entry(cache, input_path, output_path, st.st_mtime, file_hash(input_path));
    }

    arena_reset(arena);
}

void log_metrics(const BuildMetrics* metrics) {
    printf("\nBuild Report:\n"
           "  Total files:   %zu\n"
           "  Rebuilt:       %zu\n"
           "  Time elapsed:  %.2fms\n\n",
           metrics->total_files,
           metrics->built_files,
           metrics->total_time * 1000);
}