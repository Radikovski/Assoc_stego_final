#ifndef ASSOC_STEGO_H
#define ASSOC_STEGO_H
#include "buffer_pool.h"
#include "bitvector.h"
#include <stddef.h>
#include <stdbool.h>

#define MAX_ETALONS 256

// Кэш для ускорения DiscloseEtalon
typedef struct {
    uint64_t* etalon_masked;  // etalons[i] & key[i] - вычисляется 1 раз
} EtalonCache;

typedef struct {
    BitVector** etalons;
    size_t etalon_count;
    size_t etalon_length;

    BitVector** key;
    bool key_generated;

    // Кэш для ускорения расшифрования
    EtalonCache* cache;

    int** mask_unit_positions;
    size_t* mask_unit_counts;
    BufferPool* byte_pool;
    BufferPool* container_pool;
} AssocStego;

AssocStego* assoc_stego_create(const char** etalon_strings, size_t count);
void assoc_stego_free(AssocStego* as);

int assoc_stego_create_key(AssocStego* as);
int assoc_stego_save_key(const AssocStego* as, const char* filepath);
int assoc_stego_load_key(AssocStego* as, const char* filepath);

// Оптимизированные функции
BitVector* assoc_stego_hide_etalon(const AssocStego* as, int index);
int assoc_stego_disclose_etalon_cached(const AssocStego* as, const uint64_t* container_data);

// Новые функции
int assoc_stego_hide_byte_fast(const AssocStego* as, uint8_t val, uint8_t* out_buffer, size_t* out_len);
int assoc_stego_disclose_byte_fast(const AssocStego* as, const uint8_t* hidden, size_t len, uint8_t* out_val);

// Для совместимости
BitVector* assoc_stego_generate_container(size_t bit_length);
int assoc_stego_hide_byte(const AssocStego* as, uint8_t value, uint8_t** out_hidden, size_t* out_len);
int assoc_stego_disclose_byte(const AssocStego* as, const uint8_t* hidden, size_t len, uint8_t* out_value);

#endif