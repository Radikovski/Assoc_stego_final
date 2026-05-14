#include <stdio.h>
#include <stdlib.h>
#include "assoc_stego.h"
#include "assoc_stego_text.h"
#include <locale.h>
#include "benchmark.h"
#include "profiler.h"
#include <omp.h>
#include <string.h>

#ifdef NDEBUG
#pragma message("Building in RELEASE mode with optimizations")
#else
#pragma message("Building in DEBUG mode - consider switching to Release!")
#endif

// ... остальной код main.c без изменений ...
#ifdef _WIN32
    #include <windows.h>
#endif
//82 битные эталоны
static const char* ETALONS[] = {
    "000000000000000000000000111111111111111111111111111111111111111111111111111111011101000001111101001010110010100101001001011000010101101011010011001010111101100001011101010000000110011000100111101001110100101011100011100110101011011001001101100100001011011001010001000010111010100110010000011011100111110111111001001011101011100100100000010001001101110111100100011101100110100011101100100110010100110100101100010001011100001010100101000101011110101000010110001101001010101110000100111110100011011100100100010000011001100000100100",
    "111111110000000000000000000000001111111111111111111000000000000000001000000000100001001010001010001100101010111001101001001010000001101101111111111011001000110010010011101001011011000111011100101100001011010110000000101100000111100010110001010101001100101110010110101010101010000001010110011000100010101110000000001101110000010001110100101100010010001001001010100001101111101101110010101001010111011100000011011000000001010001000101010110101100011100101101000011101101000001100111110100100010010000100011011011111000011001110010",
    "000000000000000011111111111111111000000001111111111111111111000000000000000001111111100001111110001011100011110101100000111111101111011101101000111011001101111000000110010010101101110011011000010010110010100101101000110000011010011011010000111110011011001010010011101111101101100010001100111001011000011000010110010000000100010101011001010000010000011001110101010011010101111001101000100000101011010110110011011111101001111101001110100100101111101011110100110100011101100010010100001011011101100010010101001010010100101001110100",
    "111111111111111111111111000000000000000001000000001111111111000000001000000001101011011000011001001010100011100011000001011111011011101000110111010111111010100001011000011110010111000100111010101111010100101101001001110101011111110111100011000101101001101010100101011111111111111111011111001110000000100011110101110001011100100000100101011010001010010011100110000110100111011101011001001010000011100010111101100010100110011011111101001010110010001010101100111100110011110000010001010110111011110011000101000111100010011000111000",
    "000000001111111100000000000000001111111111111111111000000001111111111000000000010010101010000000111001010100001000001110110111010101110111100011101000001100111000100010001000011100100101011101010111000000001100010111101010100000000001101010010011001001101110110110010010001110111010111000011111011100101000111110111000110000110111111111001111001110100000110100100110111100011001101000001111110110000010110001110000111001000000011110100010110011010100101101001010111000011110100000010010011101101001011111010100010011000010001001",
    "000000001111111100000000111111111111111111000000001111111111111111111000000001001110010001111000001100001100111110110111110111001111100000010111110000011001000010110111010000011010001110100111100010010000101010000011000011110101111000110010110000100010001000111101110111100000101101101011010110001100001000001001111110000000101000100110101010010101001011001001100000011011000011010010101011010000111011001111011010001011001001111110110000100100010001010100010000001110011111001101001000110111001110110101101100011110111111000111",
    "111111111111111100000000111111111111111111000000001000000000000000001111111111101001100110000101011011111111110011001111111100110011110001000010011000110111110111001100010101001011101011100010010011000000111101010111110111110111010011111110111001000001110010101100110001010101011111110101111011100111110110011000000011100010001010101110011000111000100101010001010111011110101110101011011110101001100110101011111100000011111110001000000000010110111111111101011000001010001010001100101001101111110000000011010110111101111000100010",
    "111111110000000000000000000000000000000000000000001111111111000000001111111111101101011110000010010000111111000011010100110010000110100111111010000111001000100111100001110100101000001100001010111000101011101001010000100011011100111101111011010111011000110110001111010000100101010110000000010111100111100000000110101110101001011000001011010011101001000111011001010001011101000111101101100010001000100101010100001010001001001010010100100111000101001001011111010100100001010010110001000100111000111111001010010101001110011100010011",
    "000000001111111100000000111111111111111111111111111111111111111111111111111111000111001110111101110100001000011101000001101000101100011110110110000001101111101101100010001000001100110010000110111111111111111110101111010001010100000100100001100100000011101110001010100110110000001010110100011000111101110010101110001101100001100100000101100010100001000100101001111001110000100011010110001111001011110111010011101101111101000110001100011111100100101110111001101100101011000010001000001100001000101111110000001010110111001100111110",
    "000000001111111111111111000000000000000001111111111111111111111111111000000001100100111010101011010110100101100111111101111101010010110001000001111011101111100110010001100011011000010010010010111100010011011011010100010001111100110101010010011011000111111111110111111010010111101111111101001101011010101110010010010010011010001010001010001001111111001000010010101111100001100001101011111011111001000100100000001000101100100111111100110010011001011011111001011110110000000000111011000110100000000000001100100100011000010101100011"
};


