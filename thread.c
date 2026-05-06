
/**
 * Thread Manager Implementation
 * Simple threading with CRITICAL_SECTION synchronization
 * Using Windows Native Threads (no pthread required)
 *
 * Author: Araf Hussain (ID: 2111078642)
 */

#include "thread.h"
#include "mem_manager.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================
// CRITICAL_SECTION for thread safety (Windows mutex)
// ============================================
static CRITICAL_SECTION memory_lock;

// ============================================
// Statistics for each thread
// ============================================
typedef struct {
    int thread_id;
    int alloc_count;
    int free_count;
} thread_data_t;

static thread_data_t all_results[10];  // Store results for up to 10 threads
static int active_threads = 0;

// ============================================
// Random number generator
// ============================================
static unsigned int simple_rand(unsigned int r) {
    r ^= (r >> 7);
    r ^= (r << 25);
    return r ^ (r >> 12);
}

// ============================================
// Thread-safe malloc (with CRITICAL_SECTION)
// ============================================
static void* safe_malloc(size_t size, int thread_id) {
    EnterCriticalSection(&memory_lock);
    void* ptr = mem_malloc(size);
    LeaveCriticalSection(&memory_lock);

    if (ptr) {
        printf("  Thread %d: Allocated %zu bytes at %p\n", thread_id, size, ptr);
    }
    return ptr;
}

// ============================================
// Thread-safe free (with CRITICAL_SECTION)
// ============================================
static void safe_free(void* ptr, int thread_id) {
    if (!ptr) return;

    EnterCriticalSection(&memory_lock);
    printf("  Thread %d: Freeing %p\n", thread_id, ptr);
    mem_free(ptr);
    LeaveCriticalSection(&memory_lock);
}

// ============================================
// What each thread does
// ============================================
static DWORD WINAPI thread_work(LPVOID arg) {
    int id = *(int*)arg;
    free(arg);  // Free the allocated id

    thread_data_t data;
    data.thread_id = id;
    data.alloc_count = 0;
    data.free_count = 0;

    unsigned int rnd = id * 12345;

    printf("\n  Thread %d: Starting...\n", id);

    // Each thread does 10 operations
    for (int i = 0; i < 10; i++) {
        rnd = simple_rand(rnd);
        int action = rnd % 2;  // 0 = allocate, 1 = free

        if (action == 0) {
            // Allocate memory
            rnd = simple_rand(rnd);
            size_t size = 16 + (rnd % 128);  // 16 to 143 bytes

            void* ptr = safe_malloc(size, id);
            if (ptr) {
                data.alloc_count++;
            }
        } else {
            // Allocate and then free
            void* ptr = safe_malloc(32, id);
            if (ptr) {
                data.alloc_count++;
                safe_free(ptr, id);
                data.free_count++;
            }
        }

        // Small delay to simulate real work
        Sleep(10);
    }

    printf("  Thread %d: Finished (alloc=%d, free=%d)\n",
           id, data.alloc_count, data.free_count);

    // Store result
    all_results[active_threads++] = data;

    return 0;
}

// ============================================
// Start multiple threads
// ============================================
void start_thread_tests(int num_threads) {
    if (num_threads > 10) num_threads = 10;

    HANDLE threads[10];
    active_threads = 0;

    // Initialize critical section (Windows mutex)
    InitializeCriticalSection(&memory_lock);

    printf("\n========================================\n");
    printf("  STARTING %d THREADS\n", num_threads);
    printf("  Each thread will allocate/free memory\n");
    printf("  CRITICAL_SECTION protects against race conditions\n");
    printf("========================================\n");

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int* id = (int*)malloc(sizeof(int));
        *id = i + 1;

        threads[i] = CreateThread(
            NULL,           // Security attributes
            0,              // Stack size
            thread_work,    // Thread function
            id,             // Thread argument
            0,              // Creation flags
            NULL            // Thread ID
        );
    }

    // Wait for all threads to finish
    WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);

    // Close thread handles
    for (int i = 0; i < num_threads; i++) {
        CloseHandle(threads[i]);
    }

    // Clean up critical section
    DeleteCriticalSection(&memory_lock);

    printf("\n  All threads completed!\n");
}

// ============================================
// Print results
// ============================================
void print_thread_results(void) {
    printf("\n========================================\n");
    printf("         THREAD RESULTS\n");
    printf("========================================\n");

    int total_alloc = 0;
    int total_free = 0;

    for (int i = 0; i < active_threads; i++) {
        printf("Thread %d: %d allocations, %d frees\n",
               all_results[i].thread_id,
               all_results[i].alloc_count,
               all_results[i].free_count);
        total_alloc += all_results[i].alloc_count;
        total_free += all_results[i].free_count;
    }

    printf("----------------------------------------\n");
    printf("TOTAL: %d allocations, %d frees\n", total_alloc, total_free);

    printf("\n[SYNCHRONIZATION INFO]\n");
    printf("  CRITICAL_SECTION protects the memory manager\n");
    printf("  Only ONE thread can allocate/free at a time\n");
    printf("  Race conditions are PREVENTED\n");
    printf("========================================\n");
}
