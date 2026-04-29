#ifndef ASSOC_STEGO_OPT_H
#define ASSOC_STEGO_OPT_H

#include <stdint.h>  // ? ÄÎÁÀÂÈÒÜ İÒÓ ÑÒĞÎÊÓ
#include <stddef.h>  // ? Äëÿ size_t

#if defined(_M_X64) || defined(__x86_64__) || defined(__ELBRUS__)
#include <immintrin.h>

// Ğ’ĞµĞºÑ‚Ğ¾Ñ€Ğ½Ñ‹Ğµ Ğ¾Ğ¿ĞµÑ€Ğ°Ñ†Ğ¸Ğ¸ (SSE Ğ´Ğ»Ñ x86-64, __v2di Ğ´Ğ»Ñ Elbrus)
void bitvector_xor_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count);

void bitvector_and_sse(uint64_t* result, const uint64_t* a,
                       const uint64_t* b, size_t word_count);

#else
// Çàãëóøêà äëÿ äğóãèõ ïëàòôîğì
void bitvector_xor_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);

void bitvector_and_avx2(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count);
#endif

#endif
