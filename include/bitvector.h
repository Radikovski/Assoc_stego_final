#ifndef BITVECTOR_H
#define BITVECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t* data;
    size_t bit_length;
    size_t word_count;
} BitVector;

// Создание/удаление
BitVector* bitvector_create(size_t bit_length);
BitVector* bitvector_from_string(const char* binary_str);
BitVector* bitvector_from_bytes(const uint8_t* bytes, size_t byte_len, size_t bit_len);
void bitvector_free(BitVector* bv);

// Операции
void bitvector_set_bit(BitVector* bv, size_t index, bool value);
bool bitvector_get_bit(const BitVector* bv, size_t index);

// ← БЫСТРЫЕ INPLACE ОПЕРАЦИИ (без malloc)
static inline void bitvector_xor_inplace(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] ^ b[i];
    }
}

static inline void bitvector_and_inplace(uint64_t* result, const uint64_t* a,
    const uint64_t* b, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        result[i] = a[i] & b[i];
    }
}

// ← БЫСТРАЯ КОНВЕРТАЦИЯ БЕЗ MALLOC
static inline void bitvector_init_from_byte(uint64_t* buffer, size_t word_count, uint8_t byte) {
    for (size_t i = 0; i < word_count; i++) buffer[i] = 0;
    buffer[0] = (uint64_t)byte;
}

static inline uint8_t bitvector_extract_byte(uint64_t* buffer, size_t word_count) {
    (void)word_count;
    return (uint8_t)(buffer[0] & 0xFF);
}

// Для совместимости
BitVector* bitvector_and(const BitVector* a, const BitVector* b);
BitVector* bitvector_xor(const BitVector* a, const BitVector* b);
bool bitvector_equals(const BitVector* a, const BitVector* b);
bool bitvector_is_zero(const BitVector* bv);
size_t bitvector_popcount(const BitVector* bv);
uint8_t* bitvector_to_bytes(const BitVector* bv, size_t* out_byte_length);
int* bitvector_get_set_bit_positions(const BitVector* bv, size_t* out_count);
BitVector* bitvector_create_with_bits(size_t bit_length, const int* positions, size_t count);

// PRNG
void bitvector_rng_seed(uint64_t seed);
void bitvector_rng_fill(uint8_t* buffer, size_t length);

#endif