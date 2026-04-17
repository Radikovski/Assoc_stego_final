#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct {
    const char* name;
    uint64_t start_us;          //  Время начала текущего вызова
    uint64_t total_us;          //  Накопленное время
    uint64_t call_count;
    uint64_t max_us;
    uint64_t min_us;
    int is_active;              //  Флаг активного замера
} ProfileEntry;

#define MAX_PROFILE_ENTRIES 32

typedef struct {
    ProfileEntry entries[MAX_PROFILE_ENTRIES];
    int entry_count;
    int enabled;
} Profiler;

extern Profiler g_profiler;

void profiler_init(void);
void profiler_enable(int enable);
void profiler_start(const char* name);
void profiler_end(const char* name);
void profiler_print_results(void);

#define PROFILE_START(name) profiler_start(name)
#define PROFILE_END(name) profiler_end(name)

#endif