// TMNT OS Memory Manager
#include "memory.h"

typedef struct memory_block {
    uint32_t size;
    int free;
    struct memory_block* next;
    struct memory_block* prev;
} memory_block_t;

static memory_block_t* heap_start = NULL;
static uint32_t total_allocated = 0;

void memory_init() {
    heap_start = (memory_block_t*)HEAP_START;
    heap_start->size = HEAP_SIZE - sizeof(memory_block_t);
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
}

void* malloc(uint32_t size) {
    memory_block_t* current = heap_start;
    
    // Align size to 4 bytes
    size = (size + 3) & ~3;
    
    while(current) {
        if(current->free && current->size >= size) {
            // Split block if large enough
            if(current->size > size + sizeof(memory_block_t) + 4) {
                memory_block_t* new_block = (memory_block_t*)((uint8_t*)current + 
                                           sizeof(memory_block_t) + size);
                new_block->size = current->size - size - sizeof(memory_block_t);
                new_block->free = 1;
                new_block->next = current->next;
                new_block->prev = current;
                
                if(current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = size;
            }
            
            current->free = 0;
            total_allocated += current->size;
            return (void*)((uint8_t*)current + sizeof(memory_block_t));
        }
        current = current->next;
    }
    
    return NULL; // Out of memory
}

void free(void* ptr) {
    if(!ptr) return;
    
    memory_block_t* block = (memory_block_t*)((uint8_t*)ptr - sizeof(memory_block_t));
    block->free = 1;
    total_allocated -= block->size;
    
    // Merge with next block if free
    if(block->next && block->next->free) {
        block->size += sizeof(memory_block_t) + block->next->size;
        block->next = block->next->next;
        if(block->next) {
            block->next->prev = block;
        }
    }
    
    // Merge with previous block if free
    if(block->prev && block->prev->free) {
        block->prev->size += sizeof(memory_block_t) + block->size;
        block->prev->next = block->next;
        if(block->next) {
            block->next->prev = block->prev;
        }
    }
}

void memset(void* ptr, uint8_t value, uint32_t num) {
    uint8_t* p = (uint8_t*)ptr;
    while(num--) {
        *p++ = value;
    }
}

void memcpy(void* dest, const void* src, uint32_t num) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while(num--) {
        *d++ = *s++;
    }
}

int memcmp(const void* ptr1, const void* ptr2, uint32_t num) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    while(num--) {
        if(*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

void* realloc(void* ptr, uint32_t size) {
    if(!ptr) return malloc(size);
    if(size == 0) { free(ptr); return NULL; }
    
    memory_block_t* block = (memory_block_t*)((uint8_t*)ptr - sizeof(memory_block_t));
    if(block->size >= size) return ptr;
    
    void* new_ptr = malloc(size);
    if(new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}
