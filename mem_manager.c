
#include "mem_manager.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define POOL_SIZE      (64 * 1024)
#define ALIGNMENT      8

static const size_t BLOCK_SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
#define NUM_SIZES (sizeof(BLOCK_SIZES) / sizeof(BLOCK_SIZES[0]))

typedef struct block_header {
    size_t size;
    int is_free;
} block_header_t;

typedef struct token_manager {
    char* pool_start;
    size_t block_size;
    size_t free_count;
    uint16_t* free_stack;
    size_t stack_top;
    struct token_manager* next;
} token_manager_t;

typedef struct {
    token_manager_t* pools[NUM_SIZES];
    CRITICAL_SECTION lock;
    size_t total_allocated;
    size_t total_reserved;
    size_t allocation_count;
    int debug_mode;
} memory_manager_t;

static memory_manager_t manager = {0};

static int get_size_class(size_t size) {
    for (int i = 0; i < NUM_SIZES; i++)
        if (size <= BLOCK_SIZES[i]) return i;
    return -1;
}

static token_manager_t* create_token_pool(size_t block_size) {
    size_t num_blocks = POOL_SIZE / block_size;
    if (num_blocks == 0) return NULL;

    size_t stack_mem = num_blocks * sizeof(uint16_t);
    size_t total = sizeof(token_manager_t) + POOL_SIZE + stack_mem;

    char* base = (char*)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!base) return NULL;

    token_manager_t* tm = (token_manager_t*)base;
    tm->pool_start = base + sizeof(token_manager_t) + stack_mem;
    tm->block_size = block_size;
    tm->free_count = num_blocks;
    tm->free_stack = (uint16_t*)(base + sizeof(token_manager_t));
    tm->stack_top = 0;
    tm->next = NULL;

    for (uint16_t i = 0; i < num_blocks; i++)
        tm->free_stack[tm->stack_top++] = i;

    manager.total_reserved += POOL_SIZE;
    return tm;
}

static void* alloc_from_pool(token_manager_t* tm) {
    if (tm->stack_top == 0) return NULL;

    uint16_t index = tm->free_stack[--tm->stack_top];
    tm->free_count--;

    char* block = tm->pool_start + (index * tm->block_size);
    block_header_t* header = (block_header_t*)block;
    header->size = tm->block_size;
    header->is_free = 0;

    return block + sizeof(block_header_t);
}

static void free_to_pool(token_manager_t* tm, void* ptr) {
    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    size_t offset = (char*)header - tm->pool_start;
    uint16_t index = (uint16_t)(offset / tm->block_size);

    tm->free_stack[tm->stack_top++] = index;
    tm->free_count++;
    header->is_free = 1;
}

static token_manager_t* find_owning_pool(void* ptr) {
    for (int i = 0; i < NUM_SIZES; i++) {
        token_manager_t* tm = manager.pools[i];
        while (tm) {
            if (ptr >= (void*)tm->pool_start && ptr < (void*)tm->pool_start + POOL_SIZE)
                return tm;
            tm = tm->next;
        }
    }
    return NULL;
}

static void* large_alloc(size_t size) {
    size_t total = size + sizeof(block_header_t);
    char* ptr = (char*)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) return NULL;

    block_header_t* header = (block_header_t*)ptr;
    header->size = total;
    header->is_free = 0;
    manager.total_reserved += total;
    return ptr + sizeof(block_header_t);
}

static void large_free(void* ptr) {
    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    manager.total_reserved -= header->size;
    VirtualFree(header, 0, MEM_RELEASE);
}

void mem_init(int debug_mode) {
    InitializeCriticalSection(&manager.lock);
    manager.debug_mode = debug_mode;
    manager.total_allocated = 0;
    manager.total_reserved = 0;
    manager.allocation_count = 0;

    for (int i = 0; i < NUM_SIZES; i++)
        manager.pools[i] = create_token_pool(BLOCK_SIZES[i]);

    if (debug_mode) printf("[INFO] Memory Manager Initialized\n");
}

void* mem_malloc(size_t size) {
    if (size == 0) return NULL;

    EnterCriticalSection(&manager.lock);

    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    int class_idx = get_size_class(size);
    void* ptr = NULL;

    if (class_idx >= 0) {
        token_manager_t* tm = manager.pools[class_idx];
        ptr = alloc_from_pool(tm);

        if (!ptr) {
            token_manager_t* new_tm = create_token_pool(BLOCK_SIZES[class_idx]);
            if (new_tm) {
                new_tm->next = manager.pools[class_idx];
                manager.pools[class_idx] = new_tm;
                ptr = alloc_from_pool(new_tm);
            }
        }

        if (ptr) {
            manager.total_allocated += BLOCK_SIZES[class_idx];
            manager.allocation_count++;
        }
    } else {
        ptr = large_alloc(size);
        if (ptr) {
            manager.total_allocated += size;
            manager.allocation_count++;
        }
    }

    LeaveCriticalSection(&manager.lock);
    return ptr;
}

void mem_free(void* ptr) {
    if (!ptr) return;

    EnterCriticalSection(&manager.lock);

    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    size_t block_size = header->size;

    if (block_size <= 4096 + sizeof(block_header_t)) {
        token_manager_t* tm = find_owning_pool(ptr);
        if (tm) {
            free_to_pool(tm, ptr);
            manager.total_allocated -= (block_size - sizeof(block_header_t));
            manager.allocation_count--;
        }
    } else {
        manager.total_allocated -= (block_size - sizeof(block_header_t));
        manager.allocation_count--;
        large_free(ptr);
    }

    LeaveCriticalSection(&manager.lock);
}

void* mem_realloc(void* ptr, size_t new_size) {
    if (!ptr) return mem_malloc(new_size);
    if (new_size == 0) {
        mem_free(ptr);
        return NULL;
    }

    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    size_t old_size = header->size - sizeof(block_header_t);

    if (new_size <= old_size) return ptr;

    void* new_ptr = mem_malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
        mem_free(ptr);
    }
    return new_ptr;
}

size_t mem_granted_space(void* ptr) {
    if (!ptr) return 0;
    block_header_t* header = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    return header->size - sizeof(block_header_t);
}

void mem_print_stats(void) {
    EnterCriticalSection(&manager.lock);

    printf("\n========================================\n");
    printf("       MEMORY MANAGER STATISTICS        \n");
    printf("========================================\n");
    printf("Total allocated (user):   %10zu bytes\n", manager.total_allocated);
    printf("Total reserved (OS):      %10zu bytes\n", manager.total_reserved);
    printf("Active allocations:       %10zu\n", manager.allocation_count);
    printf("Memory waste:             %10zu bytes\n", manager.total_reserved - manager.total_allocated);
    printf("========================================\n");

    LeaveCriticalSection(&manager.lock);
}

size_t mem_check_leaks(void) {
    size_t leaks = manager.allocation_count;
    if (leaks > 0 && manager.debug_mode)
        printf("\n[WARNING] %zu memory leaks detected!\n", leaks);
    return leaks;
}

void mem_shutdown(void) {
    if (manager.debug_mode) printf("\n[INFO] Shutting down memory manager...\n");

    mem_check_leaks();

    for (int i = 0; i < NUM_SIZES; i++) {
        token_manager_t* tm = manager.pools[i];
        while (tm) {
            token_manager_t* next = tm->next;
            VirtualFree(tm, 0, MEM_RELEASE);
            tm = next;
        }
    }

    DeleteCriticalSection(&manager.lock);
    if (manager.debug_mode) printf("[INFO] Memory manager shutdown complete.\n");
}
