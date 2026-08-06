/**
 * basic_config_example.c - Basic DDR Configuration Example
 * 
 * This example demonstrates how to initialize and configure DDR memory
 * on RK3568-based systems. It covers basic setup, timing configuration,
 * and memory testing.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o basic_config_example basic_config_example.c -lddr_manager
 * 
 * Usage:
 *   ./basic_config_example [options]
 * 
 * Options:
 *   --board <type>    Board type (edge2, rock3b, orange5, custom)
 *   --test            Run memory test after configuration
 *   --help            Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_config.h>
#include <ddr_debug.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define DDR_MEMORY_SIZE      (1024 * 1024 * 64)  /* 64 MB test buffer */
#define DDR_TEST_PATTERN     0xDEADBEEF
#define DDR_TEST_ITERATIONS  1000

/* Colors for terminal output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * print_header - Print section header
 * @title: Section title
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
 * @msg: Message to print
 */
static void print_success(const char *msg)
{
    printf(COLOR_GREEN "[SUCCESS] %s\n" COLOR_RESET, msg);
}

/**
 * print_error - Print error message
 * @msg: Message to print
 */
static void print_error(const char *msg)
{
    printf(COLOR_RED "[ERROR] %s\n" COLOR_RESET, msg);
}

/**
 * print_info - Print info message
 * @msg: Message to print
 */
static void print_info(const char *msg)
{
    printf(COLOR_YELLOW "[INFO] %s\n" COLOR_RESET, msg);
}

/**
 * parse_board_type - Parse board type from string
 * @str: Board type string
 * Return: Board type enum
 */
static enum board_type parse_board_type(const char *str)
{
    if (strcmp(str, "edge2") == 0)
        return BOARD_MIXTILE_EDGE2;
    else if (strcmp(str, "rock3b") == 0)
        return BOARD_RADXA_ROCK3B;
    else if (strcmp(str, "orange5") == 0)
        return BOARD_ORANGE_PI_5;
    else if (strcmp(str, "custom") == 0)
        return BOARD_CUSTOM;
    else
        return BOARD_MIXTILE_EDGE2;  /* Default */
}

/**
 * get_board_name - Get board name string
 * @board: Board type
 * Return: Board name string
 */
static const char *get_board_name(enum board_type board)
{
    switch (board) {
        case BOARD_MIXTILE_EDGE2:   return "Mixtile Edge 2";
        case BOARD_RADXA_ROCK3B:    return "Radxa ROCK 3B";
        case BOARD_ORANGE_PI_5:     return "Orange Pi 5";
        case BOARD_CUSTOM:          return "Custom Board";
        default:                    return "Unknown";
    }
}

/**
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("Basic DDR Configuration Example\n\n");
    printf("Usage: ./basic_config_example [options]\n\n");
    printf("Options:\n");
    printf("  --board <type>    Board type (edge2, rock3b, orange5, custom)\n");
    printf("  --test            Run memory test after configuration\n");
    printf("  --help            Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./basic_config_example --board edge2\n");
    printf("  ./basic_config_example --board rock3b --test\n");
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/**
 * ddr_basic_init - Basic DDR initialization
 * @board: Board type
 * Return: 0 on success, negative error code on failure
 */
static int ddr_basic_init(enum board_type board)
{
    int ret;
    
    print_header("DDR Initialization");
    
    print_info("Board: %s", get_board_name(board));
    
    /* Initialize DDR configuration */
    ret = ddr_config_init(board);
    if (ret) {
        print_error("Failed to initialize DDR config: %d", ret);
        return ret;
    }
    print_success("DDR config initialized");
    
    /* Get DDR information */
    struct ddr_info info;
    ret = ddr_config_get_info(&info);
    if (ret) {
        print_error("Failed to get DDR info: %d", ret);
        return ret;
    }
    
    /* Print DDR information */
    printf("\n  DDR Information:\n");
    printf("  - Frequency: %u MHz\n", info.frequency);
    printf("  - Voltage: %u mV\n", info.voltage);
    printf("  - Timings: CL%d-tRCD%d-tRP%d-tRAS%d\n",
           info.timings.tCL, info.timings.tRCD,
           info.timings.tRP, info.timings.tRAS);
    printf("  - Total Memory: %u MB\n", info.total_memory);
    printf("  - Available: %u MB\n", info.available);
    
    return 0;
}

