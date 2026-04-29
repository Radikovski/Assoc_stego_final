#include "assoc_stego_opt.h"

#if defined(_M_X64) || defined(__x86_64__) || defined(__ELBRUS__)
#include <immintrin.h>

// SSE-версия XOR (2 слова по 64 бита)
void bitvector_xor_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count) {
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
void bitvector_and_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count) {
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
