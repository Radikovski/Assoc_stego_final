#include "assoc_stego_text.h"
#include "profiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _OPENMP
    #include <omp.h>
#endif

#define BUFFER_SIZE 65536  // 64 КБ буфер

int assoc_stego_encrypt_text(const AssocStego* as, const char* text,
    uint8_t** out_stego, size_t* out_len) {
    PROFILE_START("assoc_stego_encrypt_text");
    if (!as || !text || !out_stego || !out_len) { PROFILE_END("assoc_stego_encrypt_text"); return -1; }

    size_t text_len = strlen(text);
    size_t header_size = 4;
    size_t total_bytes = header_size + text_len;

    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t block_size = 3 * container_byte_len;
    size_t stego_size = total_bytes * block_size;

    *out_stego = (uint8_t*)malloc(stego_size);
    if (!*out_stego) { PROFILE_END("assoc_stego_encrypt_text"); return -1; }

    size_t pos = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (text_len >> (i * 8)) & 0xFF;
        uint8_t* hidden = NULL;
        size_t hlen = 0;
        if (assoc_stego_hide_byte(as, byte, &hidden, &hlen) != 0) {
            free(*out_stego); *out_stego = NULL; PROFILE_END("assoc_stego_encrypt_text"); return -1;
        }
        memcpy(*out_stego + pos, hidden, hlen);
        pos += hlen;
        free(hidden);
    }

    for (size_t i = 0; i < text_len; i++) {
        uint8_t* hidden = NULL;
        size_t hlen = 0;
        if (assoc_stego_hide_byte(as, (uint8_t)text[i], &hidden, &hlen) != 0) {
            free(*out_stego); *out_stego = NULL; PROFILE_END("assoc_stego_encrypt_text"); return -1;
        }
        memcpy(*out_stego + pos, hidden, hlen);
        pos += hlen;
        free(hidden);
    }

    *out_len = pos;
    PROFILE_END("assoc_stego_encrypt_text");
    return 0;
}

