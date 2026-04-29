#ifndef ASSOC_STEGO_TEXT_H
#define ASSOC_STEGO_TEXT_H

#include "assoc_stego.h"
#include <stddef.h>

// Шифрование строки (UTF-8) в стего-данные
int assoc_stego_encrypt_text(const AssocStego* as, const char* text,
    uint8_t** out_stego, size_t* out_len);

// Расшифрование стего-данных в строку (UTF-8)
int assoc_stego_decrypt_text(const AssocStego* as, const uint8_t* stego,
    size_t stego_len, char** out_text);

// Шифрование файла
int assoc_stego_encrypt_file(const AssocStego* as, const char* input_path,
    const char* output_path);

// Шифрование файла (Многопоточное)
int assoc_stego_encrypt_file_mt(const AssocStego *as, const char *input_path,const char *output_path, int num_threads);
// Расшифрование файла
int assoc_stego_decrypt_file(const AssocStego* as, const char* input_path,
    const char* output_path);
// Расшифрование файла (многопоточное)
int assoc_stego_decrypt_file_mt(const AssocStego* as, const char* input_path,
    const char* output_path, int num_threads);

#endif // ASSOC_STEGO_TEXT_H
