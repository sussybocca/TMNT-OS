#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

#define MEMORY_TOTAL 0x1000000  // 16MB
#define HEAP_START 0x100000     // 1MB mark
#define HEAP_SIZE 0x100000      // 1MB heap

void memory_init();
void* malloc(uint32_t size);
void* realloc(void* ptr, uint32_t size);
void free(void* ptr);
void* realloc(void* ptr, uint32_t size);
void memset(void* ptr, uint8_t value, uint32_t num);
void memcpy(void* dest, const void* src, uint32_t num);
int memcmp(const void* ptr1, const void* ptr2, uint32_t num);

#endif
void* realloc(void* ptr, uint32_t size);