int assoc_stego_decrypt_text(const AssocStego* as, const uint8_t* stego,
    size_t stego_len, char** out_text) {
    PROFILE_START("assoc_stego_decrypt_text");
    if (!as || !stego || !out_text) { PROFILE_END("assoc_stego_decrypt_text"); return -1; }

    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t block_size = 3 * container_byte_len;

    if (stego_len < 4 * block_size) { PROFILE_END("assoc_stego_decrypt_text"); return -1; }

    size_t text_len = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte;
        if (assoc_stego_disclose_byte(as, stego + i * block_size, block_size, &byte) != 0) {
            PROFILE_END("assoc_stego_decrypt_text"); return -1;
        }
        text_len |= ((size_t)byte << (i * 8));
    }

    if (text_len > 10 * 1024 * 1024) { PROFILE_END("assoc_stego_decrypt_text"); return -1; }

    *out_text = (char*)malloc(text_len + 1);
    if (!*out_text) { PROFILE_END("assoc_stego_decrypt_text"); return -1; }

    for (size_t i = 0; i < text_len; i++) {
        size_t offset = (4 + i) * block_size;
        if (offset + block_size > stego_len) {
            free(*out_text); *out_text = NULL; PROFILE_END("assoc_stego_decrypt_text"); return -1;
        }
        if (assoc_stego_disclose_byte(as, stego + offset, block_size, (uint8_t*)(*out_text + i)) != 0) {
            free(*out_text); *out_text = NULL; PROFILE_END("assoc_stego_decrypt_text"); return -1;
        }
    }

    (*out_text)[text_len] = '\0';
    PROFILE_END("assoc_stego_decrypt_text");
    return 0;
}
//// ========== ВСПОМОГАТЕЛЬНАЯ: Шифрование одного байта в буфер ==========
//static size_t encrypt_byte_to_buffer(const AssocStego * as, uint8_t byte,
//    uint8_t * output_buffer, size_t offset) {
//    size_t hidden_len = 0;
//    if (assoc_stego_hide_byte_fast(as, byte, output_buffer + offset, &hidden_len) == 0) {
//        return hidden_len;
//    }
//    return 0;
//}
//
//
//
//// ========== ОДНОПОТОЧНОЕ ШИФРОВАНИЕ ==========
//static int encrypt_file_single(const AssocStego* as, const char* input_path,
//    const char* output_path, uint8_t* input_data,
//    long file_size, FILE* fout) {
//    size_t container_byte_len = (as->etalon_length + 7) / 8;
//    size_t block_size = 3 * container_byte_len;
//    size_t output_size = (file_size + 4) * block_size;
//
//    uint8_t* output_buffer = malloc(output_size);
//    if (!output_buffer) { return -1; }
//
//    size_t pos = 0;
//
//    // Заголовок (4 байта)
//    for (int i = 0; i < 4; i++) {
//        uint8_t byte = (file_size >> (i * 8)) & 0xFF;
//        size_t len = encrypt_byte_to_buffer(as, byte, output_buffer, pos);
//        if (len == 0) { free(output_buffer); return -1; }
//        pos += len;
//    }
//
//    // Данные
//    for (long i = 0; i < file_size; i++) {
//        size_t len = encrypt_byte_to_buffer(as, input_data[i], output_buffer, pos);
//        if (len == 0) { free(output_buffer); return -1; }
//        pos += len;
//    }
//
//    fwrite(output_buffer, 1, pos, fout);
//    free(output_buffer);
//    return 0;
//}
//
//// ========== МНОГОПОТОЧНОЕ ШИФРОВАНИЕ (исправленное) ==========
//#ifdef _OPENMP
//static int encrypt_file_parallel(const AssocStego* as, const char* input_path,
//    const char* output_path, uint8_t* input_data,
//    long file_size, FILE* fout, int num_threads) {
//    size_t container_byte_len = (as->etalon_length + 7) / 8;
//    size_t block_size = 3 * container_byte_len;
//    size_t header_size = 4 * block_size;
//    size_t output_size = header_size + (file_size * block_size);
//
//    printf("DEBUG: Allocating output buffer: %zu bytes\n", output_size);
//
//    if (file_size > 2000000000) {
//        printf("ERROR: File too large for OpenMP (%ld bytes)\n", file_size);
//        return -1;
//    }
//
//    uint8_t* output_buffer = calloc(output_size, 1);
//    if (!output_buffer) {
//        printf("ERROR: Cannot allocate output buffer!\n");
//        return -1;
//    }
//
//    const int file_size_int = (int)file_size;
//    int error_occurred = 0;
//
//    // ← Шифрование заголовка (4 байта, последовательно)
//    size_t pos = 0;
//    for (int i = 0; i < 4; i++) {
//        uint8_t byte = (file_size >> (i * 8)) & 0xFF;
//        size_t hidden_len = 0;
//        int res = assoc_stego_hide_byte_fast(as, byte, output_buffer + pos, &hidden_len);
//        if (res != 0 || hidden_len == 0) {
//            printf("ERROR: Failed to encrypt header byte %d\n", i);
//            free(output_buffer);
//            return -1;
//        }
//        pos += hidden_len;
//    }
//
//    printf("DEBUG: Header encrypted, pos = %zu\n", pos);
//    printf("DEBUG: Starting parallel encryption with %d threads\n", num_threads);
//    printf("DEBUG: file_size_int = %d, block_size = %zu\n", file_size_int, block_size);
//
//    // ← Параллельное шифрование данных
//#ifdef _OPENMP
//    #pragma omp parallel num_threads(num_threads)
//#endif
//    {
//        int thread_id = omp_get_thread_num();
//        int total_threads = omp_get_num_threads();
//
//        // Разделяем работу между потоками
//        int chunk_size = (file_size_int + total_threads - 1) / total_threads;
//        int start = thread_id * chunk_size;
//        int end = (start + chunk_size < file_size_int) ? (start + chunk_size) : file_size_int;
//
//        // ← Проверка корректности диапазона (без return!)
//        if (start >= end) {
//            printf("DEBUG: Thread %d: no work (start=%d, end=%d)\n", thread_id, start, end);
//            // ← Не используем return, просто выходим из блока
//        }
//        else {
//            printf("DEBUG: Thread %d: processing bytes %d to %d (chunk=%d)\n",
//                thread_id, start, end, chunk_size);
//
//            // Каждый поток пишет в свою часть буфера
//            for (int i = start; i < end; i++) {
//                size_t offset = header_size + ((size_t)i * block_size);
//
//                // ← Проверка выхода за границы
//                if (offset + block_size > output_size) {
//#pragma omp critical
//                    {
//                        printf("ERROR: Buffer overflow at byte %d (offset %zu, size %zu)\n",
//                            i, offset, output_size);
//                        error_occurred = 1;
//                    }
//                    continue;  // ← continue можно, return нельзя!
//                }
//
//                size_t hidden_len = encrypt_byte_to_buffer(as, input_data[i], output_buffer, offset);
//                if (hidden_len == 0) {
//#pragma omp critical
//                    {
//                        printf("ERROR: Failed to encrypt byte %d\n", i);
//                        error_occurred = 1;
//                    }
//                }
//            }
//        }
//    }  // ← Конец параллельного блока
//
//    printf("DEBUG: Parallel encryption complete, error_occurred = %d\n", error_occurred);
//
//    if (!error_occurred) {
//        size_t written = fwrite(output_buffer, 1, output_size, fout);
//        printf("DEBUG: Wrote %zu of %zu bytes\n", written, output_size);
//    }
//
//    free(output_buffer);
//    return error_occurred ? -1 : 0;
//}
//#endif
//
//// ========== ОСНОВНАЯ ФУНКЦИЯ ==========
//int assoc_stego_encrypt_file_mt(const AssocStego* as, const char* input_path,
//    const char* output_path, int num_threads) {
//    PROFILE_START("assoc_stego_encrypt_file_mt");
//
//    if (!as || !input_path || !output_path) {
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//    if (!as->key_generated) {
//        printf("ERROR: Key not generated!\n");
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//
//    FILE* fin = fopen(input_path, "rb");
//    if (!fin) {
//        printf("ERROR: Cannot open input file!\n");
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//
//    fseek(fin, 0, SEEK_END);
//    long file_size = ftell(fin);
//    fseek(fin, 0, SEEK_SET);
//
//    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {
//        printf("ERROR: Invalid file size (%ld bytes)\n", file_size);
//        fclose(fin);
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//
//    printf("DEBUG: Original file size = %ld bytes\n", file_size);
//
//    // Читаем весь файл в память
//    uint8_t* input_data = malloc(file_size);
//    if (!input_data) {
//        fclose(fin);
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//
//    if (fread(input_data, 1, file_size, fin) != (size_t)file_size) {
//        printf("ERROR: Cannot read input file!\n");
//        free(input_data);
//        fclose(fin);
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//    fclose(fin);
//
//    FILE* fout = fopen(output_path, "wb");
//    if (!fout) {
//        printf("ERROR: Cannot create output file!\n");
//        free(input_data);
//        PROFILE_END("assoc_stego_encrypt_file_mt");
//        return -1;
//    }
//
//    // ← Определение числа потоков
//#ifdef __ELBRUS__
//    printf("DEBUG: Elbrus mode (VLIW + vector, single thread)\n");
//    num_threads = 1;
//#else
//#ifdef _OPENMP
//    if (num_threads <= 0) {
//        num_threads = omp_get_max_threads();
//    }
//    printf("DEBUG: x86-64 mode (OpenMP, %d threads)\n", num_threads);
//#else
//    printf("DEBUG: x86-64 mode (single thread, OpenMP not available)\n");
//#endif
//#endif
//
//    // ← Вызов соответствующей версии
//    int result;
//#ifdef _OPENMP
//    result = encrypt_file_parallel(as, input_path, output_path, input_data,
//        file_size, fout, num_threads);
//#else
//    result = encrypt_file_single(as, input_path, output_path, input_data,
//        file_size, fout);
//#endif
//
//    size_t container_byte_len = (as->etalon_length + 7) / 8;
//    size_t block_size = 3 * container_byte_len;
//    size_t output_size = (file_size + 4) * block_size;
//
//    printf("DEBUG: Successfully encrypted %ld bytes\n", file_size);
//    printf("DEBUG: Output size = %zu bytes (%.2fx expansion)\n",
//        output_size, (double)output_size / file_size);
//
//    free(input_data);
//    fclose(fout);
//    PROFILE_END("assoc_stego_encrypt_file_mt");
//    return result;
//}


