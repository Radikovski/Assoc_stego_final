#include <stdio.h>
#include "vector_ops.h"
#if defined(_M_X64) || defined(__x86_64__) || defined(__e2k__)
#include <immintrin.h>

// SSE-версия XOR (2 слова по 64 бита)
void vector_xor(const uint64_t* a,
                       const uint64_t* b, uint64_t* result, size_t word_count) {
    size_t i = 0;
    for (; i + 1 < word_count; i += 2) {
        __m128i va = _mm_loadu_si128((__m128i*) &a[i]);
        __m128i vb = _mm_loadu_si128((__m128i*) &b[i]);
        __m128i vr = _mm_xor_si128(va, vb);
        _mm_storeu_si128((__m128i*) &result[i], vr);
    }
    // Добивка оставшихся элементов
    for (; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

// SSE-версия AND
void vector_and( const uint64_t* a,
                       const uint64_t* b,uint64_t* result,size_t word_count) {
    size_t i = 0;
    for (; i + 1 < word_count; i += 2) {
        __m128i va = _mm_loadu_si128((__m128i*) &a[i]);
        __m128i vb = _mm_loadu_si128((__m128i*) &b[i]);
        __m128i vr = _mm_and_si128(va, vb);
        _mm_storeu_si128((__m128i*) &result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}

#else
// Fallback для платформ без SSE
void vector_xor( const uint64_t* restrict a,
                       const uint64_t* restrict  b, uint64_t* restrict result, size_t word_count) {
 #pragma ivdep
 #pragma unroll(4) 
    #pragma loop count(min=1, max=256)  
 for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void vector_and(const uint64_t* a,
                       const uint64_t* b, uint64_t* result, size_t word_count) {
 #pragma ivdep
 #pragma unroll(4) 
    #pragma loop count(min=1, max=256)  
 for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}
#endif

#ifdef __ELBRUS__
    #pragma message("=== COMPILING FOR ELBRUS (__v2di) ===")
#elif defined(__e2k__)
    #pragma message("=== COMPILING FOR E2K (__v2di) ===")
#elif defined(__x86_64__) || defined(_M_X64)
    #pragma message("=== COMPILING FOR x86-64 (SSE2) ===")
#else
    #pragma message("=== COMPILING FALLBACK (scalar) ===")
#endif
