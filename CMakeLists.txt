cmake_minimum_required(VERSION 3.10)
project(AssocStego C)

# ========== ОПТИМИЗАЦИИ КОМПИЛЯТОРА ==========
# ========== Оптимизации для x86-64 ==========
if(MSVC)
    add_compile_options(/std:c11 /O2 /Ob2 /Oi /Ot /favor:INTEL64 /arch:AVX2)
    # OpenMP только для x86-64
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Elbrus")
        add_compile_options(/openmp)
    endif()
    add_compile_definitions(NDEBUG)
else()
    set(CMAKE_C_STANDARD 11)
    
    # Проверка на Эльбрус
    if(CMAKE_C_COMPILER_ID MATCHES "Elbrus" OR CMAKE_SYSTEM_PROCESSOR MATCHES "e2k")
        # Эльбрус: VLIW + вектор, без OpenMP
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -ffast-math -funroll-loops")
        add_compile_definitions(__ELBRUS__)
        message(STATUS "Building for Elbrus (VLIW + vector, no OpenMP)")
	message(STATUS "Compiler flags: -Wall -O3 -ffast-math -funroll-loops")
    else()
        # x86-64: AVX2 + OpenMP
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -mavx2 -march=native -funroll-loops -fopenmp")
        message(STATUS "Building for x86-64 (AVX2 + OpenMP)")
    endif()
endif()


# Ядро библиотеки
add_library(assoc_stego_core STATIC src/bitvector.c src/assoc_stego.c )
target_include_directories(assoc_stego_core PUBLIC include)

# Модуль оптимизаций (AVX2)
add_library(assoc_stego_opt STATIC src/assoc_stego_opt.c )
target_include_directories(assoc_stego_opt PUBLIC include)

# Модуль для работы с файлами
add_library(assoc_stego_text STATIC src/assoc_stego_text.c )
target_include_directories(assoc_stego_text PUBLIC include)
target_link_libraries(assoc_stego_text assoc_stego_core)

# Модуль бенчмарков
add_library(benchmark STATIC src/benchmark.c )
target_include_directories(benchmark PUBLIC include)
target_link_libraries(benchmark assoc_stego_text profiler)

# Модуль профилировщика
add_library(profiler STATIC src/profiler.c )
target_include_directories(profiler PUBLIC include)

# Модуль пула буферов
add_library(buffer_pool STATIC src/buffer_pool.c)
target_include_directories(buffer_pool PUBLIC include)

# Модуль векторных операций
add_library(vector_ops STATIC src/vector_ops.c)
target_include_directories(vector_ops PUBLIC include)


# Подключаем ко всем библиотекам
target_link_libraries(assoc_stego_core buffer_pool vector_ops)
target_link_libraries(assoc_stego_text buffer_pool)
# Для Windows (крипто-ГСЧ)
if(WIN32)
    target_link_libraries(assoc_stego_core bcrypt)
endif()

# Тестовое приложение
add_executable(assoc_test src/main.c )
target_link_libraries(assoc_test 
    assoc_stego_core 
    assoc_stego_opt 
    assoc_stego_text 
    benchmark
    profiler
    buffer_pool 
    vector_ops
)