int assoc_stego_encrypt_file(const AssocStego* as, const char* input_path,
    const char* output_path) {
    PROFILE_START("assoc_stego_encrypt_file");
    if (!as || !input_path || !output_path) { PROFILE_END("assoc_stego_encrypt_file"); return -1; }
    if (!as->key_generated) { printf("ERROR: Key not generated!\n"); PROFILE_END("assoc_stego_encrypt_file"); return -1; }

    FILE* fin = fopen(input_path, "rb");
    if (!fin) { printf("ERROR: Cannot open input file!\n"); PROFILE_END("assoc_stego_encrypt_file"); return -1; }

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {
        printf("ERROR: Invalid file size (%ld bytes)\n", file_size);
        fclose(fin); PROFILE_END("assoc_stego_encrypt_file"); return -1;
    }

    printf("DEBUG: Original file size = %ld bytes\n", file_size);

    FILE* fout = fopen(output_path, "wb");
    if (!fout) { printf("ERROR: Cannot create output file!\n"); fclose(fin); PROFILE_END("assoc_stego_encrypt_file"); return -1; }

    // Запись заголовка
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (file_size >> (i * 8)) & 0xFF;
        uint8_t* hidden = NULL;
        size_t hidden_len = 0;
        if (assoc_stego_hide_byte(as, byte, &hidden, &hidden_len) != 0) {
            printf("ERROR: Failed to encrypt header byte %d\n", i);
            free(hidden); fclose(fin); fclose(fout); PROFILE_END("assoc_stego_encrypt_file"); return -1;
        }
        fwrite(hidden, 1, hidden_len, fout);
        free(hidden);
        printf("DEBUG: Header byte %d = 0x%02X (%d)\n", i, byte, byte);
    }

    // Буферизованная обработка
    uint8_t* input_buffer = malloc(BUFFER_SIZE);
    uint8_t* output_buffer = malloc(BUFFER_SIZE * 35);
    if (!input_buffer || !output_buffer) {
        free(input_buffer); free(output_buffer); fclose(fin); fclose(fout); PROFILE_END("assoc_stego_encrypt_file"); return -1;
    }

    size_t bytes_read;
    long bytes_encrypted = 0;
    while ((bytes_read = fread(input_buffer, 1, BUFFER_SIZE, fin)) > 0) {
        size_t out_pos = 0;

        for (size_t i = 0; i < bytes_read; i++) {
            // ← Используем быструю версию без malloc!
            size_t hidden_len = 0;
            if (assoc_stego_hide_byte_fast(as, input_buffer[i], output_buffer + out_pos, &hidden_len) == 0) {
                out_pos += hidden_len;
            }
            else {
                free(input_buffer); free(output_buffer); fclose(fin); fclose(fout);
                PROFILE_END("assoc_stego_encrypt_file");
                return -1;
            }
        }

        fwrite(output_buffer, 1, out_pos, fout);
    }

    printf("DEBUG: Successfully encrypted %ld bytes\n", bytes_encrypted);
    free(input_buffer); free(output_buffer);
    fclose(fin); fclose(fout);
    PROFILE_END("assoc_stego_encrypt_file");
    return 0;
}


