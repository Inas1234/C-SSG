// src/mlinyaml.c
#include "parser/mlinyaml.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "utils/simd.h"
#include <stdio.h>

#if defined(__aarch64__)
#define ALIGNMENT 16
#elif defined(__x86_64__)
#define ALIGNMENT 64
#else
#define ALIGNMENT 16
#endif

static inline const char* find_colon(const char* p) {
#if defined(__x86_64__)
    __m128i colon = _mm_set1_epi8(':');
    for (;; p += 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i*)p);
        __m128i eq = _mm_cmpeq_epi8(chunk, colon);
        unsigned mask = _mm_movemask_epi8(eq);
        if (mask) return p + __builtin_ctz(mask);
    }
#elif defined(__aarch64__)
    uint8x16_t colon = vdupq_n_u8(':');
    for (;; p += 16) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*)p);
        uint8x16_t cmp = vceqq_u8(chunk, colon);
        
        // Convert comparison results to 64-bit masks
        uint64x2_t cmp64 = vreinterpretq_u64_u8(cmp);
        uint64_t mask0 = vgetq_lane_u64(cmp64, 0);
        uint64_t mask1 = vgetq_lane_u64(cmp64, 1);
        
        if (mask0 | mask1) {
            if (mask0) {
                return p + (__builtin_ctzll(mask0) / 8);
            } else {
                return p + 8 + (__builtin_ctzll(mask1) / 8);
            }
        }
    }
#else
    // Fallback implementation
    return memchr(p, ':', 16);
#endif
}


static inline void trim_whitespace(const char** start, const char** end) {
    while (*start < *end && **start <= ' ') (*start)++;
    while (*end > *start && *(*end - 1) <= ' ') (*end)--;
}

int parse_yaml(const char* filename, YamlConfig* config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("File stat error");
        close(fd);
        return -1;
    }
    
    size_t size = st.st_size;
    char* data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) {
        perror("Memory mapping failed");
        return -1;
    }

    const char* p = data;  
    const char* end = data + size;

    while (p < end) {
        const char* line_end = (const char*)memchr(p, '\n', end - p);
        if (!line_end) line_end = end;

        const char* line_start = p;
        const char* colon = find_colon(line_start);
        
        if (colon && colon < line_end) {
            const char* key_start = line_start;
            const char* key_end = colon;
            trim_whitespace(&key_start, &key_end);

            const char* value_start = colon + 1;
            const char* value_end = line_end;
            trim_whitespace(&value_start, &value_end);

            size_t key_len = key_end - key_start;
            size_t value_len = value_end - value_start;

            if (key_len == 15 && !memcmp(key_start, "input_directory", 15)) {
                config->input_dir = strndup(value_start, value_len);
            } 
            else if (key_len == 16 && !memcmp(key_start, "output_directory", 16)) {
                config->output_dir = strndup(value_start, value_len);
            } 
            else if (key_len == 8 && !memcmp(key_start, "template", 8)) {
                config->tmpl = strndup(value_start, value_len);
            }
        }

        p = line_end + 1;
        if (p < end && *(p-1) == '\n' && *(p-2) == '\r') {
            p++;  
        }
    }

    munmap(data, size);
    return 0;
}
