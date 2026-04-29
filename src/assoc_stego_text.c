#include "assoc_stego_text.h"
#include "profiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <zlib.h>

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
        bytes_encrypted += bytes_read;
    }

    printf("DEBUG: Successfully encrypted %ld bytes\n", bytes_encrypted);
    free(input_buffer); free(output_buffer);
    fclose(fin); fclose(fout);
    PROFILE_END("assoc_stego_encrypt_file");
    return 0;
}

// ==================== МНОГОПОТОЧНОЕ ШИФРОВАНИЕ ====================
int assoc_stego_encrypt_file_mt(const AssocStego* as, const char* input_path, const char* output_path, int num_threads) {
    PROFILE_START("assoc_stego_encrypt_file_mt");

    // 1. Открытие файла и чтение
    long file_size = 0;
    uint8_t* input_data = NULL;

#ifndef _WIN32
    // Нативный быстрый mmap для Эльбруса/Linux
    int fd_in = open(input_path, O_RDONLY);
    if (fd_in < 0) return -1;
    struct stat st;
    fstat(fd_in, &st);
    file_size = st.st_size;
    input_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd_in, 0);
#else
    // Fallback для Windows
    FILE* fin = fopen(input_path, "rb");
    fseek(fin, 0, SEEK_END);
    file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    input_data = malloc(file_size);
    fread(input_data, 1, file_size, fin);
    fclose(fin);
#endif

    // 2. СЖАТИЕ ДАННЫХ (ZLIB)
    uLongf comp_size = compressBound(file_size);
    uint8_t* comp_buf = malloc(comp_size);
    if (compress(comp_buf, &comp_size, input_data, file_size) != Z_OK) {
        printf("ERROR: Ошибка предварительного сжатия zlib!\n");
        return -1;
    }
    printf("DEBUG: Сжатие Zlib завершено. Было %ld байт -> стало %lu байт.\n", file_size, comp_size);

    // 3. Выделение выходного файла (mmap)
    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t block_size = 3 * container_byte_len;
    // Заголовок теперь 8 байт (4 для оригинального размера, 4 для сжатого)
    size_t output_size = (8 + comp_size) * block_size; 

    uint8_t* mapped_out = NULL;
#ifndef _WIN32
    int fd_out = open(output_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    ftruncate(fd_out, output_size); // Растягиваем файл на диске
    mapped_out = mmap(NULL, output_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);
#else
    mapped_out = malloc(output_size);
#endif

    // 4. Шифруем заголовок (последовательно)
    size_t pos = 0; size_t hlen;
    uint32_t orig_sz32 = (uint32_t)file_size;
    uint32_t comp_sz32 = (uint32_t)comp_size;

    for (int i = 0; i < 4; i++) {
        assoc_stego_hide_byte_fast(as, (orig_sz32 >> (i * 8)) & 0xFF, mapped_out + pos, &hlen);
        pos += block_size;
    }
    for (int i = 0; i < 4; i++) {
        assoc_stego_hide_byte_fast(as, (comp_sz32 >> (i * 8)) & 0xFF, mapped_out + pos, &hlen);
        pos += block_size;
    }

    // 5. ПАРАЛЛЕЛЬНОЕ ШИФРОВАНИЕ ДАННЫХ (Элегантный OpenMP)
#ifdef _OPENMP
    if (num_threads <= 0) num_threads = omp_get_max_threads();
    printf("DEBUG: Using parallel encryption with %d threads\n", num_threads);
    #pragma omp parallel for num_threads(num_threads)
#endif
    for (long i = 0; i < (long)comp_size; i++) {
        size_t offset = (8 + i) * block_size;
        size_t temp_len;
        // Каждое ядро пишет строго в свой участок памяти, блокировки не нужны!
        assoc_stego_hide_byte_fast(as, comp_buf[i], mapped_out + offset, &temp_len);
    }

    // 6. Очистка и сброс на диск
#ifndef _WIN32
    munmap(mapped_out, output_size); // Ядро само сбросит данные на диск асинхронно
    close(fd_out);
    munmap(input_data, file_size);
    close(fd_in);
#else
    FILE* fout = fopen(output_path, "wb");
    fwrite(mapped_out, 1, output_size, fout);
    fclose(fout);
    free(mapped_out);
    free(input_data);
#endif
    free(comp_buf);

    PROFILE_END("assoc_stego_encrypt_file_mt");
    return 0;
}

// ==================== МНОГОПОТОЧНОЕ РАСШИФРОВАНИЕ ====================
int assoc_stego_decrypt_file_mt(const AssocStego* as, const char* input_path, const char* output_path, int num_threads) {
    PROFILE_START("assoc_stego_decrypt_file_mt");

    long stego_size = 0;
    uint8_t* stego = NULL;

#ifndef _WIN32
    int fd_in = open(input_path, O_RDONLY);
    struct stat st;
    fstat(fd_in, &st);
    stego_size = st.st_size;
    stego = mmap(NULL, stego_size, PROT_READ, MAP_PRIVATE, fd_in, 0);
#else
    FILE* fin = fopen(input_path, "rb");
    fseek(fin, 0, SEEK_END);
    stego_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    stego = malloc(stego_size);
    fread(stego, 1, stego_size, fin);
    fclose(fin);
#endif

    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t block_size = 3 * container_byte_len;

    // Читаем заголовок (8 байт)
    uint32_t orig_sz32 = 0; uint32_t comp_sz32 = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = 0;
        assoc_stego_disclose_byte_fast(as, stego + i * block_size, block_size, &byte);
        orig_sz32 |= ((uint32_t)byte << (i * 8));
    }
    for (int i = 0; i < 4; i++) {
        uint8_t byte = 0;
        assoc_stego_disclose_byte_fast(as, stego + (4 + i) * block_size, block_size, &byte);
        comp_sz32 |= ((uint32_t)byte << (i * 8));
    }

    uint8_t* comp_buf = malloc(comp_sz32);

    // ПАРАЛЛЕЛЬНОЕ РАСШИФРОВАНИЕ
#ifdef _OPENMP
    if (num_threads <= 0) num_threads = omp_get_max_threads();
    #pragma omp parallel for num_threads(num_threads)
#endif
    for (long i = 0; i < (long)comp_sz32; i++) {
        size_t offset = (8 + i) * block_size;
        assoc_stego_disclose_byte_fast(as, stego + offset, block_size, &comp_buf[i]);
    }

    // ДЕКОМПРЕССИЯ (ZLIB)
    uLongf final_size = orig_sz32;
    uint8_t* final_data = malloc(final_size);
    uncompress(final_data, &final_size, comp_buf, comp_sz32);

    // Запись на диск (файл маленький, обычный fwrite отработает моментально)
    FILE* fout = fopen(output_path, "wb");
    fwrite(final_data, 1, final_size, fout);
    fclose(fout);

    free(comp_buf);
    free(final_data);
#ifndef _WIN32
    munmap(stego, stego_size);
    close(fd_in);
#else
    free(stego);
#endif

    PROFILE_END("assoc_stego_decrypt_file_mt");
    return 0;
}

// Старая функция для совместимости
int assoc_stego_decrypt_file(const AssocStego* as, const char* input_path,
    const char* output_path) {
    return assoc_stego_decrypt_file_mt(as, input_path, output_path, 0);
}