// ========== ОДНОПОТОЧНОЕ РАСШИФРОВАНИЕ ==========
static int decrypt_file_single(const AssocStego* as, const char* input_path,
    const char* output_path, uint8_t* stego,
    long file_size, size_t block_size, FILE* fout) {
    uint8_t* output_buffer = malloc(file_size);
    if (!output_buffer) { return -1; }

    for (long i = 0; i < file_size; i++) {
        size_t offset = (4 + i) * block_size;
        uint8_t byte = 0;
        int res = assoc_stego_disclose_byte_fast(as, stego + offset, block_size, &byte);
        if (res == 0) {
            output_buffer[i] = byte;
        }
        else {
            printf("ERROR: Failed to decrypt byte %ld\n", i);
            free(output_buffer);
            return -1;
        }
    }

    fwrite(output_buffer, 1, file_size, fout);
    free(output_buffer);
    return 0;
}

// ========== МНОГОПОТОЧНОЕ РАСШИФРОВАНИЕ (OpenMP - ручное) ==========
#ifdef _OPENMP
static int decrypt_file_parallel(const AssocStego* as, const char* input_path,
    const char* output_path, uint8_t* stego,
    long file_size, size_t block_size, FILE* fout,
    int num_threads) {
    if (file_size > 2000000000) {
        printf("ERROR: File too large for OpenMP (%ld bytes)\n", file_size);
        return -1;
    }

    uint8_t* output_buffer = malloc(file_size);
    if (!output_buffer) { return -1; }

    int error_occurred = 0;
    const int file_size_int = (int)file_size;

    // ← Ручное распараллеливание (работает с MSVC OpenMP 2.0)
#ifdef _OPENMP
    #pragma omp parallel num_threads(num_threads)
#endif
    {
        int thread_id = omp_get_thread_num();
        int total_threads = omp_get_num_threads();

        // Разделяем работу между потоками
        int chunk_size = (file_size_int + total_threads - 1) / total_threads;
        int start = thread_id * chunk_size;
        int end = (start + chunk_size < file_size_int) ? (start + chunk_size) : file_size_int;

        for (int i = start; i < end; i++) {
            size_t offset = (4 + i) * block_size;
            uint8_t byte = 0;
            int res = assoc_stego_disclose_byte_fast(as, stego + offset, block_size, &byte);
            if (res == 0) {
                output_buffer[i] = byte;
            }
            else {
	#ifdef _OPENMP
		#pragma omp critical
	#endif
                {
                    printf("ERROR: Failed to decrypt byte %d\n", i);
                    error_occurred = 1;
                }
            }
        }
    }

    if (!error_occurred) {
        fwrite(output_buffer, 1, file_size, fout);
    }

    free(output_buffer);
    return error_occurred ? -1 : 0;
}
#endif

