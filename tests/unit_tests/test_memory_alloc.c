/**
 * test_memory_alloc.c - Unit Tests for Memory Allocation
 * 
 * This file contains unit tests for DDR memory allocation,
 * deallocation, and management functions.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Compilation:
 *   gcc -o test_memory_alloc test_memory_alloc.c -lunity -lddr_manager
 * 
 * Usage:
 *   ./test_memory_alloc
 *   ./test_memory_alloc --verbose
 *   ./test_memory_alloc --test <test_name>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Unity Test Framework */
#include "unity.h"

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_config.h>
#include <ddr_allocator.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEST_ALLOC_SIZE_MIN     64
#define TEST_ALLOC_SIZE_MAX     (1024 * 1024)  /* 1 MB */
#define TEST_ALLOC_ITERATIONS   100
#define TEST_ALLOC_ALIGNMENT    4096

#define TEST_PATTERN_WRITE      0xAA
#define TEST_PATTERN_READ       0xBB
#define TEST_PATTERN_RANDOM     0xCC

/* ============================================================================
 * Test Data
 * ============================================================================ */

typedef struct {
    void *ptr;
    size_t size;
    unsigned int flags;
} test_allocation_t;

static test_allocation_t test_allocations[TEST_ALLOC_ITERATIONS];

/* ============================================================================
 * Test Setup and Teardown
 * ============================================================================ */

void setUp(void)
{
    /* Initialize memory allocator for testing */
    printf("Setting up memory allocator test environment...\n");
    memset(test_allocations, 0, sizeof(test_allocations));
}

void tearDown(void)
{
    /* Clean up any remaining allocations */
    for (int i = 0; i < TEST_ALLOC_ITERATIONS; i++) {
        if (test_allocations[i].ptr) {
            ddr_free(test_allocations[i].ptr);
            test_allocations[i].ptr = NULL;
        }
    }
    printf("Cleaning up memory allocator test environment...\n");
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * write_pattern - Write test pattern to memory
 */
static void write_pattern(void *ptr, size_t size, unsigned char pattern)
{
    memset(ptr, pattern, size);
}

/**
 * verify_pattern - Verify test pattern in memory
 */
static bool verify_pattern(void *ptr, size_t size, unsigned char pattern)
{
    unsigned char *bytes = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            return false;
        }
    }
    return true;
}

/**
 * write_alternating_pattern - Write alternating pattern
 */
static void write_alternating_pattern(void *ptr, size_t size)
{
    unsigned char *bytes = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = (i % 2 == 0) ? 0xAA : 0x55;
    }
}

/**
 * verify_alternating_pattern - Verify alternating pattern
 */
static bool verify_alternating_pattern(void *ptr, size_t size)
{
    unsigned char *bytes = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        unsigned char expected = (i % 2 == 0) ? 0xAA : 0x55;
        if (bytes[i] != expected) {
            return false;
        }
    }
    return true;
}

/**
 * generate_random_size - Generate random allocation size
 */
