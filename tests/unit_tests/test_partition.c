/**
 * test_partition.c - Unit Tests for Partition Management
 * 
 * This file contains unit tests for DDR partition management,
 * including creation, deletion, and manipulation of partitions.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Compilation:
 *   gcc -o test_partition test_partition.c -lunity -lddr_manager
 * 
 * Usage:
 *   ./test_partition
 *   ./test_partition --verbose
 *   ./test_partition --test <test_name>
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
#include <ddr_partition.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEST_PARTITION_NAME_MAX    32
#define TEST_MAX_PARTITIONS        16
#define TEST_PARTITION_SIZE        (1024 * 1024)  /* 1 MB */
#define TEST_PARTITION_ALIGN       4096

/* ============================================================================
 * Test Data
 * ============================================================================ */

static struct ddr_partition test_partitions[] = {
    {
        .name = "test_boot",
        .start = 0x00000000,
        .size = 0x01000000,
        .type = PART_TYPE_BOOTLOADER,
        .flags = PART_FLAG_READONLY | PART_FLAG_BOOTABLE,
    },
    {
        .name = "test_kernel",
        .start = 0x02000000,
        .size = 0x0E000000,
        .type = PART_TYPE_KERNEL,
        .flags = PART_FLAG_READONLY,
    },
    {
        .name = "test_system",
        .start = 0x40000000,
        .size = 0x40000000,
        .type = PART_TYPE_SYSTEM,
        .flags = 0,
    },
    {
        .name = "test_user",
        .start = 0x80000000,
        .size = 0x10000000,
        .type = PART_TYPE_USER,
        .flags = 0,
    },
};

/* ============================================================================
 * Test Setup and Teardown
 * ============================================================================ */

void setUp(void)
{
    /* Initialize partition manager for testing */
    printf("Setting up partition test environment...\n");
    ddr_partition_init();
}

