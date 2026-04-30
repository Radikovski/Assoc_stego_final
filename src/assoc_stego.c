#include "assoc_stego.h"
#include "assoc_stego_opt.h"
#include "profiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "vector_ops.h"
#define MAX_WORDS 16

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#endif

// ========== Инициализация RNG ==========
static void init_rng(void) {
#ifdef _WIN32
    uint64_t seed;
    BCryptGenRandom(NULL, (PUCHAR)&seed, sizeof(seed), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    uint64_t seed = 0x123456789ABCDEF0ULL;
    if (fd >= 0) { read(fd, &seed, sizeof(seed)); close(fd); }
#endif
    bitvector_rng_seed(seed);
}

// ========== Вспомогательные функции ==========
static uint32_t crypto_rand(uint32_t max) {
    uint64_t r;
#ifdef _WIN32
    BCryptGenRandom(NULL, (PUCHAR)&r, sizeof(r), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) { read(fd, &r, sizeof(r)); close(fd); }
    else r = (uint64_t)rand();
#endif
    return (uint32_t)(r % max);
}

static void shuffle(int* arr, size_t n) {
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = crypto_rand((uint32_t)(i + 1));
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

// ========== Создание/Удаление ==========
AssocStego* assoc_stego_create(const char** strings, size_t count) {
    PROFILE_START("assoc_stego_create");
    if (!strings || count == 0 || count > MAX_ETALONS) { PROFILE_END("assoc_stego_create"); return NULL; }
    if (!strings[0]) { PROFILE_END("assoc_stego_create"); return NULL; }

    size_t len = strlen(strings[0]);
    if (len == 0) { PROFILE_END("assoc_stego_create"); return NULL; }

    for (size_t i = 1; i < count; i++) {
        if (!strings[i]) { PROFILE_END("assoc_stego_create"); return NULL; }
        if (strlen(strings[i]) != len) { PROFILE_END("assoc_stego_create"); return NULL; }
    }

    AssocStego* as = calloc(1, sizeof(AssocStego));
    if (!as) { PROFILE_END("assoc_stego_create"); return NULL; }

    as->etalon_count = count;
    as->etalon_length = len;
    as->etalons = calloc(count, sizeof(BitVector*));
    as->key = calloc(count, sizeof(BitVector*));
    as->cache = calloc(count, sizeof(EtalonCache));
    as->mask_unit_positions = calloc(count, sizeof(int*));
    as->mask_unit_counts = calloc(count, sizeof(size_t));

    if (!as->etalons || !as->key || !as->cache) { assoc_stego_free(as); PROFILE_END("assoc_stego_create"); return NULL; }

    for (size_t i = 0; i < count; i++) {
        as->mask_unit_positions[i] = NULL;
        as->mask_unit_counts[i] = 0;
        as->etalons[i] = bitvector_from_string(strings[i]);
        if (!as->etalons[i]) { assoc_stego_free(as); PROFILE_END("assoc_stego_create"); return NULL; }
    }

    init_rng();

    size_t container_size = (len + 7) / 8;
    as->byte_pool = pool_create(128, 100);  // 100 буферов по 128 байт
    as->container_pool = pool_create(container_size * 3, 50);  // 50 контейнеров
    PROFILE_END("assoc_stego_create");
    return as;
}

void assoc_stego_free(AssocStego* as) {
    if (!as) return;
    if (as->etalons) { for (size_t i = 0; i < as->etalon_count; i++) if (as->etalons[i]) bitvector_free(as->etalons[i]); free(as->etalons); }
    if (as->key) { for (size_t i = 0; i < as->etalon_count; i++) if (as->key[i]) bitvector_free(as->key[i]); free(as->key); }
    if (as->cache) { for (size_t i = 0; i < as->etalon_count; i++) if (as->cache[i].etalon_masked) free(as->cache[i].etalon_masked); free(as->cache); }
    if (as->mask_unit_positions) { for (size_t i = 0; i < as->etalon_count; i++) if (as->mask_unit_positions[i]) free(as->mask_unit_positions[i]); free(as->mask_unit_positions); }
    if (as->mask_unit_counts) free(as->mask_unit_counts);
    if (as->byte_pool) pool_destroy(as->byte_pool);
    if (as->container_pool) pool_destroy(as->container_pool);
    free(as);
}

// ========== Генерация ключа ==========
static void reset_masks(AssocStego* as) {
    for (size_t i = 0; i < as->etalon_count; i++) {
        if (as->mask_unit_positions[i]) free(as->mask_unit_positions[i]);
        as->mask_unit_positions[i] = NULL;
        as->mask_unit_counts[i] = 0;
    }
}

static void add_mask_pos(AssocStego* as, size_t idx, int pos) {
    size_t new_c = as->mask_unit_counts[idx] + 1;
    int* new_p = realloc(as->mask_unit_positions[idx], new_c * sizeof(int));
    if (new_p) {
        as->mask_unit_positions[idx] = new_p;
        as->mask_unit_positions[idx][as->mask_unit_counts[idx]++] = pos;
    }
}

static void gen_key_rec(AssocStego* as, int* indices, size_t count, bool first, bool reset) {
    if (reset) reset_masks(as);
    if (!indices || count < 2) return;

    int* work = malloc(count * sizeof(int));
    if (!work) return;
    memcpy(work, indices, count * sizeof(int));
    if (first) shuffle(work, count);

    int* d1 = malloc(count * sizeof(int));
    if (!d1) { free(work); return; }
    size_t d1_cnt = 1;
    d1[0] = work[0];

    BitVector* a0 = bitvector_xor(as->etalons[work[0]], as->etalons[work[1]]);
    if (!a0) { free(work); free(d1); return; }

    for (size_t i = 0; i < count - 2; i++) {
        BitVector* a1 = bitvector_xor(as->etalons[work[0]], as->etalons[work[i + 2]]);
        if (!a1) continue;
        BitVector* a3 = bitvector_and(a0, a1);

        if (a3 && bitvector_is_zero(a3)) {
            d1[d1_cnt++] = work[i + 2];
        }
        else if (a3) {
            bitvector_free(a0);
            a0 = a3;
        }
        bitvector_free(a1);
        if (a3 && a3 != a0) bitvector_free(a3);
    }

    size_t ones_cnt = 0;
    int* ones = bitvector_get_set_bit_positions(a0, &ones_cnt);
    if (ones_cnt > 0 && ones) {
        int rpos = ones[crypto_rand((uint32_t)ones_cnt)];
        for (size_t i = 0; i < count; i++) add_mask_pos(as, (size_t)work[i], rpos);
        free(ones);
    }
    bitvector_free(a0);

    size_t d2_cnt = 0;
    int* d2 = malloc(count * sizeof(int));
    if (!d2) { free(work); free(d1); return; }

    for (size_t i = 0; i < count; i++) {
        bool in_d1 = false;
        for (size_t j = 0; j < d1_cnt; j++) if (work[i] == d1[j]) { in_d1 = true; break; }
        if (!in_d1) d2[d2_cnt++] = work[i];
    }

    if (d1_cnt >= 2) gen_key_rec(as, d1, d1_cnt, true, false);
    if (d2_cnt >= 2) gen_key_rec(as, d2, d2_cnt, true, false);

    free(work); free(d1); free(d2);
}

int assoc_stego_create_key(AssocStego* as) {
    PROFILE_START("assoc_stego_create_key");
    if (!as) { PROFILE_END("assoc_stego_create_key"); return -1; }
    int* idx = malloc(as->etalon_count * sizeof(int));
    if (!idx) { PROFILE_END("assoc_stego_create_key"); return -1; }
    for (size_t i = 0; i < as->etalon_count; i++) idx[i] = (int)i;

    gen_key_rec(as, idx, as->etalon_count, true, true);
    free(idx);

    for (size_t i = 0; i < as->etalon_count; i++) {
        as->key[i] = bitvector_create_with_bits(as->etalon_length, as->mask_unit_positions[i], as->mask_unit_counts[i]);
        if (!as->key[i]) { PROFILE_END("assoc_stego_create_key"); return -1; }

        // КЭШИРОВАНИЕ
        as->cache[i].etalon_masked = (uint64_t*)calloc(as->etalons[i]->word_count, sizeof(uint64_t));
        if (!as->cache[i].etalon_masked) { PROFILE_END("assoc_stego_create_key"); return -1; }
        vector_and(as->etalons[i]->data, as->key[i]->data, as->cache[i].etalon_masked, as->etalons[i]->word_count);
    }
    as->key_generated = true;
    PROFILE_END("assoc_stego_create_key");
    return 0;
}

// ========== Загрузка ключа ==========
#define KEY_MAGIC 0x41534B59
int assoc_stego_load_key(AssocStego* as, const char* path) {
    PROFILE_START("assoc_stego_load_key");
    if (!as) { PROFILE_END("assoc_stego_load_key"); return -1; }
    FILE* f = fopen(path, "rb");
    if (!f) { PROFILE_END("assoc_stego_load_key"); return -1; }
    uint32_t magic, kc, bl;
    if (fread(&magic, 4, 1, f) != 1 || magic != KEY_MAGIC) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }
    if (fread(&kc, 4, 1, f) != 1 || fread(&bl, 4, 1, f) != 1) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }
    if (bl != as->etalon_length || kc != as->etalon_count) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }

    for (size_t i = 0; i < as->etalon_count; i++) {
        if (as->key[i]) bitvector_free(as->key[i]);
        if (as->cache[i].etalon_masked) free(as->cache[i].etalon_masked);
    }

    for (size_t i = 0; i < as->etalon_count; i++) {
        uint32_t len;
        if (fread(&len, 4, 1, f) != 1) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }
        uint8_t* buf = malloc(len);
        if (!buf || fread(buf, 1, len, f) != len) { if (buf)free(buf); fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }
        as->key[i] = bitvector_from_bytes(buf, len, as->etalon_length);
        free(buf);
        if (!as->key[i]) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }

        // КЭШИРОВАНИЕ
        as->cache[i].etalon_masked = (uint64_t*)calloc(as->etalons[i]->word_count, sizeof(uint64_t));
        if (!as->cache[i].etalon_masked) { fclose(f); PROFILE_END("assoc_stego_load_key"); return -1; }
        vector_and(as->etalons[i]->data, as->key[i]->data, as->cache[i].etalon_masked, as->etalons[i]->word_count);
    }
    fclose(f); as->key_generated = true; PROFILE_END("assoc_stego_load_key"); return 0;
}