/**
 * ddr_configure_custom - Configure custom DDR settings
 * Return: 0 on success, negative error code on failure
 */
static int ddr_configure_custom(void)
{
    int ret;
    
    print_header("Custom DDR Configuration");
    
    /* Set custom timings */
    struct ddr_timings timings = {
        .tCL = 20,
        .tRCD = 20,
        .tRP = 20,
        .tRAS = 48,
        .tRFC = 350,
        .tRRD = 4,
        .tWTR = 4,
        .tFAW = 16,
    };
    
    ret = ddr_config_set_timings(&timings);
    if (ret) {
        print_error("Failed to set timings: %d", ret);
        return ret;
    }
    print_success("Timings set: CL%d-tRCD%d-tRP%d-tRAS%d",
                  timings.tCL, timings.tRCD, timings.tRP, timings.tRAS);
    
    /* Set frequency */
    ret = ddr_config_set_frequency(1800);
    if (ret) {
        print_error("Failed to set frequency: %d", ret);
        return ret;
    }
    print_success("Frequency set: 1800 MHz");
    
    /* Set voltage */
    ret = ddr_config_set_voltage(1100);
    if (ret) {
        print_error("Failed to set voltage: %d", ret);
        return ret;
    }
    print_success("Voltage set: 1100 mV");
    
    /* Save configuration */
    ret = ddr_config_save("/tmp/ddr_config.json");
    if (ret) {
        print_error("Failed to save config: %d", ret);
        return ret;
    }
    print_success("Configuration saved to /tmp/ddr_config.json");
    
    return 0;
}

/**
 * ddr_memory_test - Test DDR memory
 * Return: 0 on success, negative error code on failure
 */
