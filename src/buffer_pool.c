#include "buffer_pool.h"
#include <stdlib.h>
#include <string.h>

BufferPool* pool_create(size_t buffer_size, size_t count) {
    BufferPool* pool = malloc(sizeof(BufferPool));
    if (!pool) return NULL;

    pool->buffers = malloc(count * sizeof(uint8_t*));
    pool->available = malloc(count * sizeof(size_t));
    pool->buffer_size = buffer_size;
    pool->count = count;
    pool->available_count = count;

    for (size_t i = 0; i < count; i++) {
        pool->buffers[i] = calloc(buffer_size, 1);
        pool->available[i] = i;
    }

    return pool;
}

uint8_t* pool_acquire(BufferPool* pool) {
    if (!pool || pool->available_count == 0) {
        return malloc(pool ? pool->buffer_size : 128);
    }
    size_t idx = pool->available[--pool->available_count];
    memset(pool->buffers[idx], 0, pool->buffer_size);
    return pool->buffers[idx];
}

void pool_release(BufferPool* pool, uint8_t* buffer) {
    if (!pool) { free(buffer); return; }

    for (size_t i = 0; i < pool->count; i++) {
        if (pool->buffers[i] == buffer) {
            pool->available[pool->available_count++] = i;
            return;
        }
    }
    free(buffer);
}

void pool_destroy(BufferPool* pool) {
    if (!pool) return;
    for (size_t i = 0; i < pool->count; i++) {
        free(pool->buffers[i]);
    }
    free(pool->buffers);
    free(pool->available);
    free(pool);
}