int assoc_stego_save_key(const AssocStego* as, const char* path) {
    if (!as || !as->key_generated) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t magic = KEY_MAGIC, kc = (uint32_t)as->etalon_count, bl = (uint32_t)as->etalon_length;
    fwrite(&magic, 4, 1, f); fwrite(&kc, 4, 1, f); fwrite(&bl, 4, 1, f);
    for (size_t i = 0; i < as->etalon_count; i++) {
        size_t blen;
        uint8_t* b = bitvector_to_bytes(as->key[i], &blen);
        uint32_t len32 = (uint32_t)blen;
        fwrite(&len32, 4, 1, f); fwrite(b, 1, blen, f);
        free(b);
    }
    fclose(f); return 0;
}

// ========== Скрытие эталона ==========
BitVector* assoc_stego_hide_etalon(const AssocStego* as, int idx) {
    PROFILE_START("assoc_stego_hide_etalon");
    if (!as || !as->key_generated || idx < 0 || (size_t)idx >= as->etalon_count) { PROFILE_END("assoc_stego_hide_etalon"); return NULL; }

    BitVector* c = assoc_stego_generate_container(as->etalon_length);
    if (!c) { PROFILE_END("assoc_stego_hide_etalon"); return NULL; }

    uint64_t* xe = (uint64_t*)calloc(c->word_count, sizeof(uint64_t));
    uint64_t* m = (uint64_t*)calloc(c->word_count, sizeof(uint64_t));
    if (!xe || !m) { bitvector_free(c); free(xe); free(m); PROFILE_END("assoc_stego_hide_etalon"); return NULL; }

    // Используем AVX2 версии если доступны
#if defined(_M_X64) || defined(__x86_64__)
    bitvector_xor_avx2(xe, c->data, as->etalons[idx]->data, c->word_count);
    bitvector_and_avx2(m, xe, as->key[idx]->data, c->word_count);
    bitvector_xor_avx2(c->data, c->data, m, c->word_count);
#else
    vector_xor(c->data, as->etalons[idx]->data, xe, c->word_count);
    vector_and(xe, as->key[idx]->data, m, c->word_count);
    vector_xor(c->data, m, c->data, c->word_count);
#endif

    free(xe); free(m);
    PROFILE_END("assoc_stego_hide_etalon");
    return c;
}

