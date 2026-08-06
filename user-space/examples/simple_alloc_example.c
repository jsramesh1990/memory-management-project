/**
 * simple_alloc_example.c - Simple Memory Allocation Example
 * 
 * This example demonstrates basic DDR memory allocation and management
 * in user space for RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o simple_alloc_example simple_alloc_example.c -lddr_manager
 * 
 * Usage:
 *   ./simple_alloc_example [options]
 * 
 * Options:
 *   --size <bytes>      Allocation size in bytes (default: 1MB)
 *   --iterations <n>    Number of iterations (default: 100)
 *   --verbose           Verbose output
 *   --test              Run test suite
 *   --help              Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_config.h>
#include <ddr_allocator.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define DEFAULT_SIZE        (1024 * 1024)    /* 1 MB */
#define DEFAULT_ITERATIONS  100
#define MAX_ALLOC_SIZE      (64 * 1024 * 1024) /* 64 MB */
#define MIN_ALLOC_SIZE      64

/* Colors for terminal output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct {
    void *ptr;
    size_t size;
    unsigned int flags;
    double alloc_time_us;
    double free_time_us;
} allocation_record_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * print_header - Print section header
 */
static void print_header(const char *title)
{
    printf("\n");
    printf(COLOR_BLUE "========================================\n" COLOR_RESET);
    printf(COLOR_BLUE "  %s\n" COLOR_RESET, title);
    printf(COLOR_BLUE "========================================\n" COLOR_RESET);
}

/**
 * print_success - Print success message
 */
