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
    "000000000000000000000000111111111111111111111111111111111111111111111111111111",
    "111111110000000000000000000000001111111111111111111000000000000000001000000000",
    "000000000000000011111111111111111000000001111111111111111111000000000000000001",
    "111111111111111111111111000000000000000001000000001111111111000000001000000001",
    "000000001111111100000000000000001111111111111111111000000001111111111000000000",
    "000000001111111100000000111111111111111111000000001111111111111111111000000001",
    "111111111111111100000000111111111111111111000000001000000000000000001111111111",
    "111111110000000000000000000000000000000000000000001111111111000000001111111111",
    "000000001111111100000000111111111111111111111111111111111111111111111111111111",
    "000000001111111111111111000000000000000001111111111111111111111111111000000001"
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
