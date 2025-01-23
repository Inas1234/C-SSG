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

#define TEMPLATE_PATH "templates/default.html"
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
static void process_file(Arena* process_arena, Arena* batch_arena,
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
        Arena batch_arena;
        BuildCache thread_cache;
        arena_init(&thread_arena, 1024 * 1024);
        arena_init(&batch_arena, 64 * 1024 * 1024);  
        cache_init(&thread_cache, 64);
        size_t local_built = 0;

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < files->count; i++) {
            const char* input_path = files->items[i];
            
            const char* relative_path = input_path + strlen(input_base);
            if (*relative_path == '/') relative_path++;

            char* base = strip_extension(relative_path);
            char* output_path = generate_output_path(input_base, files->items[i], output_dir);
            ensure_directory_exists(output_path);
            free(base);

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
    MappedFile input = mmap_file(input_path);
    if (!input.data) {
        fprintf(stderr, "Error mapping %s\n", input_path);
        return;
    }

    MarkdownDoc doc = parse_markdown(process_arena, input.data, input.size);
    munmap_file(input);

    if (!global_template.data) {
        fprintf(stderr, "Template not loaded\n");
        return;
    }

    char* html = render_template(process_arena, &doc.frontmatter, doc.html);

    
    size_t html_len = strlen(html);
    char* html_copy = arena_alloc(batch_arena, html_len + 1);
    memcpy(html_copy, html, html_len + 1);
    batch_add(batch, output_path, html_copy, html_len);

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