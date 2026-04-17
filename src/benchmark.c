#include "benchmark.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assoc_stego.h>
#include <assoc_stego_text.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <time.h>
#include <unistd.h>
#endif

// Глобальная переменная для частоты таймера (Windows)
#ifdef _WIN32
static LARGE_INTEGER g_frequency = { 0 };
static int g_timer_initialized = 0;
#endif

void benchmark_init(void) {
#ifdef _WIN32
    if (!g_timer_initialized) {
        QueryPerformanceFrequency(&g_frequency);
        g_timer_initialized = 1;
    }
#endif
}

uint64_t benchmark_get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000 / g_frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
#endif
}

void benchmark_get_cpu_info(char* buffer, size_t buffer_size) {
#ifdef _WIN32
    int cpu_info[4];
    char vendor[13] = { 0 };
    char brand[49] = { 0 };

    // Получаем вендора CPU
    __cpuid(cpu_info, 0);
    ((int*)vendor)[0] = cpu_info[1];
    ((int*)vendor)[1] = cpu_info[3];
    ((int*)vendor)[2] = cpu_info[2];

    // Получаем бренд процессора
    __cpuid(cpu_info, 0x80000000);
    unsigned int max_ext = cpu_info[0];

    if (max_ext >= 0x80000004) {
        int brand_info[12];
        __cpuid(brand_info, 0x80000002);
        memcpy(brand, brand_info, 16);
        __cpuid(brand_info, 0x80000003);
        memcpy(brand + 16, brand_info, 16);
        __cpuid(brand_info, 0x80000004);
        memcpy(brand + 32, brand_info, 16);
        brand[48] = '\0';
    }

    // Определяем архитектуру
#if defined(_M_X64) || defined(__x86_64__)
    const char* arch = "x86-64";
#elif defined(_M_IX86) || defined(__i386__)
    const char* arch = "x86-32";
#elif defined(_M_ARM64) || defined(__aarch64__)
    const char* arch = "ARM-64";
#elif defined(_M_ARM) || defined(__arm__)
    const char* arch = "ARM-32";
#else
    const char* arch = "Unknown";
#endif

    snprintf(buffer, buffer_size, "%s %s (%s)", vendor, brand, arch);

    // Удаляем лишние пробелы
    char* src = buffer, * dst = buffer;
    int last_space = 0;
    while (*src) {
        if (*src == ' ') {
            if (!last_space) { *dst++ = *src; last_space = 1; }
        }
        else {
            *dst++ = *src;
            last_space = 0;
        }
        src++;
    }
    *dst = '\0';

#else
    // Linux/Unix
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        char model_name[128] = "Unknown";
        char vendor_id[64] = "Unknown";

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    strncpy(model_name, p, sizeof(model_name) - 1);
                    model_name[sizeof(model_name) - 1] = '\0';
                    size_t len = strlen(model_name);
                    if (len > 0 && model_name[len - 1] == '\n')
                        model_name[len - 1] = '\0';
                }
            }
            if (strncmp(line, "vendor_id", 9) == 0) {
                char* p = strchr(line, ':');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    strncpy(vendor_id, p, sizeof(vendor_id) - 1);
                    vendor_id[sizeof(vendor_id) - 1] = '\0';
                    size_t len = strlen(vendor_id);
                    if (len > 0 && vendor_id[len - 1] == '\n')
                        vendor_id[len - 1] = '\0';
                }
            }
        }
        fclose(f);

        // Определяем архитектуру
#if defined(__x86_64__)
        const char* arch = "x86-64";
#elif defined(__i386__)
        const char* arch = "x86-32";
#elif defined(__aarch64__)
        const char* arch = "ARM-64";
#elif defined(__arm__)
        const char* arch = "ARM-32";
#elif defined(__ELBRUS__)
        const char* arch = "Elbrus";
#else
        const char* arch = "Unknown";
#endif

        snprintf(buffer, buffer_size, "%s %s (%s)", vendor_id, model_name, arch);
    }
    else {
        snprintf(buffer, buffer_size, "Unknown CPU");
    }
#endif
}

BenchmarkResult benchmark_encrypt_text(const void* as, const char* text,
    uint8_t** stego, size_t* stego_len) {
    BenchmarkResult result = { 0 };
    result.input_bytes = strlen(text);

    uint64_t start, end;

    // Шифрование
    start = benchmark_get_time_us();
    int enc_res = assoc_stego_encrypt_text((const AssocStego*)as, text, stego, stego_len);
    end = benchmark_get_time_us();

    if (enc_res != 0 || !*stego) {
        result.encrypt_time_ms = -1;
        return result;
    }

    result.encrypt_time_ms = (end - start) / 1000.0;
    result.output_bytes = *stego_len;

    // Расшифрование
    char* decrypted = NULL;
    start = benchmark_get_time_us();
    int dec_res = assoc_stego_decrypt_text((const AssocStego*)as, *stego, *stego_len, &decrypted);
    end = benchmark_get_time_us();

    if (dec_res != 0 || !decrypted) {
        result.decrypt_time_ms = -1;
    }
    else {
        result.decrypt_time_ms = (end - start) / 1000.0;
        free(decrypted);
    }

    result.total_time_ms = result.encrypt_time_ms + result.decrypt_time_ms;
    result.expansion_ratio = (double)result.output_bytes / result.input_bytes;

    // Пропускная способность (МБ/с)
    if (result.total_time_ms > 0) {
        result.throughput_mbps = (result.input_bytes / 1024.0 / 1024.0) /
            (result.total_time_ms / 1000.0);
    }

    return result;
}

void benchmark_print_results(const BenchmarkResult* result) {
    printf("\n");
    printf("===========================================================\n");
    printf("|              BENCHMARK RESULTS                           \n");
    printf("|==========================================================\n");
    printf("| Input size:        %zu bytes                             \n", result->input_bytes);
    printf("| Output size:       %zu bytes                             \n", result->output_bytes);
    printf("| Expansion ratio:   %.2fx                                 \n", result->expansion_ratio);
    printf("|==========================================================\n");
    printf("| Encrypt time:      %.3f ms                               \n", result->encrypt_time_ms);
    printf("| Decrypt time:      %.3f ms                               \n", result->decrypt_time_ms);
    printf("| Total time:        %.3f ms                               \n", result->total_time_ms);
    printf("|==========================================================\n");
    printf("| Throughput:        %.4f MB/s                             \n", result->throughput_mbps);
    printf("===========================================================\n");
}