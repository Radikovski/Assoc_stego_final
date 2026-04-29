#include "assoc_stego_opt.h"

#if defined(_M_X64) || defined(__x86_64__) 
#include <immintrin.h>

// SSE-версия XOR (2 слова по 64 бита)
// AVX2-верси¤ XOR (4 слова за раз)
void bitvector_xor_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count) {
    size_t i = 0;
    for (; i + 3 < word_count; i += 4) {
        __m256i va = _mm256_loadu_si256((__m256i*) & a[i]);
        __m256i vb = _mm256_loadu_si256((__m256i*) & b[i]);
        __m256i vr = _mm256_xor_si256(va, vb);
        _mm256_storeu_si256((__m256i*) & result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

// AVX2-верси¤ AND
void bitvector_and_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count) {
    size_t i = 0;
    for (; i + 3 < word_count; i += 4) {
        __m256i va = _mm256_loadu_si256((__m256i*) & a[i]);
        __m256i vb = _mm256_loadu_si256((__m256i*) & b[i]);
        __m256i vr = _mm256_and_si256(va, vb);
        _mm256_storeu_si256((__m256i*) & result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}

#else
// Fallback для платформ без SSE
void bitvector_xor_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void bitvector_and_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}
#endif
