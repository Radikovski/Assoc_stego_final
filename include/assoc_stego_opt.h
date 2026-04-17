#ifndef ASSOC_STEGO_OPT_H
#define ASSOC_STEGO_OPT_H

#include <stdint.h>  // ? ДОБАВИТЬ ЭТУ СТРОКУ
#include <stddef.h>  // ? Для size_t

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>

// AVX2-версия XOR (4 слова за раз)
void bitvector_xor_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);

// AVX2-версия AND
void bitvector_and_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);
#else
// Заглушка для других платформ
void bitvector_xor_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);

void bitvector_and_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);
#endif

#endif