// ========== Расшифрование с кэшем ==========
int assoc_stego_disclose_etalon_cached(const AssocStego* as, const uint64_t* container_data) {
    PROFILE_START("assoc_stego_disclose_etalon_cached");
    if (!as || !as->key_generated || !container_data) { PROFILE_END("assoc_stego_disclose_etalon_cached"); return -1; }

    uint64_t* cm = (uint64_t*)calloc(as->etalons[0]->word_count, sizeof(uint64_t));
    if (!cm) { PROFILE_END("assoc_stego_disclose_etalon_cached"); return -1; }

    for (size_t i = 0; i < as->etalon_count; i++) {
#if defined(_M_X64) || defined(__x86_64__)
        bitvector_and_avx2(cm, container_data, as->key[i]->data, as->etalons[i]->word_count);
#else
        vector_and(container_data, as->key[i]->data, cm, as->etalons[i]->word_count);
#endif

        bool match = true;
        for (size_t j = 0; j < as->etalons[i]->word_count; j++) {
            if (cm[j] != as->cache[i].etalon_masked[j]) { match = false; break; }
        }
        if (match) { free(cm); PROFILE_END("assoc_stego_disclose_etalon_cached"); return (int)i; }
    }

    free(cm);
    PROFILE_END("assoc_stego_disclose_etalon_cached");
    return -1;
}

