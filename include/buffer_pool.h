#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t** buffers;
    size_t buffer_size;
    size_t count;
    size_t* available;
    size_t available_count;
} BufferPool;

BufferPool* pool_create(size_t buffer_size, size_t count);
void pool_destroy(BufferPool* pool);
uint8_t* pool_acquire(BufferPool* pool);
void pool_release(BufferPool* pool, uint8_t* buffer);

#endif