// ========== ОСНОВНАЯ ФУНКЦИЯ ==========
int assoc_stego_decrypt_file_mt(const AssocStego* as, const char* input_path,
    const char* output_path, int num_threads) {
    PROFILE_START("assoc_stego_decrypt_file_mt");

    if (!as || !input_path || !output_path) {
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }
    if (!as->key_generated) {
        printf("ERROR: Key not generated!\n");
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    FILE* fin = fopen(input_path, "rb");
    if (!fin) {
        printf("ERROR: Cannot open stego file!\n");
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    fseek(fin, 0, SEEK_END);
    long stego_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (stego_size <= 0) {
        printf("ERROR: Stego file is empty!\n");
        fclose(fin);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    uint8_t* stego = malloc(stego_size);
    if (!stego) {
        printf("ERROR: Cannot allocate memory!\n");
        fclose(fin);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    if (fread(stego, 1, stego_size, fin) != (size_t)stego_size) {
        printf("ERROR: Cannot read stego file!\n");
        free(stego);
        fclose(fin);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }
    fclose(fin);

    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t block_size = 3 * container_byte_len;

    printf("DEBUG: Block size = %zu\n", block_size);
    printf("DEBUG: Stego file size = %ld bytes\n", stego_size);

    if (stego_size < 4 * block_size) {
        printf("ERROR: Stego file too small!\n");
        free(stego);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    long file_size = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = 0;
        int res = assoc_stego_disclose_byte_fast(as, stego + i * block_size, block_size, &byte);
        if (res != 0) {
            printf("ERROR: Failed to decrypt header byte %d\n", i);
            free(stego);
            PROFILE_END("assoc_stego_decrypt_file_mt");
            return -1;
        }
        file_size |= ((long)byte << (i * 8));
        printf("DEBUG: Header byte %d = 0x%02X (%ld)\n", i, byte, (long)byte);
    }

    printf("DEBUG: Decrypted file size = %ld bytes\n", file_size);

    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {
        printf("ERROR: Invalid file size (%ld bytes)\n", file_size);
        free(stego);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    FILE* fout = fopen(output_path, "wb");
    if (!fout) {
        printf("ERROR: Cannot create output file!\n");
        free(stego);
        PROFILE_END("assoc_stego_decrypt_file_mt");
        return -1;
    }

    // ← Определение числа потоков
#ifdef __ELBRUS__
    printf("DEBUG: Elbrus mode (VLIW + vector, single thread)\n");
    num_threads = 1;
#else
#ifdef _OPENMP
    if (num_threads <= 0) {
        num_threads = omp_get_max_threads();
    }
    printf("DEBUG: x86-64 mode (OpenMP, %d threads)\n", num_threads);
#else
    printf("DEBUG: x86-64 mode (single thread, OpenMP not available)\n");
#endif
#endif

    // ← Вызов соответствующей версии
    int result;
#ifdef _OPENMP
    result = decrypt_file_parallel(as, input_path, output_path, stego,
        file_size, block_size, fout, num_threads);
#else
    result = decrypt_file_single(as, input_path, output_path, stego,
        file_size, block_size, fout);
#endif

    printf("DEBUG: Successfully wrote %ld bytes\n", file_size);

    free(stego);
    fclose(fout);
    PROFILE_END("assoc_stego_decrypt_file_mt");
    return result;
}

// Старая функция для совместимости
int assoc_stego_decrypt_file(const AssocStego* as, const char* input_path,
    const char* output_path) {
    return assoc_stego_decrypt_file_mt(as, input_path, output_path, 0);
}