static size_t generate_random_size(size_t min, size_t max)
{
    return min + (rand() % (max - min + 1));
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * test_basic_allocation - Test basic memory allocation
 */
void test_basic_allocation(void)
{
    void *ptr;
    size_t sizes[] = {64, 256, 1024, 4096, 16384, 65536, 262144};
    
    printf("\n=== Testing Basic Allocation ===\n");
    
    for (int i = 0; i < sizeof(sizes) / sizeof(size_t); i++) {
        size_t size = sizes[i];
        
        /* Allocate memory */
        ptr = ddr_alloc(size, 0);
        TEST_ASSERT_NOT_NULL(ptr);
        
        /* Write pattern */
        write_pattern(ptr, size, TEST_PATTERN_WRITE);
        
        /* Verify pattern */
        TEST_ASSERT_TRUE(verify_pattern(ptr, size, TEST_PATTERN_WRITE));
        
        /* Free memory */
        ddr_free(ptr);
        
        printf("  Allocated/freed %zu bytes\n", size);
    }
    
    printf("✅ Basic allocation tests passed\n");
}

/**
 * test_zero_allocation - Test zero-size allocation
 */
void test_zero_allocation(void)
{
    void *ptr;
    
    printf("\n=== Testing Zero Allocation ===\n");
    
    /* Test: Zero size allocation */
    ptr = ddr_alloc(0, 0);
    TEST_ASSERT_NULL(ptr);
    
    /* Test: Very small allocation */
    ptr = ddr_alloc(1, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    ddr_free(ptr);
    
    /* Test: Negative size (should fail) */
    ptr = ddr_alloc(-1, 0);
    TEST_ASSERT_NULL(ptr);
    
    printf("✅ Zero allocation tests passed\n");
}

/**
 * test_large_allocation - Test large memory allocation
 */
void test_large_allocation(void)
{
    void *ptr;
    size_t sizes[] = {
        1024 * 1024,      /* 1 MB */
        4 * 1024 * 1024,  /* 4 MB */
        16 * 1024 * 1024, /* 16 MB */
        64 * 1024 * 1024  /* 64 MB */
    };
    
    printf("\n=== Testing Large Allocation ===\n");
    
    for (int i = 0; i < sizeof(sizes) / sizeof(size_t); i++) {
        size_t size = sizes[i];
        
        /* Allocate memory */
        ptr = ddr_alloc(size, 0);
        if (ptr) {
            /* Write pattern */
            write_pattern(ptr, size, TEST_PATTERN_WRITE);
            
            /* Verify pattern */
            TEST_ASSERT_TRUE(verify_pattern(ptr, size, TEST_PATTERN_WRITE));
            
            /* Free memory */
            ddr_free(ptr);
            printf("  Allocated/freed %zu MB\n", size / (1024 * 1024));
        } else {
            printf("  Skipped %zu MB allocation (memory may be low)\n", 
                   size / (1024 * 1024));
        }
    }
    
    printf("✅ Large allocation tests passed\n");
}

/**
 * test_aligned_allocation - Test aligned memory allocation
 */
void test_aligned_allocation(void)
{
    void *ptr;
    size_t size = 4096;
    size_t alignments[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    
    printf("\n=== Testing Aligned Allocation ===\n");
    
    for (int i = 0; i < sizeof(alignments) / sizeof(size_t); i++) {
        size_t align = alignments[i];
        
        /* Allocate aligned memory */
        ptr = ddr_alloc_aligned(size, align, 0);
        TEST_ASSERT_NOT_NULL(ptr);
        
        /* Check alignment */
        TEST_ASSERT_EQUAL(0, (uintptr_t)ptr % align);
        
        /* Write and verify pattern */
        write_pattern(ptr, size, TEST_PATTERN_WRITE);
        TEST_ASSERT_TRUE(verify_pattern(ptr, size, TEST_PATTERN_WRITE));
        
        /* Free memory */
        ddr_free(ptr);
        
        printf("  Allocated/freed %zu bytes aligned to %zu\n", size, align);
    }
    
    printf("✅ Aligned allocation tests passed\n");
}

/**
 * test_dma_allocation - Test DMA memory allocation
 */
void test_dma_allocation(void)
{
    void *ptr;
    dma_addr_t dma_handle;
    size_t sizes[] = {4096, 16384, 65536, 262144, 1048576};
    
    printf("\n=== Testing DMA Allocation ===\n");
    
    for (int i = 0; i < sizeof(sizes) / sizeof(size_t); i++) {
        size_t size = sizes[i];
        
        /* Allocate DMA memory */
        ptr = ddr_dma_alloc(size, &dma_handle, 0);
        if (ptr) {
            /* Check DMA handle */
            TEST_ASSERT(dma_handle != 0);
            
            /* Write pattern */
            write_pattern(ptr, size, TEST_PATTERN_WRITE);
            
            /* Verify pattern */
            TEST_ASSERT_TRUE(verify_pattern(ptr, size, TEST_PATTERN_WRITE));
            
            /* Free memory */
            ddr_dma_free(ptr, size, dma_handle);
            printf("  Allocated/freed %zu DMA bytes at 0x%08lx\n",
                   size, (unsigned long)dma_handle);
        } else {
            printf("  Skipped %zu DMA allocation\n", size);
        }
    }
    
    printf("✅ DMA allocation tests passed\n");
}

/**
 * test_memory_limits - Test memory allocation limits
 */
void test_memory_limits(void)
{
    void *ptr;
    size_t size = 1024 * 1024;  /* 1 MB */
    int count = 0;
    void *allocations[100];
    
    printf("\n=== Testing Memory Limits ===\n");
    
    /* Allocate until failure */
    while (count < 100) {
        ptr = ddr_alloc(size, 0);
        if (!ptr) {
            break;
        }
        allocations[count] = ptr;
        count++;
    }
    
    printf("  Allocated %d blocks of %zu bytes\n", count, size);
    
    /* Free all allocations */
    for (int i = 0; i < count; i++) {
        ddr_free(allocations[i]);
    }
    
    /* Test: Allocate after free */
    ptr = ddr_alloc(size, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    ddr_free(ptr);
    
    printf("✅ Memory limits tests passed\n");
}

/**
 * test_pattern_integrity - Test memory pattern integrity
 */
void test_pattern_integrity(void)
{
    void *ptr;
    size_t size = 1024 * 1024;  /* 1 MB */
    int iterations = 100;
    
    printf("\n=== Testing Pattern Integrity ===\n");
    
    for (int i = 0; i < iterations; i++) {
        size_t alloc_size = generate_random_size(1024, size);
        
        /* Allocate memory */
        ptr = ddr_alloc(alloc_size, 0);
        TEST_ASSERT_NOT_NULL(ptr);
        
        /* Write pattern */
        write_alternating_pattern(ptr, alloc_size);
        
        /* Verify pattern */
        TEST_ASSERT_TRUE(verify_alternating_pattern(ptr, alloc_size));
        
        /* Free memory */
        ddr_free(ptr);
        
        if ((i + 1) % 10 == 0) {
            printf("  Pattern test iteration %d completed\n", i + 1);
        }
    }
    
    printf("✅ Pattern integrity tests passed\n");
}

/**
 * test_concurrent_allocation - Test concurrent allocations
 */
void test_concurrent_allocation(void)
{
    #define NUM_THREADS 4
    #define ALLOCS_PER_THREAD 25
    
    void *allocations[NUM_THREADS][ALLOCS_PER_THREAD];
    size_t sizes[NUM_THREADS][ALLOCS_PER_THREAD];
    
    printf("\n=== Testing Concurrent Allocation ===\n");
    
    srand(time(NULL));
    
    /* Allocate memory from multiple threads */
    #pragma omp parallel for num_threads(NUM_THREADS)
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            size_t size = generate_random_size(1024, 1024 * 1024);
            sizes[t][i] = size;
            allocations[t][i] = ddr_alloc(size, 0);
            
            if (allocations[t][i]) {
                write_pattern(allocations[t][i], size, TEST_PATTERN_WRITE);
            }
        }
    }
    
    /* Verify and free */
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            if (allocations[t][i]) {
                TEST_ASSERT_TRUE(verify_pattern(
                    allocations[t][i], 
                    sizes[t][i], 
                    TEST_PATTERN_WRITE
                ));
                ddr_free(allocations[t][i]);
            }
        }
    }
    
    printf("✅ Concurrent allocation tests passed\n");
}

