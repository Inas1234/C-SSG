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
#include "utils/mmap.h"
#include "utils/io.h"

#define TEMPLATE_PATH "templates/default.html"
#define CACHE_FILE ".cssg_cache"

static void collect_markdown_files(const char* input_dir, FileVector* files);
static void process_files_parallel(FileVector* files, const char* output_dir, 
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base);
static void process_file(Arena* process_arena, Arena* batch_arena,
                        const char* input_path, const char* output_path,
                        BuildCache* cache, WriteBatch* batch);
static char* render_template(Arena* arena, const char* template, const FrontMatter* fm, const char* content);
// static char* read_entire_file(const char* path, size_t* len);
static void log_metrics(const BuildMetrics* metrics);
static void load_template(void);
static void unload_template(void);
static char* generate_output_path(const char* base, const char* input, const char* output_dir);
static void ensure_directory_exists(const char* filepath);

// Global thread-local cache
static MappedFile global_template = {0};
static BuildCache thread_cache;
#pragma omp threadprivate(thread_cache)

// static WriteBatch thread_batch;
// #pragma omp threadprivate(thread_batch)

int main(int argc, char** argv) {
    load_template();
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

    // batch_flush(&thread_batch);

    metrics.total_time = (double)(clock() - start) / CLOCKS_PER_SEC;

    metrics.total_files = files.count;
    log_metrics(&metrics);

    cache_purge_missing(&global_cache);
    cache_save(&global_cache, CACHE_FILE);
    

    vec_free(&files);
    cache_free(&global_cache);
    arena_free(&arena);
    unload_template();
    return 0;
}


static void load_template() {
    global_template = mmap_file(TEMPLATE_PATH);
    if (global_template.data) {
        char* copy = malloc(global_template.size + 1);
        memcpy(copy, global_template.data, global_template.size);
        copy[global_template.size] = '\0';
        global_template.data = copy;
    }
}

static void unload_template() {
    if (global_template.data) {
        free((void*)global_template.data);
        global_template.data = NULL;
    }
}

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

// static char* read_entire_file(const char* path, size_t* len) {
//     FILE* f = fopen(path, "rb");
//     if (!f) return NULL;
    
//     fseek(f, 0, SEEK_END);
//     long size = ftell(f);
//     fseek(f, 0, SEEK_SET);
    
//     char* buffer = malloc(size + 1);
//     fread(buffer, 1, size, f);
//     buffer[size] = '\0';
    
//     fclose(f);
//     *len = size;
//     return buffer;
// }

static void process_files_parallel(FileVector* files, const char* output_dir,
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base) {
    #pragma omp parallel
    {
        WriteBatch local_batch = {0};

        Arena thread_arena;
        Arena batch_arena;
        BuildCache thread_cache;
        arena_init(&thread_arena, 1024 * 1024);
        arena_init(&batch_arena, 64 * 1024 * 1024);  // Larger arena
        cache_init(&thread_cache, 64);
        size_t local_built = 0;

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < files->count; i++) {
            const char* input_path = files->items[i];
            
            // Calculate relative path
            const char* relative_path = input_path + strlen(input_base);
            if (*relative_path == '/') relative_path++;

            // Generate output path
            char* base = strip_extension(relative_path);
            char* output_path = generate_output_path(input_base, files->items[i], output_dir);
            ensure_directory_exists(output_path);
            free(base);

            // Check rebuild needs
            int should_rebuild = 0;
            #pragma omp critical
            {
                should_rebuild = needs_rebuild(input_path, output_path, global_cache);
            }

            if (should_rebuild) {
                process_file(&thread_arena, &batch_arena, input_path, output_path, &thread_cache, &local_batch);
                local_built++;
            }
        }
        
        batch_flush(&local_batch);

        #pragma omp atomic
        metrics->built_files += local_built;

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
        arena_free(&batch_arena);
        cache_free(&thread_cache);
    }
}

static char* generate_output_path(const char* base, const char* input, const char* output_dir) {
    const char* rel_path = input + strlen(base);
    if (*rel_path == '/') rel_path++;
    
    // Handle files without .md extension
    char* copy = strdup(rel_path);
    char* dot = strrchr(copy, '.');
    if (dot && strcmp(dot, ".md") == 0) {
        *dot = '\0';
    }
    
    char* path = malloc(PATH_MAX);
    snprintf(path, PATH_MAX, "%s/%s.html", output_dir, copy);
    free(copy);
    
    return path;
}

static void ensure_directory_exists(const char* filepath) {
    char* dir = strdup(filepath);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkpath(dir, 0755);
    }
    free(dir);
}

static void process_file(Arena* process_arena, Arena* batch_arena,
                        const char* input_path, const char* output_path,
                        BuildCache* cache, WriteBatch* batch) {
    // Map input file
    MappedFile input = mmap_file(input_path);
    if (!input.data) {
        fprintf(stderr, "Error mapping %s\n", input_path);
        return;
    }

    // Parse markdown directly from memory
    MarkdownDoc doc = parse_markdown(process_arena, input.data, input.size);
    munmap_file(input);

    // Use cached template
    if (!global_template.data) {
        fprintf(stderr, "Template not loaded\n");
        return;
    }

    // Render using template in memory
    char* html = render_template(process_arena, global_template.data, 
                               &doc.frontmatter, doc.html);

    
    size_t html_len = strlen(html);
    char* html_copy = arena_alloc(batch_arena, html_len + 1);
    memcpy(html_copy, html, html_len + 1);
    batch_add(batch, output_path, html_copy, html_len);

    // Update cache
    struct stat st;
    if (stat(input_path, &st) == 0) {
        cache_add_entry(cache, input_path, output_path, 
                       st.st_mtime, file_hash(input_path));
    }

    arena_reset(process_arena);
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