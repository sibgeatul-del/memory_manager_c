#ifndef MEM_MANAGER_H_INCLUDED
#define MEM_MANAGER_H_INCLUDED
#include <stddef.h>

void mem_init(int debug_mode);
void* mem_malloc(size_t size);
void mem_free(void* ptr);
void* mem_realloc(void* ptr, size_t new_size);
size_t mem_granted_space(void* ptr);
void mem_print_stats(void);
size_t mem_check_leaks(void);
void mem_shutdown(void);


#endif // MEM_MANAGER_H_INCLUDED