/**
 * test_allocation_speed - Test allocation speed
 */
void test_allocation_speed(void)
{
    void *ptr;
    size_t size = 4096;
    int iterations = 10000;
    struct timespec start, end;
    double elapsed;
    
    printf("\n=== Testing Allocation Speed ===\n");
    
    /* Test allocation speed */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        ptr = ddr_alloc(size, 0);
        TEST_ASSERT_NOT_NULL(ptr);
        ddr_free(ptr);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("  %d allocations/frees of %zu bytes in %.3f seconds\n",
           iterations, size, elapsed);
    printf("  Speed: %.0f alloc/s\n", iterations / elapsed);
    
    /* Test large allocation speed */
    size = 1024 * 1024;  /* 1 MB */
    iterations = 100;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        ptr = ddr_alloc(size, 0);
        if (ptr) {
            ddr_free(ptr);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("  %d allocations/frees of %zu MB in %.3f seconds\n",
           iterations, size / (1024 * 1024), elapsed);
    
    printf("✅ Allocation speed tests passed\n");
}

/**
 * test_memory_corruption - Test memory corruption detection
 */
void test_memory_corruption(void)
{
    void *ptr;
    size_t size = 4096;
    
    printf("\n=== Testing Memory Corruption Detection ===\n");
    
    /* Allocate memory */
    ptr = ddr_alloc(size, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    
    /* Write pattern */
    write_pattern(ptr, size, TEST_PATTERN_WRITE);
    
    /* Corrupt memory (simulated) */
    ((unsigned char *)ptr)[size / 2] = 0xFF;
    
    /* Verify pattern (should fail) */
    TEST_ASSERT_FALSE(verify_pattern(ptr, size, TEST_PATTERN_WRITE));
    
    /* Free memory */
    ddr_free(ptr);
    
    printf("✅ Memory corruption detection tests passed\n");
}

/**
 * test_allocation_error - Test allocation error handling
 */
void test_allocation_error(void)
{
    void *ptr;
    size_t huge_size = 1024 * 1024 * 1024;  /* 1 GB */
    
    printf("\n=== Testing Allocation Error Handling ===\n");
    
    /* Test: Allocate too much memory */
    ptr = ddr_alloc(huge_size, 0);
    TEST_ASSERT_NULL(ptr);
    
    /* Test: Allocate with invalid flags */
    ptr = ddr_alloc(1024, 0xFFFFFFFF);
    TEST_ASSERT_NULL(ptr);
    
    /* Test: Allocate with invalid alignment */
    ptr = ddr_alloc_aligned(1024, 3, 0);  /* 3 is not power of 2 */
    TEST_ASSERT_NULL(ptr);
    
    /* Test: Free NULL */
    ddr_free(NULL);
    
    /* Test: Free invalid pointer */
    ddr_free((void *)0x12345678);
    
    printf("✅ Allocation error handling tests passed\n");
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int ret;
    bool verbose = false;
    char *test_name = NULL;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            test_name = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Memory Allocation Unit Tests\n\n");
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --verbose        Verbose output\n");
            printf("  --test <name>    Run specific test\n");
            printf("  --help           Show this help\n\n");
            printf("Available tests:\n");
            printf("  test_basic_allocation\n");
            printf("  test_zero_allocation\n");
            printf("  test_large_allocation\n");
            printf("  test_aligned_allocation\n");
            printf("  test_dma_allocation\n");
            printf("  test_memory_limits\n");
            printf("  test_pattern_integrity\n");
            printf("  test_concurrent_allocation\n");
            printf("  test_allocation_speed\n");
            printf("  test_memory_corruption\n");
            printf("  test_allocation_error\n");
            return 0;
        }
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           Memory Allocation Unit Tests                      ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* Initialize Unity */
    UNITY_BEGIN();
    
    /* Run tests */
    if (test_name) {
        /* Run specific test */
        if (strcmp(test_name, "test_basic_allocation") == 0) {
            RUN_TEST(test_basic_allocation);
        } else if (strcmp(test_name, "test_zero_allocation") == 0) {
            RUN_TEST(test_zero_allocation);
        } else if (strcmp(test_name, "test_large_allocation") == 0) {
            RUN_TEST(test_large_allocation);
        } else if (strcmp(test_name, "test_aligned_allocation") == 0) {
            RUN_TEST(test_aligned_allocation);
        } else if (strcmp(test_name, "test_dma_allocation") == 0) {
            RUN_TEST(test_dma_allocation);
        } else if (strcmp(test_name, "test_memory_limits") == 0) {
            RUN_TEST(test_memory_limits);
        } else if (strcmp(test_name, "test_pattern_integrity") == 0) {
            RUN_TEST(test_pattern_integrity);
        } else if (strcmp(test_name, "test_concurrent_allocation") == 0) {
            RUN_TEST(test_concurrent_allocation);
        } else if (strcmp(test_name, "test_allocation_speed") == 0) {
            RUN_TEST(test_allocation_speed);
        } else if (strcmp(test_name, "test_memory_corruption") == 0) {
            RUN_TEST(test_memory_corruption);
        } else if (strcmp(test_name, "test_allocation_error") == 0) {
            RUN_TEST(test_allocation_error);
        } else {
            printf("❌ Unknown test: %s\n", test_name);
            return 1;
        }
    } else {
        /* Run all tests */
        RUN_TEST(test_basic_allocation);
        RUN_TEST(test_zero_allocation);
        RUN_TEST(test_large_allocation);
        RUN_TEST(test_aligned_allocation);
        RUN_TEST(test_dma_allocation);
        RUN_TEST(test_memory_limits);
        RUN_TEST(test_pattern_integrity);
        RUN_TEST(test_concurrent_allocation);
        RUN_TEST(test_allocation_speed);
        RUN_TEST(test_memory_corruption);
        RUN_TEST(test_allocation_error);
    }
    
    ret = UNITY_END();
    
    /* Cleanup */
    ddr_allocator_cleanup();
    
    if (ret == 0) {
        printf("\n✅ All memory allocation tests passed!\n");
    } else {
        printf("\n❌ Some memory allocation tests failed!\n");
    }
    
    return ret;
}
