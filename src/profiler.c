#include "profiler.h"
#include <string.h>
#include <stdlib.h>

Profiler g_profiler = { 0 };

static uint64_t get_time_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
#endif
}

void profiler_init(void) {
    memset(&g_profiler, 0, sizeof(g_profiler));
    g_profiler.enabled = 1;
}

void profiler_enable(int enable) {
    g_profiler.enabled = enable;
}

void profiler_start(const char *name) {
    if (!g_profiler.enabled) return;
    
    for (int i = 0; i < g_profiler.entry_count; i++) {
        if (strcmp(g_profiler.entries[i].name, name) == 0) {
            g_profiler.entries[i].start_us = get_time_us();  //  Просто сохраняем время
            g_profiler.entries[i].is_active = 1;
            return;
        }
    }
    
    if (g_profiler.entry_count < MAX_PROFILE_ENTRIES) {
        int idx = g_profiler.entry_count++;
        g_profiler.entries[idx].name = name;
        g_profiler.entries[idx].start_us = get_time_us();
        g_profiler.entries[idx].total_us = 0;
        g_profiler.entries[idx].call_count = 1;
        g_profiler.entries[idx].max_us = 0;
        g_profiler.entries[idx].min_us = UINT64_MAX;
        g_profiler.entries[idx].is_active = 1;
    }
}

void profiler_end(const char* name) {
    if (!g_profiler.enabled) return;

    uint64_t now = get_time_us();
    for (int i = 0; i < g_profiler.entry_count; i++) {
        if (strcmp(g_profiler.entries[i].name, name) == 0) {
            if (g_profiler.entries[i].is_active) {
                uint64_t elapsed = now - g_profiler.entries[i].start_us;
                // ← Проверка на переполнение
                if (elapsed < 1000000) {  // Игнорировать > 1 секунды (ошибка)
                    g_profiler.entries[i].total_us += elapsed;
                }
                if (elapsed > 0 && elapsed < 1000000 && elapsed > g_profiler.entries[i].max_us)
                    g_profiler.entries[i].max_us = elapsed;
                if (elapsed > 0 && elapsed < 1000000 && elapsed < g_profiler.entries[i].min_us)
                    g_profiler.entries[i].min_us = elapsed;
                g_profiler.entries[i].is_active = 0;
            }
            return;
        }
    }
}

void profiler_print_results(void) {
    if (!g_profiler.enabled) return;
    
    printf("\n");
    printf("============================================================\n");
    printf("|              PROFILER RESULTS                            |\n");
    printf("============================================================\n");
    printf("| %-35s | %-12s | %-10s | %-12s |\n", "Function", "Total ms", "Calls", "Avg us");
    printf("------------------------------------------------------------\n");
    
    // Сортировка по времени
    for (int i = 0; i < g_profiler.entry_count - 1; i++) {
        for (int j = 0; j < g_profiler.entry_count - i - 1; j++) {
            if (g_profiler.entries[j].total_us < g_profiler.entries[j+1].total_us) {
                ProfileEntry temp = g_profiler.entries[j];
                g_profiler.entries[j] = g_profiler.entries[j+1];
                g_profiler.entries[j+1] = temp;
            }
        }
    }
    
    uint64_t total_time = 0;
    for (int i = 0; i < g_profiler.entry_count; i++) {
        total_time += g_profiler.entries[i].total_us;
    }
    
    for (int i = 0; i < g_profiler.entry_count; i++) {
        ProfileEntry *e = &g_profiler.entries[i];
        if (e->total_us == 0) continue;
        
        double percent = (total_time > 0) ? (100.0 * e->total_us / total_time) : 0;
        double avg_us = (e->call_count > 0) ? ((double)e->total_us / e->call_count) : 0;
        
        printf("| %-35s | %10.3f (%5.1f%%) | %10lu | %12.2f |\n", 
               e->name, 
               e->total_us / 1000.0, 
               percent,
               (unsigned long)e->call_count,
               avg_us);
    }
    
    printf("------------------------------------------------------------\n");
    printf("| Total time: %.3f ms                                      |\n", total_time / 1000.0);
    printf("============================================================\n");
}