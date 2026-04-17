#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdint.h>
#include <stddef.h>

// Структура для хранения результатов бенчмарка
typedef struct {
    double encrypt_time_ms;      // Время шифрования (мс)
    double decrypt_time_ms;      // Время расшифрования (мс)
    double total_time_ms;        // Общее время
    size_t input_bytes;          // Входные данные (байт)
    size_t output_bytes;         // Выходные данные (байт)
    double throughput_mbps;      // Пропускная способность (МБ/с)
    double expansion_ratio;      // Коэффициент расширения
} BenchmarkResult;

// Инициализация таймера
void benchmark_init(void);

// Получение текущего времени в микросекундах
uint64_t benchmark_get_time_us(void);

// Получение информации о CPU
void benchmark_get_cpu_info(char* buffer, size_t buffer_size);

// Запуск бенчмарка для текста
BenchmarkResult benchmark_encrypt_text(const void* as, const char* text,
    uint8_t** stego, size_t* stego_len);

// Форматированный вывод результатов
void benchmark_print_results(const BenchmarkResult* result);

#endif // BENCHMARK_H