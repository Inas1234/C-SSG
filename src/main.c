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
#include "utils/simd.h"
#include "parser/mlinyaml.h"

#define TEMPLATE_PATH "templates/default.html"
char *template_path = "templates/default.html";
#define CACHE_FILE ".cssg_cache"

typedef struct {
    const char* head;
    size_t head_len;
    const char* middle;
    size_t middle_len;
    const char* tail;
    size_t tail_len;
} TemplateParts;

static TemplateParts template_parts;

static void collect_markdown_files(const char* input_dir, FileVector* files);
static void process_files_parallel(FileVector* files, const char* output_dir, 
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base);
static void process_file(Arena* process_arena,
                        const char* input_path, const char* output_path,
                        BuildCache* cache, WriteBatch* batch);
char* render_template(Arena* arena, const FrontMatter* fm, const char* content);
static void log_metrics(const BuildMetrics* metrics);
static void load_template(void);
static void unload_template(void);
static char* generate_output_path(const char* base, const char* input, const char* output_dir);
static void ensure_directory_exists(const char* filepath);

static MappedFile global_template = {0};
static BuildCache thread_cache;
#pragma omp threadprivate(thread_cache)



int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config-file>\n", argv[0]);
        return 1;
    }


    YamlConfig config = {0};
    if (parse_yaml(argv[1], &config) != 0) {
        fprintf(stderr, "Error parsing config file\n");
        return 1;
    }

    if (config.tmpl) {
        template_path = (char*)config.tmpl;
    }

    load_template();
    

    omp_set_num_threads(4); // Optimized for M1 Pro performance

    Arena arena;
    BuildCache global_cache = NULL;
    FileVector files;
    BuildMetrics metrics = {0};

    arena_init(&arena, 4 * 1024 * 1024); // 4MB arena
    vec_init(&files);

    cache_load(&global_cache, CACHE_FILE);
    collect_markdown_files(config.input_dir, &files);
    create_directory(config.output_dir); 
    copy_directory_structure(config.input_dir, config.output_dir);

    clock_t start = clock();
    process_files_parallel(&files, config.output_dir, &global_cache, &metrics, config.input_dir);

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
    global_template = mmap_file(template_path);
    if (global_template.data) {
        char* copy = malloc(global_template.size + 1);
        memcpy(copy, global_template.data, global_template.size);
        copy[global_template.size] = '\0';
        global_template.data = copy;
    }

    char* title_start = simd_strstr((char*)global_template.data, "{{title}}");
    char* content_start = simd_strstr(title_start + 9, "{{content}}");
    
    template_parts.head = global_template.data;
    template_parts.head_len = title_start - global_template.data;
    
    template_parts.middle = title_start + 9;
    template_parts.middle_len = content_start - template_parts.middle;
    
    template_parts.tail = content_start + 11;
    template_parts.tail_len = global_template.size - (content_start - global_template.data) - 11;

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


char* render_template(Arena* arena, const FrontMatter* fm, const char* content) {
    size_t title_len = fm->title ? strlen(fm->title) : 0;
    size_t content_len = content ? strlen(content) : 0;

    size_t total_size = template_parts.head_len + title_len + 
                       template_parts.middle_len + content_len + 
                       template_parts.tail_len + 1;
    
    char* output = arena_alloc(arena, total_size);
    char* ptr = output;
    
    memcpy(ptr, template_parts.head, template_parts.head_len);
    ptr += template_parts.head_len;
    
    if (fm->title) {
        memcpy(ptr, fm->title, title_len);
        ptr += title_len;
    }
    
    memcpy(ptr, template_parts.middle, template_parts.middle_len);
    ptr += template_parts.middle_len;
    
    if (content) {
        memcpy(ptr, content, content_len);
        ptr += content_len;
    }
    
    memcpy(ptr, template_parts.tail, template_parts.tail_len);
    ptr += template_parts.tail_len;
    *ptr = '\0';
    
    return output;
}


static void process_files_parallel(FileVector* files, const char* output_dir,
                                  BuildCache* global_cache, BuildMetrics* metrics,
                                  const char* input_base) {
    #pragma omp parallel
    {
        WriteBatch local_batch = {0};
        Arena thread_arena;
        BuildCache thread_cache = NULL; 
        size_t local_built = 0;

        arena_init(&thread_arena, 4 * 1024 * 1024);

        #pragma omp for schedule(static, 100)
        for (size_t i = 0; i < files->count; i++) {
            const char* input_path = files->items[i];
            
            int should_rebuild = 0;
            #pragma omp critical(CacheCheck)
            {
                should_rebuild = needs_rebuild(input_path, global_cache);
            }

            if (should_rebuild) {
                char* output_path = generate_output_path(input_base, input_path, output_dir);
                ensure_directory_exists(output_path);

                process_file(&thread_arena, input_path, output_path, &thread_cache, &local_batch);
                local_built++;

                free(output_path);
            }
        }
        
        batch_flush(&local_batch);

        #pragma omp atomic
        metrics->built_files += local_built;

        #pragma omp critical(CacheUpdate)
        {
            CacheEntry *entry, *tmp;
            HASH_ITER(hh, thread_cache, entry, tmp) {
                cache_update_entry(global_cache,
                                 entry->input_path,
                                 entry->output_path,
                                 entry->last_modified,
                                 entry->content_hash);
            }
        }

        arena_free(&thread_arena);
        cache_free(&thread_cache);
    }
}

static char* generate_output_path(const char* base, const char* input, const char* output_dir) {
    const char* rel_path = input + strlen(base);
    if (*rel_path == '/') rel_path++;
    
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

static void process_file(Arena* process_arena, const char* input_path,
                        const char* output_path, BuildCache* local_cache,
                        WriteBatch* batch) {
    MappedFile input = mmap_file(input_path);
    if (!input.data) return;

    uint64_t content_hash = hash_from_memory(input.data, input.size);
    MarkdownDoc doc = parse_markdown(process_arena, input.data, input.size);
    munmap_file(input);

    char* html = render_template(process_arena, &doc.frontmatter, doc.html);
    size_t html_len = strlen(html);
    batch_add(batch, output_path, html, html_len);

    struct stat st;
    if (stat(input_path, &st) == 0) {
        // Add the build artifact to this thread's LOCAL cache.
        cache_update_entry(local_cache, input_path, output_path, st.st_mtime, content_hash);
    }
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