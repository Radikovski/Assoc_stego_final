#include "bitvector.h"
#include "profiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vector_ops.h"

// ========== Быстрый PRNG (Xorshift64) ==========
static uint64_t g_rng_state = 0x123456789ABCDEF0ULL;

void bitvector_rng_seed(uint64_t seed) {
    g_rng_state = seed ? seed : 0x123456789ABCDEF0ULL;
}

void bitvector_rng_fill(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        g_rng_state ^= g_rng_state >> 12;
        g_rng_state ^= g_rng_state << 25;
        g_rng_state ^= g_rng_state >> 27;
        buffer[i] = (uint8_t)((g_rng_state * 0x2545F4914F6CDD1DULL) >> 32);
    }
}

// ========== Базовые функции ==========
static inline size_t calc_word_count(size_t bits) { return (bits + 63) / 64; }

static inline void get_pos(size_t bit_idx, size_t* word, int* offset) {
    *word = bit_idx / 64;
    *offset = bit_idx % 64;
}

BitVector* bitvector_create(size_t bit_length) {
    if (bit_length == 0) return NULL;
    BitVector* bv = (BitVector*)malloc(sizeof(BitVector));
    if (!bv) return NULL;
    bv->bit_length = bit_length;
    bv->word_count = calc_word_count(bit_length);
    bv->data = (uint64_t*)calloc(bv->word_count, sizeof(uint64_t));
    if (!bv->data) { free(bv); return NULL; }
    return bv;
}

BitVector* bitvector_from_string(const char* binary_str) {
    PROFILE_START("bitvector_from_string");
    if (!binary_str) { PROFILE_END("bitvector_from_string"); return NULL; }
    size_t len = strlen(binary_str);
    if (len == 0) { PROFILE_END("bitvector_from_string"); return NULL; }
    BitVector* bv = bitvector_create(len);
    if (!bv) { PROFILE_END("bitvector_from_string"); return NULL; }
    for (size_t i = 0; i < len; i++) {
        if (binary_str[i] == '1') {
            size_t bit_pos = len - 1 - i;
            size_t w; int o;
            get_pos(bit_pos, &w, &o);
            bv->data[w] |= ((uint64_t)1 << o);
        }
    }
    PROFILE_END("bitvector_from_string");
    return bv;
}

BitVector* bitvector_from_bytes(const uint8_t* bytes, size_t byte_len, size_t bit_length) {
    PROFILE_START("bitvector_from_bytes");
    if (!bytes || bit_length == 0) { PROFILE_END("bitvector_from_bytes"); return NULL; }
    BitVector* bv = bitvector_create(bit_length);
    if (!bv) { PROFILE_END("bitvector_from_bytes"); return NULL; }
    size_t limit = (byte_len * 8 < bit_length) ? byte_len * 8 : bit_length;
    for (size_t i = 0; i < limit; i++) {
        if (bytes[i / 8] & (1 << (i % 8))) {
            bitvector_set_bit(bv, i, true);
        }
    }
    PROFILE_END("bitvector_from_bytes");
    return bv;
}

void bitvector_free(BitVector* bv) {
    if (bv) { if (bv->data) free(bv->data); free(bv); }
}

bool bitvector_get_bit(const BitVector* bv, size_t index) {
    if (!bv || index >= bv->bit_length) return false;
    size_t w; int o;
    get_pos(index, &w, &o);
    return (bv->data[w] >> o) & 1;
}

void bitvector_set_bit(BitVector* bv, size_t index, bool value) {
    if (!bv || index >= bv->bit_length) return;
    size_t w; int o;
    get_pos(index, &w, &o);
    if (value) bv->data[w] |= ((uint64_t)1 << o);
    else bv->data[w] &= ~((uint64_t)1 << o);
}

BitVector* bitvector_and(const BitVector* a, const BitVector* b) {
    PROFILE_START("bitvector_and");
    if (!a || !b || a->bit_length != b->bit_length) { PROFILE_END("bitvector_and"); return NULL; }
    BitVector* res = bitvector_create(a->bit_length);
    if (!res) { PROFILE_END("bitvector_and"); return NULL; }
    vector_and(a->data, b->data, res->data, a->word_count);
    PROFILE_END("bitvector_and");
    return res;
}

BitVector* bitvector_xor(const BitVector* a, const BitVector* b) {
    PROFILE_START("bitvector_xor");
    if (!a || !b || a->bit_length != b->bit_length) { PROFILE_END("bitvector_xor"); return NULL; }
    BitVector* res = bitvector_create(a->bit_length);
    if (!res) { PROFILE_END("bitvector_xor"); return NULL; }
    vector_xor(a->data, b->data, res->data, a->word_count);
    PROFILE_END("bitvector_xor");
    return res;
}

bool bitvector_is_zero(const BitVector* bv) {
    if (!bv) return true;
    for (size_t i = 0; i < bv->word_count; i++) if (bv->data[i]) return false;
    return true;
}

bool bitvector_equals(const BitVector* a, const BitVector* b) {
    if (!a || !b || a->bit_length != b->bit_length) return false;
    for (size_t i = 0; i < a->word_count; i++) if (a->data[i] != b->data[i]) return false;
    return true;
}

size_t bitvector_popcount(const BitVector* bv) {
    if (!bv) return 0;
    size_t count = 0;
    for (size_t i = 0; i < bv->word_count; i++) {
#if defined(__GNUC__)
        count += __builtin_popcountll(bv->data[i]);
#elif defined(_MSC_VER)
        count += __popcnt64(bv->data[i]);
#else
        uint64_t v = bv->data[i];
        v = v - ((v >> 1) & 0x5555555555555555ULL);
        v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
        count += (((v + (v >> 4)) & 0xF0F0F0F0F0F0F0FULL) * 0x101010101010101ULL) >> 56;
#endif
    }
    return count;
}

uint8_t* bitvector_to_bytes(const BitVector* bv, size_t* out_len) {
    PROFILE_START("bitvector_to_bytes");
    if (!bv || !out_len) { PROFILE_END("bitvector_to_bytes"); return NULL; }
    *out_len = (bv->bit_length + 7) / 8;
    uint8_t* bytes = (uint8_t*)calloc(*out_len, 1);
    if (!bytes) { PROFILE_END("bitvector_to_bytes"); return NULL; }
    for (size_t i = 0; i < bv->bit_length; i++) {
        if (bitvector_get_bit(bv, i)) {
            bytes[i / 8] |= (1 << (i % 8));
        }
    }
    PROFILE_END("bitvector_to_bytes");
    return bytes;
}

int* bitvector_get_set_bit_positions(const BitVector* bv, size_t* out_count) {
    if (!bv || !out_count) return NULL;
    size_t count = bitvector_popcount(bv);
    *out_count = count;
    if (count == 0) return NULL;
    int* pos = (int*)malloc(count * sizeof(int));
    if (!pos) return NULL;
    size_t idx = 0;
    for (size_t i = 0; i < bv->bit_length && idx < count; i++) {
        if (bitvector_get_bit(bv, i)) pos[idx++] = (int)i;
    }
    return pos;
}

BitVector* bitvector_create_with_bits(size_t bit_length, const int* positions, size_t count) {
    BitVector* bv = bitvector_create(bit_length);
    if (!bv) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (positions[i] >= 0 && (size_t)positions[i] < bit_length)
            bitvector_set_bit(bv, (size_t)positions[i], true);
    }
    return bv;
}