static int ddr_memory_test(void)
{
    int ret;
    void *buffer;
    uint32_t *ptr;
    size_t i;
    struct timespec start, end;
    double elapsed;
    
    print_header("DDR Memory Test");
    
    print_info("Allocating %u MB test buffer", DDR_MEMORY_SIZE / (1024 * 1024));
    
    /* Allocate memory */
    buffer = malloc(DDR_MEMORY_SIZE);
    if (!buffer) {
        print_error("Failed to allocate memory");
        return -ENOMEM;
    }
    print_success("Memory allocated at %p", buffer);
    
    /* Write test */
    print_info("Writing test pattern...");
    ptr = (uint32_t *)buffer;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < DDR_MEMORY_SIZE / sizeof(uint32_t); i++) {
        ptr[i] = DDR_TEST_PATTERN ^ i;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    print_success("Write completed in %.3f seconds", elapsed);
    printf("  - Write speed: %.2f MB/s\n",
           DDR_MEMORY_SIZE / (elapsed * 1024 * 1024));
    
    /* Read verify */
    print_info("Verifying test pattern...");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < DDR_MEMORY_SIZE / sizeof(uint32_t); i++) {
        uint32_t expected = DDR_TEST_PATTERN ^ i;
        if (ptr[i] != expected) {
            print_error("Mismatch at offset %zu: expected 0x%08X, got 0x%08X",
                        i * sizeof(uint32_t), expected, ptr[i]);
            free(buffer);
            return -EIO;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    print_success("Verification completed in %.3f seconds", elapsed);
    printf("  - Read speed: %.2f MB/s\n",
           DDR_MEMORY_SIZE / (elapsed * 1024 * 1024));
    
    /* Free memory */
    free(buffer);
    print_success("Memory test passed!");
    
    return 0;
}

/**
 * ddr_performance_test - Run performance tests
 * Return: 0 on success, negative error code on failure
 */
static int ddr_performance_test(void)
{
    int ret;
    void *buffer1, *buffer2;
    size_t size = 1024 * 1024;  /* 1 MB */
    struct timespec start, end;
    double elapsed;
    int i;
    
    print_header("DDR Performance Test");
    
    print_info("Allocating buffers for performance test...");
    buffer1 = malloc(size);
    buffer2 = malloc(size);
    if (!buffer1 || !buffer2) {
        print_error("Failed to allocate buffers");
        free(buffer1);
        free(buffer2);
        return -ENOMEM;
    }
    
    /* Test sequential read/write */
    print_info("Sequential Read/Write Test (1 MB)...");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < 1000; i++) {
        memcpy(buffer2, buffer1, size);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  - Total: %.3f seconds\n", elapsed);
    printf("  - Throughput: %.2f MB/s\n",
           (size * 1000) / (elapsed * 1024 * 1024));
    
    /* Test random access */
    print_info("Random Access Test (1 MB)...");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    volatile uint8_t sum = 0;
    for (i = 0; i < 1000000; i++) {
        size_t offset = rand() % size;
        sum += ((uint8_t *)buffer1)[offset];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  - Random access: %d iterations in %.3f seconds\n", 1000000, elapsed);
    printf("  - Access speed: %.2f ns/access\n",
           (elapsed * 1e9) / 1000000);
    
    free(buffer1);
    free(buffer2);
    print_success("Performance test completed");
    
    return 0;
}

/**
 * ddr_monitor_demo - Demonstrate monitoring capabilities
 * Return: 0 on success, negative error code on failure
 */
static int ddr_monitor_demo(void)
{
    int ret;
    struct ddr_monitor_info info;
    
    print_header("DDR Monitoring");
    
    /* Get monitor info */
    ret = ddr_monitor_get_info(&info);
    if (ret) {
        print_error("Failed to get monitor info: %d", ret);
        return ret;
    }
    
    printf("\n  Memory Usage:\n");
    printf("  - Total: %u MB\n", info.total_memory);
    printf("  - Used: %u MB\n", info.used_memory);
    printf("  - Free: %u MB\n", info.free_memory);
    printf("  - Usage: %d%%\n", info.usage_percent);
    
    printf("\n  Performance Metrics:\n");
    printf("  - Average alloc time: %u us\n", info.avg_alloc_time);
    printf("  - Average free time: %u us\n", info.avg_free_time);
    
    printf("\n  Error Counters:\n");
    printf("  - ECC errors: %u\n", info.ecc_errors);
    printf("  - Overflow errors: %u\n", info.overflow_errors);
    printf("  - Alignment errors: %u\n", info.alignment_errors);
    
    print_success("Monitoring demo completed");
    
    return 0;
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int ret;
    enum board_type board = BOARD_MIXTILE_EDGE2;
    int run_test = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     RK3568 DDR Memory Manager - Basic Example      ║\n");
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--board") == 0) {
            if (i + 1 < argc) {
                board = parse_board_type(argv[++i]);
            }
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
    
    /* Initialize DDR */
    ret = ddr_basic_init(board);
    if (ret) {
        print_error("DDR initialization failed: %d", ret);
        return 1;
    }
    
    /* Configure custom settings */
    ret = ddr_configure_custom();
    if (ret) {
        print_error("Custom configuration failed: %d", ret);
        return 1;
    }
    
    /* Run memory test if requested */
    if (run_test) {
        ret = ddr_memory_test();
        if (ret) {
            print_error("Memory test failed: %d", ret);
            return 1;
        }
        
        ret = ddr_performance_test();
        if (ret) {
            print_error("Performance test failed: %d", ret);
            return 1;
        }
    }
    
    /* Monitoring demo */
    ret = ddr_monitor_demo();
    if (ret) {
        print_error("Monitoring demo failed: %d", ret);
        return 1;
    }
    
    printf("\n");
    print_header("Example Completed Successfully");
    printf("\n");
    
    return 0;
}
