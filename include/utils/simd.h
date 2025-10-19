// include/utils/simd.h
#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <stddef.h>

#ifdef __x86_64__
#include <immintrin.h>
#elif __aarch64__
#include <arm_neon.h>
#endif

typedef enum {
    SIMD_NONE,
    SIMD_SSE4,
    SIMD_AVX2,
    SIMD_NEON
} SimdLevel;

SimdLevel detect_simd_support(void);
char* simd_strstr(char* haystack, const char* needle);
char* simd_strchr(const char* str, int c);
char* simd_memchr(const char* str, int c, size_t len);

#endif