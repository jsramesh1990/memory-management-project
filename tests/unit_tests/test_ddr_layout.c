/**
 * test_ddr_layout.c - Unit Tests for DDR Layout Management
 * 
 * This file contains unit tests for DDR layout validation,
 * region management, and layout operations.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Compilation:
 *   gcc -o test_ddr_layout test_ddr_layout.c -lunity -lddr_manager
 * 
 * Usage:
 *   ./test_ddr_layout
 *   ./test_ddr_layout --verbose
 *   ./test_ddr_layout --test <test_name>
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
#include <ddr_layout.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEST_REGION_NAME_MAX    32
#define TEST_MAX_REGIONS        16
#define TEST_MEMORY_SIZE        (1024 * 1024 * 64)  /* 64 MB */
#define TEST_ALIGNMENT          4096

/* ============================================================================
 * Test Data
 * ============================================================================ */

/* Test regions */
static struct ddr_region test_regions[] = {
    {
        .name = "test_boot",
        .start = 0x00000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
    },
    {
        .name = "test_kernel",
        .start = 0x02000000,
        .size = 0x0E000000,      /* 224 MB */
        .flags = 0,
    },
    {
        .name = "test_dtb",
        .start = 0x10000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
    },
    {
        .name = "test_npu",
        .start = 0x20000000,
        .size = 0x08000000,      /* 128 MB */
        .flags = 0,
    },
    {
        .name = "test_gpu",
        .start = 0x28000000,
        .size = 0x08000000,      /* 128 MB */
        .flags = 0,
    },
    {
        .name = "test_system",
        .start = 0x40000000,
        .size = 0x40000000,      /* 1 GB */
        .flags = 0,
    },
};

/* ============================================================================
 * Test Setup and Teardown
 * ============================================================================ */

void setUp(void)
{
    /* Initialize DDR layout for testing */
    printf("Setting up DDR layout test environment...\n");
}