static void print_success(const char *msg, ...)
{
    va_list args;
    printf(COLOR_GREEN "[SUCCESS] " COLOR_RESET);
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * print_error - Print error message
 */
static void print_error(const char *msg, ...)
{
    va_list args;
    printf(COLOR_RED "[ERROR] " COLOR_RESET);
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * print_info - Print info message
 */
static void print_info(const char *msg, ...)
{
    va_list args;
    printf(COLOR_YELLOW "[INFO] " COLOR_RESET);
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * print_warning - Print warning message
 */
static void print_warning(const char *msg, ...)
{
    va_list args;
    printf(COLOR_MAGENTA "[WARNING] " COLOR_RESET);
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

/**
 * get_time_us - Get current time in microseconds
 */
static double get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

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
static int verify_pattern(void *ptr, size_t size, unsigned char pattern)
{
    unsigned char *bytes = (unsigned char *)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            return 0;
        }
    }
    return 1;
}

/**
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("Simple Memory Allocation Example\n\n");
    printf("Usage: ./simple_alloc_example [options]\n\n");
    printf("Options:\n");
    printf("  --size <bytes>      Allocation size in bytes (default: 1MB)\n");
    printf("  --iterations <n>    Number of iterations (default: 100)\n");
    printf("  --verbose           Verbose output\n");
    printf("  --test              Run test suite\n");
    printf("  --help              Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./simple_alloc_example\n");
    printf("  ./simple_alloc_example --size 1048576 --iterations 50\n");
    printf("  ./simple_alloc_example --test\n");
}

/* ============================================================================
 * Example Functions
 * ============================================================================ */

/**
 * example_basic_allocation - Basic allocation example
 */
static int example_basic_allocation(size_t size)
{
    void *ptr;
    int ret = 0;
    
    print_info("Basic allocation of %zu bytes", size);
    
    /* Allocate memory */
    ptr = ddr_alloc(size, 0);
    if (!ptr) {
        print_error("Failed to allocate %zu bytes", size);
        return -ENOMEM;
    }
    print_success("Allocated %zu bytes at %p", size, ptr);
    
    /* Write pattern */
    write_pattern(ptr, size, 0xAA);
    print_info("Wrote pattern 0xAA to memory");
    
    /* Verify pattern */
    if (verify_pattern(ptr, size, 0xAA)) {
        print_success("Pattern verified successfully");
    } else {
        print_error("Pattern verification failed");
        ret = -EIO;
    }
    
    /* Free memory */
    ddr_free(ptr);
    print_success("Freed memory at %p", ptr);
    
    return ret;
}

/**
 * example_aligned_allocation - Aligned allocation example
 */
static int example_aligned_allocation(size_t size, size_t align)
{
    void *ptr;
    int ret = 0;
    
    print_info("Aligned allocation of %zu bytes with alignment %zu", size, align);
    
    /* Allocate aligned memory */
    ptr = ddr_alloc_aligned(size, align, 0);
    if (!ptr) {
        print_error("Failed to allocate %zu bytes aligned to %zu", size, align);
        return -ENOMEM;
    }
    
    /* Check alignment */
    if ((uintptr_t)ptr % align == 0) {
        print_success("Allocated %zu bytes at %p (aligned to %zu)", 
                     size, ptr, align);
    } else {
        print_error("Allocation not properly aligned");
        ret = -EINVAL;
    }
    
    /* Write and verify pattern */
    write_pattern(ptr, size, 0x55);
    if (verify_pattern(ptr, size, 0x55)) {
        print_success("Pattern verified successfully");
    } else {
        print_error("Pattern verification failed");
        ret = -EIO;
    }
    
    /* Free memory */
    ddr_free(ptr);
    print_success("Freed memory at %p", ptr);
    
    return ret;
}

/**
 * example_dma_allocation - DMA allocation example
 */
static int example_dma_allocation(size_t size)
{
    void *ptr;
    dma_addr_t dma_handle;
    int ret = 0;
    
    print_info("DMA allocation of %zu bytes", size);
    
    /* Allocate DMA memory */
    ptr = ddr_dma_alloc(size, &dma_handle, 0);
    if (!ptr) {
        print_error("Failed to allocate %zu DMA bytes", size);
        return -ENOMEM;
    }
    print_success("Allocated %zu DMA bytes at %p (dma: 0x%08lx)", 
                 size, ptr, (unsigned long)dma_handle);
    
    /* Write and verify pattern */
    write_pattern(ptr, size, 0xCC);
    if (verify_pattern(ptr, size, 0xCC)) {
        print_success("Pattern verified successfully");
    } else {
        print_error("Pattern verification failed");
        ret = -EIO;
    }
    
    /* Free DMA memory */
    ddr_dma_free(ptr, size, dma_handle);
    print_success("Freed DMA memory at %p", ptr);
    
    return ret;
}

/**
 * example_memory_pool - Memory pool example
 */
static int example_memory_pool(size_t size, int count)
{
    void *ptrs[100];
    int allocated = 0;
    int ret = 0;
    
    print_info("Memory pool example: %d allocations of %zu bytes", count, size);
    
    /* Allocate multiple blocks */
    for (int i = 0; i < count && i < 100; i++) {
        ptrs[i] = ddr_alloc(size, 0);
        if (!ptrs[i]) {
            print_warning("Failed to allocate block %d", i);
            break;
        }
        allocated++;
        
        /* Write pattern with different values */
        write_pattern(ptrs[i], size, (i & 0xFF));
    }
    print_success("Allocated %d blocks of %zu bytes", allocated, size);
    
    /* Verify all blocks */
    for (int i = 0; i < allocated; i++) {
        if (!verify_pattern(ptrs[i], size, (i & 0xFF))) {
            print_error("Block %d verification failed", i);
            ret = -EIO;
        }
    }
    
    /* Free all blocks */
    for (int i = 0; i < allocated; i++) {
        ddr_free(ptrs[i]);
    }
    print_success("Freed %d blocks", allocated);
    
    return ret;
}

/**
 * example_performance - Performance test example
 */
static int example_performance(size_t size, int iterations)
{
    void *ptr;
    double start_time, end_time;
    double total_alloc_time = 0;
    double total_free_time = 0;
    double total_rw_time = 0;
    int ret = 0;
    
    print_info("Performance test: %d iterations of %zu bytes", iterations, size);
    
    for (int i = 0; i < iterations; i++) {
        /* Allocation time */
        start_time = get_time_us();
        ptr = ddr_alloc(size, 0);
        end_time = get_time_us();
        
        if (!ptr) {
            print_error("Failed to allocate at iteration %d", i);
            ret = -ENOMEM;
            break;
        }
        total_alloc_time += (end_time - start_time);
        
        /* Read/write time */
        start_time = get_time_us();
        write_pattern(ptr, size, (i & 0xFF));
        if (!verify_pattern(ptr, size, (i & 0xFF))) {
            print_error("Verification failed at iteration %d", i);
            ret = -EIO;
            ddr_free(ptr);
            break;
        }
        end_time = get_time_us();
        total_rw_time += (end_time - start_time);
        
        /* Free time */
        start_time = get_time_us();
        ddr_free(ptr);
        end_time = get_time_us();
        total_free_time += (end_time - start_time);
        
        if ((i + 1) % 10 == 0) {
            printf("  Iteration %d completed\n", i + 1);
        }
    }
    
    if (ret == 0) {
        printf("\n  Performance Summary:\n");
        printf("  Average allocation time: %.2f us\n", 
               total_alloc_time / iterations);
        printf("  Average free time: %.2f us\n", 
               total_free_time / iterations);
        printf("  Average read/write time: %.2f us\n", 
               total_rw_time / iterations);
        printf("  Throughput: %.2f MB/s\n", 
               (double)(size * iterations) / (total_rw_time / 1000000.0) / (1024 * 1024));
    }
    
    return ret;
}

/**
 * example_error_handling - Error handling example
 */
static int example_error_handling(void)
{
    void *ptr;
    int ret = 0;
    
    print_header("Error Handling Example");
    
    /* Test 1: Very large allocation (should fail) */
    print_info("Attempting very large allocation...");
    ptr = ddr_alloc(MAX_ALLOC_SIZE * 1024, 0);
    if (!ptr) {
        print_success("Large allocation failed as expected");
    } else {
        print_error("Large allocation should have failed");
        ddr_free(ptr);
        ret = -1;
    }
    
    /* Test 2: Zero size allocation (should fail) */
    print_info("Attempting zero-size allocation...");
    ptr = ddr_alloc(0, 0);
    if (!ptr) {
        print_success("Zero-size allocation failed as expected");
    } else {
        print_error("Zero-size allocation should have failed");
        ddr_free(ptr);
        ret = -1;
    }
    
    /* Test 3: Free NULL pointer (should handle gracefully) */
    print_info("Freeing NULL pointer...");
    ddr_free(NULL);
    print_success("NULL free handled gracefully");
    
    /* Test 4: Invalid alignment (should fail) */
    print_info("Attempting invalid alignment...");
    ptr = ddr_alloc_aligned(1024, 3, 0);  /* 3 is not power of 2 */
    if (!ptr) {
        print_success("Invalid alignment failed as expected");
    } else {
        print_error("Invalid alignment should have failed");
        ddr_free(ptr);
        ret = -1;
    }
    
    return ret;
}

/**
 * example_lifecycle - Full allocation lifecycle example
 */
static int example_lifecycle(size_t size)
{
    void *ptr;
    dma_addr_t dma_handle;
    int ret = 0;
    
    print_header("Allocation Lifecycle Example");
    
    /* Stage 1: Initialize */
    print_info("Stage 1: Initializing DDR subsystem");
    ret = ddr_config_init(BOARD_MIXTILE_EDGE2);
    if (ret) {
        print_error("Failed to initialize DDR: %d", ret);
        return ret;
    }
    print_success("DDR subsystem initialized");
    
    /* Stage 2: Allocate memory */
    print_info("Stage 2: Allocating memory");
    ptr = ddr_alloc(size, 0);
    if (!ptr) {
        print_error("Failed to allocate memory");
        return -ENOMEM;
    }
    print_success("Memory allocated at %p", ptr);
    
    /* Stage 3: Use memory */
    print_info("Stage 3: Using memory");
    write_pattern(ptr, size, 0xDE);
    if (verify_pattern(ptr, size, 0xDE)) {
        print_success("Memory usage successful");
    } else {
        print_error("Memory usage failed");
        ret = -EIO;
    }
    
    /* Stage 4: Get memory info */
    print_info("Stage 4: Getting memory info");
    struct ddr_info info;
    ret = ddr_config_get_info(&info);
    if (!ret) {
        printf("  Total Memory: %u MB\n", info.total_memory);
        printf("  Used Memory: %u MB\n", info.used_memory);
        printf("  Frequency: %u MHz\n", info.frequency);
        printf("  Voltage: %u mV\n", info.voltage);
        printf("  Temperature: %u C\n", info.temperature);
    }
    
    /* Stage 5: Free memory */
    print_info("Stage 5: Freeing memory");
    ddr_free(ptr);
    print_success("Memory freed");
    
    return ret;
}

/* ============================================================================
 * Test Suite
 * ============================================================================ */

/**
 * run_test_suite - Run complete test suite
 */
static int run_test_suite(void)
{
    int ret = 0;
    int total_tests = 0;
    int passed_tests = 0;
    
    print_header("Test Suite");
    
    /* Test 1: Basic allocation */
    print_info("Test 1: Basic allocation");
    if (example_basic_allocation(DEFAULT_SIZE) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 2: Aligned allocation */
    print_info("Test 2: Aligned allocation");
    if (example_aligned_allocation(DEFAULT_SIZE, 4096) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 3: DMA allocation */
    print_info("Test 3: DMA allocation");
    if (example_dma_allocation(DEFAULT_SIZE) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 4: Memory pool */
    print_info("Test 4: Memory pool");
    if (example_memory_pool(DEFAULT_SIZE / 10, 10) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 5: Performance test */
    print_info("Test 5: Performance test");
    if (example_performance(DEFAULT_SIZE, 10) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 6: Error handling */
    print_info("Test 6: Error handling");
    if (example_error_handling() == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Test 7: Lifecycle */
    print_info("Test 7: Lifecycle");
    if (example_lifecycle(DEFAULT_SIZE) == 0) {
        passed_tests++;
    }
    total_tests++;
    
    /* Summary */
    print_header("Test Summary");
    printf("\n  Total Tests: %d\n", total_tests);
    printf("  Passed: %d\n", passed_tests);
    printf("  Failed: %d\n", total_tests - passed_tests);
    printf("  Success Rate: %.1f%%\n", 
           (double)passed_tests / total_tests * 100.0);
    
    if (passed_tests == total_tests) {
        print_success("All tests passed!");
        return 0;
    } else {
        print_error("Some tests failed!");
        return -1;
    }
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    size_t size = DEFAULT_SIZE;
    int iterations = DEFAULT_ITERATIONS;
    int verbose = 0;
    int run_test = 0;
    int ret = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     Simple Memory Allocation Example               ║\n");
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0) {
            if (i + 1 < argc) {
                size = atol(argv[++i]);
                if (size < MIN_ALLOC_SIZE) {
                    print_error("Size too small, using %d", MIN_ALLOC_SIZE);
                    size = MIN_ALLOC_SIZE;
                }
                if (size > MAX_ALLOC_SIZE) {
                    print_error("Size too large, using %d", MAX_ALLOC_SIZE);
                    size = MAX_ALLOC_SIZE;
                }
            }
        } else if (strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 < argc) {
                iterations = atoi(argv[++i]);
                if (iterations < 1) {
                    iterations = 1;
                }
            }
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--test") == 0) {
            run_test = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else {
            print_error("Unknown option: %s", argv[i]);
            show_help();
            return 1;
        }
    }
    
    /* Initialize DDR subsystem */
    ret = ddr_config_init(BOARD_MIXTILE_EDGE2);
    if (ret) {
        print_error("Failed to initialize DDR: %d", ret);
        return ret;
    }
    print_success("DDR subsystem initialized");
    
    /* Run test suite or examples */
    if (run_test) {
        ret = run_test_suite();
    } else {
        /* Run examples */
        print_header("Examples");
        
        ret = example_basic_allocation(size);
        if (ret) {
            print_error("Basic allocation example failed");
            goto cleanup;
        }
        
        ret = example_aligned_allocation(size, 4096);
        if (ret) {
            print_error("Aligned allocation example failed");
            goto cleanup;
        }
        
        ret = example_dma_allocation(size);
        if (ret) {
            print_error("DMA allocation example failed");
            goto cleanup;
        }
        
        ret = example_memory_pool(size / 10, iterations);
        if (ret) {
            print_error("Memory pool example failed");
            goto cleanup;
        }
        
        ret = example_performance(size, iterations);
        if (ret) {
            print_error("Performance example failed");
            goto cleanup;
        }
        
        ret = example_error_handling();
        if (ret) {
            print_error("Error handling example failed");
            goto cleanup;
        }
        
        ret = example_lifecycle(size);
        if (ret) {
            print_error("Lifecycle example failed");
            goto cleanup;
        }
    }
    
cleanup:
    /* Clean up DDR subsystem */
    ddr_allocator_cleanup();
    
    if (ret == 0) {
        print_success("Example completed successfully!");
    } else {
        print_error("Example failed with error %d", ret);
    }
    
    return ret;
}
