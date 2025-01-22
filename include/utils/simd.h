// include/utils/simd.h
#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

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

#endif