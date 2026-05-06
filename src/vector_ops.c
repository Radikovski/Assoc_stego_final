#include <stdio.h>
#include "vector_ops.h"

#if defined(__AVX2__)
#include <immintrin.h>

// ==========================================
// AVX2-версия (4 слова по 64 бита = 256 бит)
// ==========================================
void vector_xor(const uint64_t* a, const uint64_t* b, uint64_t* result, size_t word_count) {
    size_t i = 0;
    // Обрабатываем сразу по 4 элемента
    for (; i + 3 < word_count; i += 4) {
        __m256i va = _mm256_loadu_si256((const __m256i*) & a[i]);
        __m256i vb = _mm256_loadu_si256((const __m256i*) & b[i]);
        __m256i vr = _mm256_xor_si256(va, vb);
        _mm256_storeu_si256((__m256i*) & result[i], vr);
    }
    // Добивка оставшихся элементов (если word_count не кратно 4)
    for (; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void vector_and(const uint64_t* a, const uint64_t* b, uint64_t* result, size_t word_count) {
    size_t i = 0;
    for (; i + 3 < word_count; i += 4) {
        __m256i va = _mm256_loadu_si256((const __m256i*) & a[i]);
        __m256i vb = _mm256_loadu_si256((const __m256i*) & b[i]);
        __m256i vr = _mm256_and_si256(va, vb);
        _mm256_storeu_si256((__m256i*) & result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}

#elif defined(_M_X64) || defined(__x86_64__) 
#include <immintrin.h>

// ==========================================
// SSE-версия (2 слова по 64 бита = 128 бит)
// ==========================================
void vector_xor(const uint64_t* a, const uint64_t* b, uint64_t* result, size_t word_count) {
    size_t i = 0;
    for (; i + 1 < word_count; i += 2) {
        __m128i va = _mm_loadu_si128((const __m128i*) & a[i]);
        __m128i vb = _mm_loadu_si128((const __m128i*) & b[i]);
        __m128i vr = _mm_xor_si128(va, vb);
        _mm_storeu_si128((__m128i*) & result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void vector_and(const uint64_t* a, const uint64_t* b, uint64_t* result, size_t word_count) {
    size_t i = 0;
    for (; i + 1 < word_count; i += 2) {
        __m128i va = _mm_loadu_si128((const __m128i*) & a[i]);
        __m128i vb = _mm_loadu_si128((const __m128i*) & b[i]);
        __m128i vr = _mm_and_si128(va, vb);
        _mm_storeu_si128((__m128i*) & result[i], vr);
    }
    for (; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}

#else
// ==========================================
// Fallback для платформ без SSE/AVX (Эльбрус)
// ==========================================
void vector_xor(const uint64_t* restrict a, const uint64_t* restrict b, uint64_t* restrict result, size_t word_count) {
#pragma ivdep
#pragma unroll(4) 
#pragma loop count(min=1, max=256)  
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

void vector_and(const uint64_t* restrict a, const uint64_t* restrict b, uint64_t* restrict result, size_t word_count) {
#pragma ivdep
#pragma unroll(4) 
#pragma loop count(min=1, max=256)  
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}
#endif

// ==========================================
// Информационные сообщения при компиляции
// ==========================================
#ifdef __ELBRUS__
#pragma message("=== COMPILING FOR ELBRUS (VLIW / scalar loop) ===")
#elif defined(__e2k__)
#pragma message("=== COMPILING FOR E2K (VLIW / scalar loop) ===")
#elif defined(__AVX2__)
#pragma message("=== COMPILING FOR x86-64 (AVX2 - 256 bit) ===")
#elif defined(__x86_64__) || defined(_M_X64)
#pragma message("=== COMPILING FOR x86-64 (SSE2 - 128 bit) ===")
#else
#pragma message("=== COMPILING FALLBACK (scalar) ===")
#endif