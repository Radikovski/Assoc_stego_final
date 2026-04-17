#ifndef VECTOR_OPS_H
#define VECTOR_OPS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Векторное побитовое XOR: result = a ^ b
 * 
 * @param a Первый операнд (массив uint64_t)
 * @param b Второй операнд (массив uint64_t)
 * @param result Буфер для результата (должен быть выделен заранее)
 * @param word_count Количество 64-битных слов для обработки
 * 
 * @note Буферы a, b и result НЕ ДОЛЖНЫ пересекаться!
 */
void vector_xor(const uint64_t *a, const uint64_t *b, 
                uint64_t *result, size_t word_count);

/**
 * @brief Векторное побитовое AND: result = a & b
 * 
 * @param a Первый операнд (массив uint64_t)
 * @param b Второй операнд (массив uint64_t)
 * @param result Буфер для результата (должен быть выделен заранее)
 * @param word_count Количество 64-битных слов для обработки
 * 
 * @note Буферы a, b и result НЕ ДОЛЖНЫ пересекаться!
 */
void vector_and(const uint64_t *a, const uint64_t *b, 
                uint64_t *result, size_t word_count);

#endif /* VECTOR_OPS_H */
