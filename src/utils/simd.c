// src/utils/simd.c
#include "utils/simd.h"
#include <string.h>

SimdLevel detect_simd_support(void) {
#ifdef __x86_64__
#elif __aarch64__
    return SIMD_NEON;
#endif
    return SIMD_NONE;
}

char* simd_strstr(char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    const size_t needle_len = strlen(needle);
    if (needle_len == 0) return haystack;

#ifdef __x86_64__
#elif __aarch64__
    if (detect_simd_support() == SIMD_NEON) {
        const uint8x16_t first = vdupq_n_u8(needle[0]);
        const size_t len = strlen(haystack);
        
        for (size_t i = 0; i < len; i += 16) {
            const uint8x16_t block = vld1q_u8((const uint8_t*)(haystack + i));
            
            uint8x16_t cmp = vceqq_u8(block, first);
            
            uint64_t mask = vget_lane_u64(vreinterpret_u64_u32(
                vqmovn_u64(vreinterpretq_u64_u8(cmp))), 0);
            
            while (mask) {
                int idx = __builtin_ctzll(mask);
                if (i + idx + needle_len > len) break;
                
                if (memcmp(haystack + i + idx, needle, needle_len) == 0) {
                    return haystack + i + idx;
                }
                mask &= ~(1ULL << idx);
            }
        }
    }
#endif
    return strstr(haystack, needle);
}