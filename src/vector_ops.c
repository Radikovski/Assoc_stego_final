#include "vector_ops.h"
#ifdef __ELBRUS__
    #include <e2kintrin.h>
    
    // Используем нативные векторные регистры Эльбруса __v2di (128 бит = 2 × uint64)
    
    void vector_xor(const uint64_t *a, const uint64_t *b, 
                    uint64_t *result, size_t word_count) {
        size_t i = 0;
        
        // Обработка по 2 слова (128 бит) за раз с помощью __v2di
	 // Раскрутка x2 + векторизация через __v2di
        #pragma unroll(1)
        #pragma loop count(100)
        for (; i + 1 < word_count; i += 2) {
            __v2di va = {a[i], a[i+1]};
            __v2di vb = {b[i], b[i+1]};
            __v2di vr = va ^ vb;  // Векторный XOR
            result[i] = vr[0];
            result[i+1] = vr[1];
        }
        
        // Хвост
        for (; i < word_count; i++) {
            result[i] = a[i] ^ b[i];
        }
    }
    
    void vector_and(const uint64_t *a, const uint64_t *b, 
                    uint64_t *result, size_t word_count) {
        size_t i = 0;
        
	 // Раскрутка x2 + векторизация через __v2di
        #pragma unroll(1)
        #pragma loop count(100)
        for (; i + 1 < word_count; i += 2) {
            __v2di va = {a[i], a[i+1]};
            __v2di vb = {b[i], b[i+1]};
            __v2di vr = va & vb;  // Векторный AND
            result[i] = vr[0];
            result[i+1] = vr[1];
        }
        
        for (; i < word_count; i++) {
            result[i] = a[i] & b[i];
        }
    }

#else
    // Скалярная версия для x86-64
    void vector_xor(const uint64_t *a, const uint64_t *b, 
                    uint64_t *result, size_t word_count) {
        for (size_t i = 0; i < word_count; i++) {
            result[i] = a[i] ^ b[i];
        }
    }
    
    void vector_and(const uint64_t *a, const uint64_t *b, 
                    uint64_t *result, size_t word_count) {
        for (size_t i = 0; i < word_count; i++) {
            result[i] = a[i] & b[i];
        }
    }
#endif