BitVector* assoc_stego_generate_container(size_t len) {
    size_t blen = (len + 7) / 8;
    uint8_t* buf = malloc(blen);
    if (!buf) return NULL;
    bitvector_rng_fill(buf, blen);
    BitVector* bv = bitvector_from_bytes(buf, blen, len);
    free(buf); return bv;
}

// ========== Шифрование/Расшифрование байта ==========
int assoc_stego_hide_byte(const AssocStego* as, uint8_t val, uint8_t** out, size_t* out_len) {
    PROFILE_START("assoc_stego_hide_byte");
    if (!as || !out || !out_len) { PROFILE_END("assoc_stego_hide_byte"); return -1; }
    int d[] = { val / 100, (val % 100) / 10, val % 10 };
    BitVector* c[3];
    for (int i = 0; i < 3; i++) 
    { 
        c[i] = assoc_stego_hide_etalon(as, d[i]); if (!c[i]) goto err; 
    }

    size_t lens[3]; uint8_t* bytes[3];
    for (int i = 0; i < 3; i++) { bytes[i] = bitvector_to_bytes(c[i], &lens[i]); bitvector_free(c[i]); if (!bytes[i]) goto err_bytes; }

    size_t total = lens[0] + lens[1] + lens[2];
    *out = malloc(total); if (!*out) goto err_bytes;
    memcpy(*out, bytes[0], lens[0]);
    memcpy(*out + lens[0], bytes[1], lens[1]);
    memcpy(*out + lens[0] + lens[1], bytes[2], lens[2]);
    *out_len = total;
    for (int i = 0; i < 3; i++) free(bytes[i]);
    PROFILE_END("assoc_stego_hide_byte");
    return 0;
err_bytes:
    for (int i = 0; i < 3; i++) if (bytes[i]) free(bytes[i]);
err:
    for (int i = 0; i < 3; i++) if (c[i]) bitvector_free(c[i]);
    PROFILE_END("assoc_stego_hide_byte");
    return -1;
}