/* Вспомогательная функция для отладки длин эталонов */
static void print_etalon_lengths(const char** etalons, size_t count) 
{
    printf("Etalon lengths:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu]: %zu bits\n", i, strlen(etalons[i]));
    }
}

int main(void) {
    const size_t ETALON_COUNT = 10;

    // Инициализация
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    setlocale(LC_ALL, "ru_RU.UTF-8");
    benchmark_init();
    profiler_init();
    // Показываем информацию о потоках
    
    #ifdef _OPENMP
    printf("| Threads: %d (max: %d)                              |\n", 
           omp_get_num_threads(), omp_get_max_threads());
    #else

    //printf("| Threads: 1 (OpenMP not available)                    |\n");
    #endif
    //ввод данных , если не текст 
    char input_path[256];
    const char* default_path = "test_plain.txt";

    
    printf("file to encrypt and decrypt ( Enter for '%s'): ", default_path);

    
    if (fgets(input_path, sizeof(input_path), stdin) != NULL) {
       
        input_path[strcspn(input_path, "\r\n")] = 0;

       
        if (strlen(input_path) == 0) {
            strcpy(input_path, default_path);
        }
    }
    else {
        
        strcpy(input_path, default_path);
    }

    char stego_path[256];
    char decrypt_path[256];

    // Функция sprintf склеивает строки
    
    sprintf(stego_path, "%s.stego.bin", input_path);

    
    sprintf(decrypt_path, "%s.decrypted", input_path);
    // 
    // Информация о системе
    // 
    printf("\n");
    printf("============================================================\n");
    printf("|         ASSOCIATIVE STEGO BENCHMARK                      \n");
    printf("============================================================\n");

    char cpu_info[256];
    benchmark_get_cpu_info(cpu_info, sizeof(cpu_info));
    printf("| CPU: %-54s \n", cpu_info);

#ifdef _WIN32
    printf("| OS:  Windows                                              \n");
#elif defined(__linux__)
    printf("| OS:  Linux                                                \n");
#elif defined(__APPLE__)
    printf("| OS:  macOS                                               \n");
#else
    printf("| OS:  Unknown                                             \n");
#endif

#ifdef __ELBRUS__
    printf("| Architecture: Elbrus (VLIW)                              \n");
#elif defined(_M_X64) || defined(__x86_64__)
    printf("| Architecture: x86-64                                     \n");
#elif defined(__aarch64__)
    printf("| Architecture: ARM-64                                     \n");
#else
    printf("| Architecture: Unknown                                    \n");
#endif

    printf("| Etalons: %zu, Length: %zu bits                            \n", ETALON_COUNT, strlen(ETALONS[0]));
    printf("|===========================================================\n");

    // Создание структуры
    printf("\n[INIT] Creating AssocStego structure...\n");
    AssocStego* as = assoc_stego_create(ETALONS, ETALON_COUNT);
    printf("[INIT] Creating buffer pools...\n");
    if (!as) {
        printf("ERROR: Create failed\n");
        return 1;
    }
    printf("       OK: Buffer pools initialized\n");
    printf("       OK: Etalon length = %zu, Count = %zu\n", as->etalon_length, as->etalon_count);

    printf("[INIT] Generating key...\n");
    if (assoc_stego_create_key(as) != 0) {
        printf("ERROR: Key gen failed\n");
        assoc_stego_free(as);
        return 1;
    }
    printf("       OK: Key generated\n");

    // 
    // ТЕСТ: Шифрование и расшифрование файла
    // 
    printf("\n");
    printf("============================================================\n");
    printf("|              FILE ENCRYPTION TEST                        \n");
    printf("============================================================\n");
    
    FILE* fcheck = fopen(input_path, "rb");
    if (!fcheck) {
        printf("\n[ERROR] File '%s' not found!\n", input_path);
        printf("        Please create this file with your text (UTF-8 encoding).\n");
        printf("        Location: %s\n", "working directory of assoc_test.exe");
        assoc_stego_free(as);
        printf("\nPress Enter to close...");
        getchar();
        return 1;
    }

    // Получаем размер файла
    int64_t fsize = 0;
#ifdef _WIN32
    _fseeki64(fcheck, 0, SEEK_END);
    fsize = _ftelli64(fcheck);
#else
    fseeko(fcheck, 0, SEEK_END);
    fsize = ftello(fcheck);
#endif
    rewind(fcheck);

    printf("\n[INPUT] File: %s\n",input_path);
    printf("        Size: %lld bytes\n", fsize);

    // Читаем содержимое для отображения (если файл небольшой)
    if (fsize < 1024) {
        FILE* f = fopen(input_path, "rb");
        if (f) {
            char* content = (char*)malloc(fsize + 1);
            if (content) {
                fread(content, 1, fsize, f);
                content[fsize] = '\0';
                printf("        Content: \"%s\"\n", content);
                free(content);
            }
            fclose(f);
        }
    }

    // 
    // Шифрование
    // 
    printf("\n[ENCRYPT] %s -> %s\n", input_path ,stego_path );
    uint64_t enc_start = benchmark_get_time_us();
    //int enc_res = assoc_stego_encrypt_file(as, "test_plain.txt", "test_stego.bin");//Однопоточное шифрование 
    int enc_res = assoc_stego_encrypt_file_mt(as, input_path, stego_path, 0);//Многопоточное шифрование 
    uint64_t enc_end = benchmark_get_time_us();
    double enc_time = (enc_end - enc_start) / 1000.0;

    if (enc_res != 0) {
        printf("[ERROR] Encryption failed (code %d)\n", enc_res);
        assoc_stego_free(as);
        printf("\nPress Enter to close...");
        getchar();
        return 1;
    }

    // Получаем размер зашифрованного файла
    FILE* f_stego = fopen(stego_path, "rb");
    int64_t stego_size = 0;
    if (f_stego) {
#ifdef _WIN32
        _fseeki64(f_stego, 0, SEEK_END);
        stego_size = _ftelli64(f_stego);
#else
        fseeko(f_stego, 0, SEEK_END);
        stego_size = ftello(f_stego);
#endif
        fclose(f_stego);
    }

    printf("          Status: SUCCESS\n");
    printf("          Time:   %.3f ms\n", enc_time);
    printf("          Output: %lld bytes (%.2fx expansion)\n", stego_size, (double)stego_size / fsize);

    // 
    // Расшифрование
    // 
    printf("\n[DECRYPT] %s -> %s\n", stego_path, decrypt_path);
    uint64_t dec_start = benchmark_get_time_us();
    int use_parallel = 1;  // 1 = РїР°СЂР°Р»Р»РµР»СЊРЅРѕ, 0 = РїРѕСЃР»РµРґРѕРІР°С‚РµР»СЊРЅРѕ
    int num_threads = omp_get_max_threads();
    int dec_res;
    if (use_parallel) {
        printf("DEBUG: Using parallel decryption with %d threads\n", num_threads);
        dec_res = assoc_stego_decrypt_file_mt(as, stego_path, decrypt_path, 0);
        if (dec_res != 0) {
            printf("[ERROR] Decryption failed (code %d)\n",dec_res);
            return 1;
        }
    }
    else {
	dec_res = assoc_stego_decrypt_file(as, stego_path, decrypt_path);
    }
    uint64_t dec_end = benchmark_get_time_us();
    double dec_time = (dec_end - dec_start) / 1000.0;

    if (dec_res != 0) {
        printf("[ERROR] Decryption failed (code %d)\n", dec_res);
        assoc_stego_free(as);
        printf("\nPress Enter to close...");
        getchar();
        return 1;
    }

    printf("          Status: SUCCESS\n");
    printf("          Time:   %.3f ms\n", dec_time);

    // 
    // Верификация файлов
    // 
    printf("\n[VERIFY] Comparing %s and %s\n",input_path,decrypt_path);

    FILE* f1 = fopen(input_path, "rb");
    FILE* f2 = fopen(decrypt_path, "rb");

    int verification_ok = 0;

    if (f1 && f2) {
        fseek(f1, 0, SEEK_END); size_t s1 = ftell(f1); rewind(f1);
        fseek(f2, 0, SEEK_END); size_t s2 = ftell(f2); rewind(f2);

        if (s1 != s2) {
            printf("          Status: FAIL (Sizes differ: %lld vs %lld)\n", s1, s2);
        }
        else {
            int match = 1;
            for (long i = 0; i < s1; i++) {
                if (fgetc(f1) != fgetc(f2)) {
                    match = 0;
                    printf("          Mismatch at byte %ld\n", i);
                    break;
                }
            }

            if (match) {
                printf("          Status: SUCCESS (All %lld bytes match)\n", s1);
                verification_ok = 1;
            }
            else {
                printf("          Status: FAIL (Content mismatch)\n");
            }
        }
        fclose(f1);
        fclose(f2);
    }
    else {
        printf("          Status: FAIL (Cannot open files for comparison)\n");
    }

    // 
    // Итоговые результаты бенчмарка
    // 
    double total_time = enc_time + dec_time;
    double throughput = (fsize / 1024.0 / 1024.0) / (total_time / 1000.0);

    printf("\n");
    printf("===========================================================\n");
    printf("|              BENCHMARK RESULTS                           \n");
    printf("|==========================================================\n");
    printf("| Input size:        %lld bytes                             \n", fsize);
    printf("| Output size:       %lld bytes                             \n", stego_size);
    printf("| Expansion ratio:   %.2fx                                 \n", (double)stego_size / fsize);
    printf("|==========================================================\n");
    printf("| Encrypt time:      %.3f ms                               \n", enc_time);
    printf("| Decrypt time:      %.3f ms                               \n", dec_time);
    printf("| Total time:        %.3f ms                               \n", total_time);
    printf("|==========================================================\n");
    printf("| Throughput:        %.6f MB/s                             \n", throughput);
    printf("| Verification:      %s                                    \n", verification_ok ? "SUCCESS" : "FAIL");
    printf("|==========================================================\n");

    // 
    // Завершение
    // 
    assoc_stego_free(as);

    printf("\n[COMPLETE] Test finished successfully!\n");
    profiler_print_results();
    printf("\nPress Enter to close...");
    getchar();
    return 0;
}
