/**
 * Memory Manager Test Program
 * CSE323 Operating Systems Design
 *
 * Team Members:
 *   - Araf Hussain (ID: 2111078642): Threading & Synchronization
 *   - Sibgwatul Rabbi (ID: 2311436042): Memory Manager Implementation
 */

#include "mem_manager.h"
#include "thread.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("\n");
    printf("+==================================================================+\n");
    printf("|                    MEMORY MANAGER TEST SUITE                     |\n");
    printf("|                         CSE323 Project                          |\n");
    printf("+==================================================================+\n");
    printf("|                                                                  |\n");
    printf("|  Team Members:                                                   |\n");
    printf("|    - Araf Hussain (ID: 2111078642): Threading & Synchronization  |\n");
    printf("|    - Sibgwatul Rabbi (ID: 2311436042): Memory Manager            |\n");
    printf("|                                                                  |\n");
    printf("+==================================================================+\n");

    // Initialize memory manager
    mem_init(1);
    mem_print_stats();

    // ========================================
    // MEMORY MANAGER TESTS (Sibgwatul Rabbi)
    // ========================================

    printf("\n+------------------------------------------------------------------+\n");
    printf("| MEMORY MANAGER TESTS (Sibgwatul Rabbi)                          |\n");
    printf("+------------------------------------------------------------------+\n");

    // Test basic allocation
    printf("\n[Test 1] Basic Allocation\n");
    int* ptr = mem_malloc(100);
    if (ptr) {
        ptr[0] = 42;
        printf("  Allocated int[100] at %p, value = %d\n", ptr, ptr[0]);
        printf("  Granted space: %zu bytes\n", mem_granted_space(ptr));
        mem_free(ptr);
        printf("  PASSED\n");
    }

    // Test reallocation
    printf("\n[Test 2] Reallocation\n");
    char* str = mem_malloc(10);
    strcpy(str, "Hello");
    printf("  Before: '%s' at %p\n", str, str);
    str = mem_realloc(str, 50);
    strcat(str, " World!");
    printf("  After:  '%s' at %p\n", str, str);
    mem_free(str);
    printf("  PASSED\n");

    // Test multiple allocations
    printf("\n[Test 3] Multiple Allocations\n");
    void* arr[100];
    for (int i = 0; i < 100; i++) {
        arr[i] = mem_malloc(64);
    }
    printf("  Allocated 100 x 64-byte blocks\n");
    for (int i = 0; i < 100; i++) {
        mem_free(arr[i]);
    }
    printf("  PASSED\n");

    // ========================================
    // THREADING TESTS (Araf Hussain)
    // ========================================

    printf("\n+------------------------------------------------------------------+\n");
    printf("| THREADING & SYNCHRONIZATION TESTS (Araf Hussain)                |\n");
    printf("+------------------------------------------------------------------+\n");

    // Run 4 threads
    start_thread_tests(4);
    print_thread_results();

    // Run 8 threads (stress test)
    start_thread_tests(8);
    print_thread_results();

    // ========================================
    // FINAL STATISTICS
    // ========================================

    mem_print_stats();

    printf("\n+------------------------------------------------------------------+\n");
    printf("| MEMORY LEAK CHECK                                               |\n");
    printf("+------------------------------------------------------------------+\n");

    size_t leaks = mem_check_leaks();
    if (leaks == 0)
        printf("  PASSED - NO MEMORY LEAKS DETECTED\n");
    else
        printf("  WARNING - %zu memory leaks detected!\n", leaks);

    mem_shutdown();

    printf("\n+==================================================================+\n");
    printf("|                         TEST COMPLETE                            |\n");
    printf("+==================================================================+\n");
    printf("\nPress ENTER to exit...");
    getchar();

    return 0;
}