int assoc_stego_disclose_byte(const AssocStego* as, const uint8_t* hidden, size_t len, uint8_t* out_val) {
    PROFILE_START("assoc_stego_disclose_byte");
    if (!as || !hidden || !out_val) { PROFILE_END("assoc_stego_disclose_byte"); return -1; }
    size_t cblen = (as->etalon_length + 7) / 8;
    if (len != 3 * cblen) { PROFILE_END("assoc_stego_disclose_byte"); return -1; }

    BitVector* c[3];
    for (int i = 0; i < 3; i++) c[i] = bitvector_from_bytes(hidden + i * cblen, cblen, as->etalon_length);
    for (int i = 0; i < 3; i++) if (!c[i]) goto err_c;

    int d[3];
    for (int i = 0; i < 3; i++) {
        d[i] = assoc_stego_disclose_etalon_cached(as, c[i]->data);
        bitvector_free(c[i]);
        if (d[i] < 0) { PROFILE_END("assoc_stego_disclose_byte"); return -1; }
    }

    int val = d[0] * 100 + d[1] * 10 + d[2];
    if (val > 255) { PROFILE_END("assoc_stego_disclose_byte"); return -1; }
    *out_val = (uint8_t)val;
    PROFILE_END("assoc_stego_disclose_byte");
    return 0;
err_c:
    for (int i = 0; i < 3; i++) if (c[i]) bitvector_free(c[i]);
    PROFILE_END("assoc_stego_disclose_byte");
    return -1;
}
// ========== ОПТИМИЗИРОВАННЫЙ ПОИСК С AVX2 ==========
// ========== ОПТИМИЗИРОВАННЫЙ ПОИСК С РАННИМ ВЫХОДОМ ==========
int assoc_stego_disclose_etalon_optimized(const AssocStego* as, const uint64_t* container_data) {
    PROFILE_START("assoc_stego_disclose_etalon_optimized");

    if (!as || !as->key_generated || !container_data) {
        PROFILE_END("assoc_stego_disclose_etalon_optimized");
        return -1;
    }

    size_t word_count = as->etalons[0]->word_count;
    int found_idx = -1;

    // === ИСПОЛЬЗУЕМ СТЕК ВМЕСТО posix_memalign ===
#if defined(__GNUC__) || defined(__ELBRUS__) || defined(__clang__)
    uint64_t cm[MAX_WORDS] __attribute__((aligned(32))) = {0};
#else
    __declspec(align(32)) uint64_t cm[MAX_WORDS] = {0};
#endif

    // Проверяем эталоны
    for (size_t i = 0; i < as->etalon_count; i++) {
#if defined(_M_X64) || defined(__x86_64__)
        bitvector_and_avx2(cm, container_data, as->key[i]->data, word_count);
#else
        vector_and(container_data, as->key[i]->data, cm, word_count);
#endif

        uint64_t diff = 0;
        
        // Безветвленная проверка (предикаты для VLIW)
        #pragma unroll(4)
        for (size_t j = 0; j < word_count; j++) {
            diff |= (cm[j] ^ as->cache[i].etalon_masked[j]);
        }

        if (diff == 0) {
            found_idx = (int)i;
            break;
        }
    }

    PROFILE_END("assoc_stego_disclose_etalon_optimized");
    return found_idx;
}
// ========== БЫСТРАЯ ВЕРСИЯ БЕЗ КУЧИ (МАССИВ В СТЕКЕ , БУФЕРЫ В СТЕКЕ) ==========
int assoc_stego_hide_byte_fast(const AssocStego* as, uint8_t val, uint8_t* out_buffer, size_t* out_len) {
    if (!as || !as->key_generated || !out_buffer || !out_len) return -1;
    PROFILE_START("assoc_stego_hide_byte_fast");

    int d[] = { val / 100, (val % 100) / 10, val % 10 };
    size_t container_byte_len = (as->etalon_length + 7) / 8;
    size_t word_count = as->etalons[0]->word_count;
    size_t pos = 0;

    for (int i = 0; i < 3; i++) {
        // ОПТИМИЗАЦИЯ ПАМЯТИ ---

        // Выделяемвыровненные массивы для всего: и для временных данных, и для самого контейнера
#if defined(__GNUC__) || defined(__ELBRUS__) || defined(__clang__)
        uint64_t aligned_container[MAX_WORDS] __attribute__((aligned(32))) = {0};
        uint64_t xe[MAX_WORDS] __attribute__((aligned(32))) = {0};
        uint64_t m[MAX_WORDS] __attribute__((aligned(32))) = {0};
#else
        __declspec(align(32)) uint64_t aligned_container[MAX_WORDS] = {0};
        __declspec(align(32)) uint64_t xe[MAX_WORDS] = {0};
        __declspec(align(32)) uint64_t m[MAX_WORDS] = {0};
#endif

        // Заполняем контейнер случайными битами (приводим к uint8_t*, так как функция ждет байты)
        bitvector_rng_fill((uint8_t*)aligned_container, container_byte_len);

        // Векторная математика работает без приведений типов и с ровными адресами
        vector_xor(aligned_container, as->etalons[d[i]]->data, xe, word_count);
        vector_and(xe, as->key[d[i]]->data, m, word_count);
        vector_xor(aligned_container, m, aligned_container, word_count);

        // копируем готовый результат в итоговый невыровненный массив out_buffer
        memcpy(out_buffer + pos, (uint8_t*)aligned_container, container_byte_len);
        
        

        pos += container_byte_len;
    }

    *out_len = pos;
    PROFILE_END("assoc_stego_hide_byte_fast");
    return 0;
}
// ========== Обновляеный assoc_stego_disclose_byte_fast ==========
int assoc_stego_disclose_byte_fast(const AssocStego* as, const uint8_t* hidden, size_t len, uint8_t* out_val) {
    PROFILE_START("assoc_stego_disclose_byte_fast");

    if (!as || !as->key_generated || !hidden || !out_val) {
        PROFILE_END("assoc_stego_disclose_byte_fast");
        return -1;
    }

    size_t cblen = (as->etalon_length + 7) / 8;
    if (len != 3 * cblen) { PROFILE_END("assoc_stego_disclose_byte_fast"); return -1; }

    int d[3];

    for (int i = 0; i < 3; i++) {
        // ОПТИМИЗАЦИЯ ПАМЯТИ ---
        
        // Создаем идеально выровненный буфер на стеке (выделяем с запасом 8 слов = 64 байта, выровненные по границе 32 байт)
	#if defined(__GNUC__) || defined(__ELBRUS__) || defined(__clang__)
        	uint64_t aligned_container[8] __attribute__((aligned(32))) = {0};
	#else
        	__declspec(align(32)) uint64_t aligned_container[8] = {0};
	#endif

        //  копирование невыровненных байт в ровный массив
        memcpy(aligned_container, hidden + i * cblen, cblen);

        // Передача оптимизированной функции ровного буфера
        d[i] = assoc_stego_disclose_etalon_optimized(as, aligned_container);
        
        
        if (d[i] < 0) {
            printf("DEBUG: Failed to disclose etalon for digit %d (container %d)\n", d[i], i);
            PROFILE_END("assoc_stego_disclose_byte_fast");
            return -1;
        }
    }

    int val = d[0] * 100 + d[1] * 10 + d[2];
    if (val > 255) { PROFILE_END("assoc_stego_disclose_byte_fast"); return -1; }
    *out_val = (uint8_t)val;

    PROFILE_END("assoc_stego_disclose_byte_fast");
    return 0;
}