void tearDown(void)
{
    /* Clean up partitions */
    printf("Cleaning up partition test environment...\n");
    ddr_partition_cleanup();
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * compare_partitions - Compare two DDR partitions
 */
static bool compare_partitions(struct ddr_partition *p1, struct ddr_partition *p2)
{
    if (p1->start != p2->start) return false;
    if (p1->size != p2->size) return false;
    if (p1->type != p2->type) return false;
    if (p1->flags != p2->flags) return false;
    if (strcmp(p1->name, p2->name) != 0) return false;
    return true;
}

/**
 * create_test_partition - Create a test partition
 */
static struct ddr_partition create_test_partition(const char *name, 
                                                   phys_addr_t start,
                                                   phys_size_t size,
                                                   unsigned int type,
                                                   unsigned int flags)
{
    struct ddr_partition part;
    memset(&part, 0, sizeof(part));
    strncpy(part.name, name, sizeof(part.name) - 1);
    part.start = start;
    part.size = size;
    part.type = type;
    part.flags = flags;
    return part;
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * test_partition_init - Test partition initialization
 */
void test_partition_init(void)
{
    unsigned int count;
    
    printf("\n=== Testing Partition Initialization ===\n");
    
    /* Test: Initialize partition table */
    ddr_partition_cleanup();
    int ret = ddr_partition_init();
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Get partition count */
    count = ddr_partition_get_count();
    TEST_ASSERT(count > 0);
    printf("  Initialized with %u partitions\n", count);
    
    /* Test: Initialize again (should succeed) */
    ret = ddr_partition_init();
    TEST_ASSERT_EQUAL(0, ret);
    
    printf("✅ Partition initialization tests passed\n");
}

/**
 * test_partition_add - Test adding partitions
 */
void test_partition_add(void)
{
    int id;
    struct ddr_partition part;
    struct ddr_partition retrieved;
    
    printf("\n=== Testing Partition Addition ===\n");
    
    /* Create test partition */
    part = create_test_partition("test_add", 0x90000000, 0x01000000,
                                 PART_TYPE_USER, 0);
    
    /* Test: Add partition */
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    printf("  Added partition with ID %d\n", id);
    
    /* Test: Get partition by ID */
    memset(&retrieved, 0, sizeof(retrieved));
    int ret = ddr_partition_get(id, &retrieved);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(compare_partitions(&part, &retrieved));
    
    /* Test: Add duplicate partition */
    id = ddr_partition_add(&part);
    TEST_ASSERT(id < 0);
    
    /* Test: Add partition with invalid parameters */
    struct ddr_partition invalid;
    memset(&invalid, 0, sizeof(invalid));
    id = ddr_partition_add(&invalid);
    TEST_ASSERT(id < 0);
    
    /* Test: Add partition with NULL name */
    strcpy(invalid.name, "");
    invalid.start = 0x91000000;
    invalid.size = 0x01000000;
    id = ddr_partition_add(&invalid);
    TEST_ASSERT(id < 0);
    
    printf("✅ Partition addition tests passed\n");
}

/**
 * test_partition_remove - Test removing partitions
 */
void test_partition_remove(void)
{
    int id;
    int ret;
    struct ddr_partition part;
    struct ddr_partition retrieved;
    
    printf("\n=== Testing Partition Removal ===\n");
    
    /* Add a test partition */
    part = create_test_partition("test_remove", 0x92000000, 0x01000000,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    
    /* Test: Remove partition */
    ret = ddr_partition_remove(id);
    TEST_ASSERT_EQUAL(0, ret);
    
    /* Test: Get removed partition (should fail) */
    memset(&retrieved, 0, sizeof(retrieved));
    ret = ddr_partition_get(id, &retrieved);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Remove non-existent partition */
    ret = ddr_partition_remove(9999);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Partition removal tests passed\n");
}

/**
 * test_partition_get - Test getting partitions
 */
void test_partition_get(void)
{
    int id;
    int ret;
    struct ddr_partition part;
    struct ddr_partition retrieved;
    
    printf("\n=== Testing Partition Retrieval ===\n");
    
    /* Add a test partition */
    part = create_test_partition("test_get", 0x93000000, 0x01000000,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    
    /* Test: Get partition by ID */
    memset(&retrieved, 0, sizeof(retrieved));
    ret = ddr_partition_get(id, &retrieved);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(compare_partitions(&part, &retrieved));
    
    /* Test: Get partition with invalid ID */
    ret = ddr_partition_get(9999, &retrieved);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Get partition with NULL output */
    ret = ddr_partition_get(id, NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Partition retrieval tests passed\n");
}

/**
 * test_partition_find - Test finding partitions
 */
void test_partition_find(void)
{
    int id;
    int ret;
    struct ddr_partition part;
    struct ddr_partition found;
    
    printf("\n=== Testing Partition Finding ===\n");
    
    /* Add a test partition */
    part = create_test_partition("test_find", 0x94000000, 0x01000000,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    
    /* Test: Find partition by name */
    memset(&found, 0, sizeof(found));
    ret = ddr_partition_find("test_find", &found);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT(compare_partitions(&part, &found));
    
    /* Test: Find non-existent partition */
    ret = ddr_partition_find("nonexistent", &found);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Find with NULL output */
    ret = ddr_partition_find("test_find", NULL);
    TEST_ASSERT_EQUAL(id, ret);
    
    /* Test: Find with NULL name */
    ret = ddr_partition_find(NULL, &found);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Partition finding tests passed\n");
}

/**
 * test_partition_by_type - Test getting partitions by type
 */
void test_partition_by_type(void)
{
    int ret;
    unsigned int count;
    struct ddr_partition part;
    
    printf("\n=== Testing Partition Retrieval by Type ===\n");
    
    /* Test: Get system partition */
    memset(&part, 0, sizeof(part));
    ret = ddr_partition_get_by_type(PART_TYPE_SYSTEM, 0, &part);
    TEST_ASSERT_EQUAL(0, ret);
    printf("  System partition: %s (0x%08lx - 0x%08lx)\n",
           part.name, (unsigned long)part.start,
           (unsigned long)(part.start + part.size));
    
    /* Test: Get kernel partition */
    memset(&part, 0, sizeof(part));
    ret = ddr_partition_get_by_type(PART_TYPE_KERNEL, 0, &part);
    TEST_ASSERT_EQUAL(0, ret);
    printf("  Kernel partition: %s (0x%08lx - 0x%08lx)\n",
           part.name, (unsigned long)part.start,
           (unsigned long)(part.start + part.size));
    
    /* Test: Get bootloader partition */
    memset(&part, 0, sizeof(part));
    ret = ddr_partition_get_by_type(PART_TYPE_BOOTLOADER, 0, &part);
    TEST_ASSERT_EQUAL(0, ret);
    printf("  Bootloader partition: %s (0x%08lx - 0x%08lx)\n",
           part.name, (unsigned long)part.start,
           (unsigned long)(part.start + part.size));
    
    /* Test: Get non-existent partition type */
    ret = ddr_partition_get_by_type(0xFFFF, 0, &part);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Get partition with invalid index */
    ret = ddr_partition_get_by_type(PART_TYPE_SYSTEM, 100, &part);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    printf("✅ Partition type retrieval tests passed\n");
}

/**
 * test_partition_dump - Test partition dumping
 */
void test_partition_dump(void)
{
    printf("\n=== Testing Partition Dumping ===\n");
    
    /* Test: Dump partition table (just check it doesn't crash) */
    ddr_partition_dump();
    
    /* Test: Print specific partition */
    struct ddr_partition part;
    int ret = ddr_partition_get_by_type(PART_TYPE_SYSTEM, 0, &part);
    if (ret == 0) {
        ddr_partition_print(&part);
    }
    
    printf("✅ Partition dumping tests passed\n");
}

/**
 * test_partition_cleanup - Test partition cleanup
 */
void test_partition_cleanup(void)
{
    int ret;
    unsigned int count;
    struct ddr_partition part;
    
    printf("\n=== Testing Partition Cleanup ===\n");
    
    /* Test: Cleanup partitions */
    ddr_partition_cleanup();
    
    /* Test: Count should be 0 */
    count = ddr_partition_get_count();
    TEST_ASSERT_EQUAL(0, count);
    
    /* Test: Get partition should fail */
    ret = ddr_partition_get_by_type(PART_TYPE_SYSTEM, 0, &part);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    
    /* Test: Re-initialize */
    ret = ddr_partition_init();
    TEST_ASSERT_EQUAL(0, ret);
    count = ddr_partition_get_count();
    TEST_ASSERT(count > 0);
    
    printf("✅ Partition cleanup tests passed\n");
}

/**
 * test_partition_edge_cases - Test edge cases
 */
void test_partition_edge_cases(void)
{
    int id;
    int ret;
    struct ddr_partition part;
    unsigned int count;
    
    printf("\n=== Testing Partition Edge Cases ===\n");
    
    count = ddr_partition_get_count();
    
    /* Test: Add partition with maximum size */
    part = create_test_partition("test_max", 0x95000000, 0x7FFFFFFF,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    if (id > 0) {
        ddr_partition_remove(id);
        printf("  Added and removed partition with max size\n");
    } else {
        printf("  Skipped max size test (memory limit)\n");
    }
    
    /* Test: Add partition with minimum size */
    part = create_test_partition("test_min", 0x96000000, 1,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    if (id > 0) {
        ddr_partition_remove(id);
        printf("  Added and removed partition with min size\n");
    } else {
        printf("  Skipped min size test\n");
    }
    
    /* Test: Add partition with very long name */
    char long_name[256];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    part = create_test_partition(long_name, 0x97000000, 0x01000000,
                                 PART_TYPE_USER, 0);
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    ddr_partition_remove(id);
    printf("  Added partition with long name\n");
    
    /* Test: Add partition with invalid flags */
    part = create_test_partition("test_flags", 0x98000000, 0x01000000,
                                 PART_TYPE_USER, 0xFFFFFFFF);
    id = ddr_partition_add(&part);
    TEST_ASSERT(id > 0);
    ddr_partition_remove(id);
    printf("  Added partition with invalid flags\n");
    
    /* Verify count unchanged */
    TEST_ASSERT_EQUAL(count, ddr_partition_get_count());
    
    printf("✅ Partition edge case tests passed\n");
}

/**
 * test_partition_performance - Test partition performance
 */
void test_partition_performance(void)
{
    int id;
    int ret;
    struct ddr_partition part;
    struct timespec start, end;
    double elapsed;
    int iterations = 1000;
    int i;
    
    printf("\n=== Testing Partition Performance ===\n");
    
    /* Test: Partition add performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        snprintf(part.name, sizeof(part.name), "perf_%d", i);
        part.start = 0x99000000 + i * 0x01000000;
        part.size = 0x01000000;
        part.type = PART_TYPE_USER;
        part.flags = 0;
        id = ddr_partition_add(&part);
        TEST_ASSERT(id > 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Partition add: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    /* Test: Partition find performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        char name[32];
        snprintf(name, sizeof(name), "perf_%d", i);
        ret = ddr_partition_find(name, NULL);
        TEST_ASSERT(ret > 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Partition find: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    /* Test: Partition remove performance */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < iterations; i++) {
        ret = ddr_partition_remove(i + 1);
        TEST_ASSERT_EQUAL(0, ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  Partition remove: %d iterations in %.3f seconds (%.3f us/op)\n",
           iterations, elapsed, (elapsed / iterations) * 1e6);
    
    printf("✅ Partition performance tests passed\n");
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
            printf("Partition Management Unit Tests\n\n");
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --verbose        Verbose output\n");
            printf("  --test <name>    Run specific test\n");
            printf("  --help           Show this help\n\n");
            printf("Available tests:\n");
            printf("  test_partition_init\n");
            printf("  test_partition_add\n");
            printf("  test_partition_remove\n");
            printf("  test_partition_get\n");
            printf("  test_partition_find\n");
            printf("  test_partition_by_type\n");
            printf("  test_partition_dump\n");
            printf("  test_partition_cleanup\n");
            printf("  test_partition_edge_cases\n");
            printf("  test_partition_performance\n");
            return 0;
        }
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            Partition Management Unit Tests                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* Initialize Unity */
    UNITY_BEGIN();
    
    /* Run tests */
    if (test_name) {
        /* Run specific test */
        if (strcmp(test_name, "test_partition_init") == 0) {
            RUN_TEST(test_partition_init);
        } else if (strcmp(test_name, "test_partition_add") == 0) {
            RUN_TEST(test_partition_add);
        } else if (strcmp(test_name, "test_partition_remove") == 0) {
            RUN_TEST(test_partition_remove);
        } else if (strcmp(test_name, "test_partition_get") == 0) {
            RUN_TEST(test_partition_get);
        } else if (strcmp(test_name, "test_partition_find") == 0) {
            RUN_TEST(test_partition_find);
        } else if (strcmp(test_name, "test_partition_by_type") == 0) {
            RUN_TEST(test_partition_by_type);
        } else if (strcmp(test_name, "test_partition_dump") == 0) {
            RUN_TEST(test_partition_dump);
        } else if (strcmp(test_name, "test_partition_cleanup") == 0) {
            RUN_TEST(test_partition_cleanup);
        } else if (strcmp(test_name, "test_partition_edge_cases") == 0) {
            RUN_TEST(test_partition_edge_cases);
        } else if (strcmp(test_name, "test_partition_performance") == 0) {
            RUN_TEST(test_partition_performance);
        } else {
            printf("❌ Unknown test: %s\n", test_name);
            return 1;
        }
    } else {
        /* Run all tests */
        RUN_TEST(test_partition_init);
        RUN_TEST(test_partition_add);
        RUN_TEST(test_partition_remove);
        RUN_TEST(test_partition_get);
        RUN_TEST(test_partition_find);
        RUN_TEST(test_partition_by_type);
        RUN_TEST(test_partition_dump);
        RUN_TEST(test_partition_cleanup);
        RUN_TEST(test_partition_edge_cases);
        RUN_TEST(test_partition_performance);
    }
    
    ret = UNITY_END();
    
    /* Cleanup */
    ddr_partition_cleanup();
    
    if (ret == 0) {
        printf("\n✅ All partition management tests passed!\n");
    } else {
        printf("\n❌ Some partition management tests failed!\n");
    }
    
    return ret;
}