void tearDown(void)
{
    /* Clean up DDR layout */
    printf("Cleaning up DDR layout test environment...\n");
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * compare_regions - Compare two DDR regions
 */
static bool compare_regions(struct ddr_region *r1, struct ddr_region *r2)
{
    if (r1->start != r2->start) return false;
    if (r1->size != r2->size) return false;
    if (r1->flags != r2->flags) return false;
    if (strcmp(r1->name, r2->name) != 0) return false;
    return true;
}

/**
 * create_test_layout - Create a test DDR layout
 */
static struct ddr_layout *create_test_layout(void)
{
    struct ddr_layout *layout;
    
    layout = malloc(sizeof(*layout));
    if (!layout) {
        return NULL;
    }
    
    memset(layout, 0, sizeof(*layout));
    
    /* Copy test regions */
    memcpy(&layout->bootloader, &test_regions[0], sizeof(struct ddr_region));
    memcpy(&layout->kernel, &test_regions[1], sizeof(struct ddr_region));
    memcpy(&layout->dtb, &test_regions[2], sizeof(struct ddr_region));
    memcpy(&layout->npu, &test_regions[3], sizeof(struct ddr_region));
    memcpy(&layout->gpu, &test_regions[4], sizeof(struct ddr_region));
    memcpy(&layout->system, &test_regions[5], sizeof(struct ddr_region));
    
    return layout;
}

/**
 * destroy_test_layout - Destroy a test DDR layout
 */
static void destroy_test_layout(struct ddr_layout *layout)
{
    if (layout) {
        free(layout);
    }
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * test_layout_init - Test layout initialization
 */
void test_layout_init(void)
{
    int ret;
    struct ddr_layout layout;
    
    printf("\n=== Testing Layout Initialization ===\n");
    
    /* Test: Initialize layout for Mixtile Edge 2 */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Initialize layout for Radxa ROCK 3B */
    ret = ddr_layout_init(BOARD_RADXA_ROCK3B);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Initialize layout for Orange Pi 5 */
    ret = ddr_layout_init(BOARD_ORANGE_PI_5);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Initialize layout for custom board */
    ret = ddr_layout_init(BOARD_CUSTOM);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Invalid board type */
    ret = ddr_layout_init(999);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Get layout */
    memset(&layout, 0, sizeof(layout));
    ret = ddr_layout_get(&layout);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(layout.system.start > 0);
    TEST_ASSERT(layout.system.size > 0);
    
    printf("✅ Layout initialization tests passed\n");
}

/**
 * test_layout_validation - Test layout validation
 */
void test_layout_validation(void)
{
    int ret;
    struct ddr_layout *layout;
    
    printf("\n=== Testing Layout Validation ===\n");
    
    /* Create test layout */
    layout = create_test_layout();
    TEST_ASSERT_NOT_NULL(layout);
    
    /* Test: Valid layout */
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Layout with overlapping regions */
    layout->kernel.start = 0x00000000;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Layout with zero-size region */
    layout->kernel.size = 0;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Layout with invalid start address */
    layout->kernel.start = 0;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Clean up */
    destroy_test_layout(layout);
    
    printf("✅ Layout validation tests passed\n");
}

/**
 * test_region_info - Test region info retrieval
 */
void test_region_info(void)
{
    int ret;
    phys_addr_t start;
    phys_size_t size;
    struct ddr_layout layout;
    
    printf("\n=== Testing Region Info Retrieval ===\n");
    
    /* Initialize layout */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Get system region info */
    ret = ddr_layout_region_info("system", &start, &size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    TEST_ASSERT(size > 0);
    printf("  System: 0x%08lx - 0x%08lx (%lu MB)\n",
           (unsigned long)start,
           (unsigned long)(start + size),
           (unsigned long)size / (1024 * 1024));
    
    /* Test: Get NPU region info */
    ret = ddr_layout_region_info("npu", &start, &size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    TEST_ASSERT(size > 0);
    printf("  NPU: 0x%08lx - 0x%08lx (%lu MB)\n",
           (unsigned long)start,
           (unsigned long)(start + size),
           (unsigned long)size / (1024 * 1024));
    
    /* Test: Get GPU region info */
    ret = ddr_layout_region_info("gpu", &start, &size);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    TEST_ASSERT(size > 0);
    printf("  GPU: 0x%08lx - 0x%08lx (%lu MB)\n",
           (unsigned long)start,
           (unsigned long)(start + size),
           (unsigned long)size / (1024 * 1024));
    
    /* Test: Invalid region name */
    ret = ddr_layout_region_info("invalid", NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: NULL parameters */
    ret = ddr_layout_region_info(NULL, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Region info tests passed\n");
}

/**
 * test_layout_set - Test layout setting
 */
void test_layout_set(void)
{
    int ret;
    struct ddr_layout *layout;
    struct ddr_layout retrieved;
    
    printf("\n=== Testing Layout Setting ===\n");
    
    /* Create test layout */
    layout = create_test_layout();
    TEST_ASSERT_NOT_NULL(layout);
    
    /* Test: Set layout */
    ret = ddr_layout_set(layout);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Get layout back */
    memset(&retrieved, 0, sizeof(retrieved));
    ret = ddr_layout_get(&retrieved);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Compare layouts */
    TEST_ASSERT_EQUAL(layout->system.start, retrieved.system.start);
    TEST_ASSERT_EQUAL(layout->system.size, retrieved.system.size);
    TEST_ASSERT_EQUAL(layout->npu.start, retrieved.npu.start);
    TEST_ASSERT_EQUAL(layout->npu.size, retrieved.npu.size);
    
    /* Clean up */
    destroy_test_layout(layout);
    
    printf("✅ Layout setting tests passed\n");
}

/**
 * test_find_free - Test finding free memory
 */
void test_find_free(void)
{
    int ret;
    phys_addr_t start;
    size_t size;
    struct ddr_layout layout;
    
    printf("\n=== Testing Free Memory Finding ===\n");
    
    /* Initialize layout */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Find 1 MB free */
    size = 1024 * 1024;
    ret = ddr_layout_find_free(size, &start, 0);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    printf("  Found 1 MB at 0x%08lx\n", (unsigned long)start);
    
    /* Test: Find 10 MB free */
    size = 10 * 1024 * 1024;
    ret = ddr_layout_find_free(size, &start, 0);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    printf("  Found 10 MB at 0x%08lx\n", (unsigned long)start);
    
    /* Test: Find 100 MB free */
    size = 100 * 1024 * 1024;
    ret = ddr_layout_find_free(size, &start, 0);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(start > 0);
    printf("  Found 100 MB at 0x%08lx\n", (unsigned long)start);
    
    /* Test: Find too large */
    size = 10 * 1024 * 1024 * 1024;  /* 10 GB */
    ret = ddr_layout_find_free(size, &start, 0);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Invalid parameters */
    ret = ddr_layout_find_free(0, &start, 0);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    ret = ddr_layout_find_free(size, NULL, 0);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Free memory finding tests passed\n");
}

/**
 * test_layout_print - Test layout printing
 */
void test_layout_print(void)
{
    int ret;
    
    printf("\n=== Testing Layout Printing ===\n");
    
    /* Initialize layout */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Print layout (just check it doesn't crash) */
    ddr_layout_print();
    
    /* Test: Print with no layout initialized */
    ddr_layout_cleanup();
    ddr_layout_print();
    
    /* Re-initialize for other tests */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    printf("✅ Layout printing tests passed\n");
}

/**
 * test_layout_cleanup - Test layout cleanup
 */
void test_layout_cleanup(void)
{
    int ret;
    struct ddr_layout layout;
    
    printf("\n=== Testing Layout Cleanup ===\n");
    
    /* Initialize layout */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Get layout */
    memset(&layout, 0, sizeof(layout));
    ret = ddr_layout_get(&layout);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(layout.system.start > 0);
    
    /* Test: Cleanup */
    ddr_layout_cleanup();
    
    /* Test: Get layout after cleanup */
    memset(&layout, 0, sizeof(layout));
    ret = ddr_layout_get(&layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Re-initialize after cleanup */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    printf("✅ Layout cleanup tests passed\n");
}

/**
 * test_layout_edge_cases - Test edge cases
 */
void test_layout_edge_cases(void)
{
    int ret;
    struct ddr_layout *layout;
    phys_addr_t start;
    phys_size_t size;
    
    printf("\n=== Testing Edge Cases ===\n");
    
    /* Create test layout */
    layout = create_test_layout();
    TEST_ASSERT_NOT_NULL(layout);
    
    /* Test: Very large region size */
    layout->system.size = 0xFFFFFFFFFFFFFFFFULL;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Very small region size */
    layout->system.size = 1;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Maximum address */
    layout->system.start = 0xFFFFFFFFFFFFFFFFULL;
    ret = ddr_layout_validate(layout);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Region name too long */
    char long_name[256];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    ret = ddr_layout_region_info(long_name, &start, &size);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Clean up */
    destroy_test_layout(layout);
    
    printf("✅ Edge case tests passed\n");
}

/**
 * test_layout_performance - Test layout performance
 */
void test_layout_performance(void)
{
    int ret;
    struct ddr_layout layout;
    struct timespec start, end;
    double elapsed;
    int iterations = 10000;
    int i;
    
    printf("\n=== Testing Layout Performance ===\n");
    
    /* Initialize layout */
    ret = ddr_layout_init(BOARD_MIXTILE_EDGE2);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Layout get performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        memset(&layout, 0, sizeof(layout));
        ret = ddr_layout_get(&layout);
        TEST_ASSERT_EQUAL(0, ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Layout get: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    /* Test: Region info performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        ret = ddr_layout_region_info("system", NULL, NULL);
        TEST_ASSERT_EQUAL(0, ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Region info: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    /* Test: Find free performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        ret = ddr_layout_find_free(1024 * 1024, NULL, 0);
        TEST_ASSERT_EQUAL(0, ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Find free: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    printf("✅ Performance tests passed\n");
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
            printf("DDR Layout Unit Tests\n\n");
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --verbose        Verbose output\n");
            printf("  --test <name>    Run specific test\n");
            printf("  --help           Show this help\n\n");
            printf("Available tests:\n");
            printf("  test_layout_init\n");
            printf("  test_layout_validation\n");
            printf("  test_region_info\n");
            printf("  test_layout_set\n");
            printf("  test_find_free\n");
            printf("  test_layout_print\n");
            printf("  test_layout_cleanup\n");
            printf("  test_layout_edge_cases\n");
            printf("  test_layout_performance\n");
            return 0;
        }
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              DDR Layout Unit Tests                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* Initialize Unity */
    UNITY_BEGIN();
    
    /* Run tests */
    if (test_name) {
        /* Run specific test */
        if (strcmp(test_name, "test_layout_init") == 0) {
            RUN_TEST(test_layout_init);
        } else if (strcmp(test_name, "test_layout_validation") == 0) {
            RUN_TEST(test_layout_validation);
        } else if (strcmp(test_name, "test_region_info") == 0) {
            RUN_TEST(test_region_info);
        } else if (strcmp(test_name, "test_layout_set") == 0) {
            RUN_TEST(test_layout_set);
        } else if (strcmp(test_name, "test_find_free") == 0) {
            RUN_TEST(test_find_free);
        } else if (strcmp(test_name, "test_layout_print") == 0) {
            RUN_TEST(test_layout_print);
        } else if (strcmp(test_name, "test_layout_cleanup") == 0) {
            RUN_TEST(test_layout_cleanup);
        } else if (strcmp(test_name, "test_layout_edge_cases") == 0) {
            RUN_TEST(test_layout_edge_cases);
        } else if (strcmp(test_name, "test_layout_performance") == 0) {
            RUN_TEST(test_layout_performance);
        } else {
            printf("❌ Unknown test: %s\n", test_name);
            return 1;
        }
    } else {
        /* Run all tests */
        RUN_TEST(test_layout_init);
        RUN_TEST(test_layout_validation);
        RUN_TEST(test_region_info);
        RUN_TEST(test_layout_set);
        RUN_TEST(test_find_free);
        RUN_TEST(test_layout_print);
        RUN_TEST(test_layout_cleanup);
        RUN_TEST(test_layout_edge_cases);
        RUN_TEST(test_layout_performance);
    }
    
    ret = UNITY_END();
    
    /* Cleanup */
    ddr_layout_cleanup();
    
    if (ret == 0) {
        printf("\n✅ All DDR layout tests passed!\n");
    } else {
        printf("\n❌ Some DDR layout tests failed!\n");
    }
    
    return ret;
}
