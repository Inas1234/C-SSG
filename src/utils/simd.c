// src/utils/simd.c
#include "utils/simd.h"
#include <string.h>
#include <stddef.h>

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

char* simd_strchr(const char* str, int c) {
    if (!str) return NULL;
    
#ifdef __aarch64__
    if (detect_simd_support() == SIMD_NEON) {
        const uint8x16_t target = vdupq_n_u8((uint8_t)c);
        const char* p = str;
        
        // Align to 16-byte boundary for better performance
        while (((uintptr_t)p & 15) != 0) {
            if (*p == c) return (char*)p;
            if (*p == '\0') return NULL;
            p++;
        }
        
        // Process 16 bytes at a time
        for (;; p += 16) {
            const uint8x16_t chunk = vld1q_u8((const uint8_t*)p);
            
            // Check for null terminator first
            uint8x16_t null_cmp = vceqq_u8(chunk, vdupq_n_u8(0));
            uint64x2_t null_mask = vreinterpretq_u64_u8(null_cmp);
            uint64_t null_mask0 = vgetq_lane_u64(null_mask, 0);
            uint64_t null_mask1 = vgetq_lane_u64(null_mask, 1);
            
            if (null_mask0 | null_mask1) {
                // Found null terminator, check remaining bytes
                int null_pos = null_mask0 ? __builtin_ctzll(null_mask0) / 8 : 8 + __builtin_ctzll(null_mask1) / 8;
                for (int i = 0; i < null_pos; i++) {
                    if (p[i] == c) return (char*)(p + i);
                }
                return NULL;
            }
            
            // Check for target character
            uint8x16_t cmp = vceqq_u8(chunk, target);
            uint64x2_t mask = vreinterpretq_u64_u8(cmp);
            uint64_t mask0 = vgetq_lane_u64(mask, 0);
            uint64_t mask1 = vgetq_lane_u64(mask, 1);
            
            if (mask0 | mask1) {
                int pos = mask0 ? __builtin_ctzll(mask0) / 8 : 8 + __builtin_ctzll(mask1) / 8;
                return (char*)(p + pos);
            }
        }
    }
#endif
    return strchr(str, c);
}

char* simd_memchr(const char* str, int c, size_t len) {
    if (!str || len == 0) return NULL;
    
#ifdef __aarch64__
    if (detect_simd_support() == SIMD_NEON) {
        const uint8x16_t target = vdupq_n_u8((uint8_t)c);
        const char* p = str;
        size_t remaining = len;
        
        // Process 16 bytes at a time
        while (remaining >= 16) {
            const uint8x16_t chunk = vld1q_u8((const uint8_t*)p);
            uint8x16_t cmp = vceqq_u8(chunk, target);
            
            uint64x2_t mask = vreinterpretq_u64_u8(cmp);
            uint64_t mask0 = vgetq_lane_u64(mask, 0);
            uint64_t mask1 = vgetq_lane_u64(mask, 1);
            
            if (mask0 | mask1) {
                int pos = mask0 ? __builtin_ctzll(mask0) / 8 : 8 + __builtin_ctzll(mask1) / 8;
                return (char*)(p + pos);
            }
            
            p += 16;
            remaining -= 16;
        }
        
        // Handle remaining bytes
        for (size_t i = 0; i < remaining; i++) {
            if (p[i] == c) return (char*)(p + i);
        }
        return NULL;
    }
#endif
    return memchr(str, c, len);
}