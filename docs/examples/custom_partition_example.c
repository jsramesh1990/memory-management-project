/**
 * custom_partition_example.c - Custom Partition Example
 * 
 * This example demonstrates how to create, manage, and use custom
 * memory partitions on RK3568-based systems. It covers partition
 * creation, resizing, and protection.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o custom_partition_example custom_partition_example.c -lddr_manager
 * 
 * Usage:
 *   ./custom_partition_example [options]
 * 
 * Options:
 *   --create          Create sample partitions
 *   --delete          Delete partitions
 *   --list            List all partitions
 *   --test            Run partition tests
 *   --help            Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_partition.h>
#include <ddr_debug.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define PARTITION_SIZE_KB     (1024)          /* 1 KB */
#define PARTITION_SIZE_MB     (1024 * 1024)   /* 1 MB */
#define PARTITION_MAX         16

/* Colors for terminal output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
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
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("Custom Partition Example\n\n");
    printf("Usage: ./custom_partition_example [options]\n\n");
    printf("Options:\n");
    printf("  --create          Create sample partitions\n");
    printf("  --delete          Delete partitions\n");
    printf("  --list            List all partitions\n");
    printf("  --test            Run partition tests\n");
    printf("  --help            Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./custom_partition_example --create\n");
    printf("  ./custom_partition_example --list\n");
    printf("  ./custom_partition_example --test\n");
}

/**
 * get_partition_type_name - Get partition type name
 * @type: Partition type
 * Return: Name string
 */
static const char *get_partition_type_name(enum partition_type type)
{
    switch (type) {
        case PARTITION_TYPE_SYSTEM:   return "SYSTEM";
        case PARTITION_TYPE_BOOT:     return "BOOT";
        case PARTITION_TYPE_KERNEL:   return "KERNEL";
        case PARTITION_TYPE_NPU:      return "NPU";
        case PARTITION_TYPE_GPU:      return "GPU";
        case PARTITION_TYPE_VPU:      return "VPU";
        case PARTITION_TYPE_DATA:     return "DATA";
        case PARTITION_TYPE_TEMP:     return "TEMP";
        case PARTITION_TYPE_BACKUP:   return "BACKUP";
        case PARTITION_TYPE_RESERVED: return "RESERVED";
        case PARTITION_TYPE_USER:     return "USER";
        case PARTITION_TYPE_CUSTOM:   return "CUSTOM";
        default:                      return "UNKNOWN";
    }
}

/**
 * print_partition_info - Print partition information
 * @info: Partition information structure
 */
static void print_partition_info(const struct partition_info *info)
{
    printf("  ID: %d\n", info->id);
    printf("  Name: %s\n", info->name);
    printf("  Type: %s\n", get_partition_type_name(info->type));
    printf("  Start: 0x%08lx\n", (unsigned long)info->start);
    printf("  Size: %zu KB\n", info->size / 1024);
    printf("  Flags: 0x%04x\n", info->flags);
    printf("  Protected: %s\n", info->protected ? "Yes" : "No");
    printf("  Usage: %d%%\n", info->usage);
}

/* ============================================================================
 * Partition Core Functions
 * ============================================================================ */

/**
 * partition_init_example - Initialize partition subsystem
 * Return: 0 on success, negative error code on failure
 */
static int partition_init_example(void)
{
    int ret;
    
    print_header("Partition Subsystem Initialization");
    
    ret = partition_init();
    if (ret) {
        print_error("Failed to initialize partition subsystem: %d", ret);
        return ret;
    }
    print_success("Partition subsystem initialized");
    
    return 0;
}

/**
 * partition_create_example - Create sample partitions
 * Return: 0 on success, negative error code on failure
 */
static int partition_create_example(void)
{
    int ret;
    int partition_ids[PARTITION_MAX];
    int count = 0;
    
    print_header("Creating Partitions");
    
    /* Create NPU partition */
    print_info("Creating NPU partition...");
    int id = partition_create("npu_memory", 0x20000000, 0x08000000,
                              PARTITION_TYPE_NPU,
                              PARTITION_FLAG_DMA | PARTITION_FLAG_PROTECT);
    if (id < 0) {
        print_error("Failed to create NPU partition: %d", id);
        return id;
    }
    partition_ids[count++] = id;
    print_success("NPU partition created (ID: %d)", id);
    
    /* Create GPU partition */
    print_info("Creating GPU partition...");
    id = partition_create("gpu_memory", 0x28000000, 0x08000000,
                          PARTITION_TYPE_GPU,
                          PARTITION_FLAG_DMA | PARTITION_FLAG_CACHE);
    if (id < 0) {
        print_error("Failed to create GPU partition: %d", id);
        return id;
    }
    partition_ids[count++] = id;
    print_success("GPU partition created (ID: %d)", id);
    
    /* Create VPU partition */
    print_info("Creating VPU partition...");
    id = partition_create("vpu_memory", 0x30000000, 0x10000000,
                          PARTITION_TYPE_VPU,
                          PARTITION_FLAG_DMA);
    if (id < 0) {
        print_error("Failed to create VPU partition: %d", id);
        return id;
    }
    partition_ids[count++] = id;
    print_success("VPU partition created (ID: %d)", id);
    
    /* Create data partition */
    print_info("Creating data partition...");
    id = partition_create("user_data", 0x40000000, 0x10000000,
                          PARTITION_TYPE_DATA,
                          PARTITION_FLAG_NONE);
    if (id < 0) {
        print_error("Failed to create data partition: %d", id);
        return id;
    }
    partition_ids[count++] = id;
    print_success("Data partition created (ID: %d)", id);
    
    /* Create backup partition */
    print_info("Creating backup partition...");
    id = partition_create("backup", 0x50000000, 0x08000000,
                          PARTITION_TYPE_BACKUP,
                          PARTITION_FLAG_PROTECT | PARTITION_FLAG_BACKUP);
    if (id < 0) {
        print_error("Failed to create backup partition: %d", id);
        return id;
    }
    partition_ids[count++] = id;
    print_success("Backup partition created (ID: %d)", id);
    
    printf("\n  Created %d partitions:\n", count);
    for (int i = 0; i < count; i++) {
        struct partition_info info;
        ret = partition_info(partition_ids[i], &info);
        if (!ret) {
            printf("  - %s (ID: %d, Size: %zu MB)\n",
                   info.name, info.id, info.size / (1024 * 1024));
        }
    }
    
    return 0;
}

/**
 * partition_list_example - List all partitions
 * Return: 0 on success, negative error code on failure
 */
static int partition_list_example(void)
{
    int ret;
    struct partition_info partitions[PARTITION_MAX];
    int count;
    
    print_header("Listing Partitions");
    
    count = partition_list(partitions, PARTITION_MAX);
    if (count < 0) {
        print_error("Failed to list partitions: %d", count);
        return count;
    }
    
    if (count == 0) {
        print_info("No partitions found");
        return 0;
    }
    
    printf("\n  Found %d partitions:\n\n", count);
    
    printf("  %-4s %-20s %-12s %-12s %-8s %-8s\n",
           "ID", "Name", "Type", "Size", "Protected", "Usage");
    printf("  %s\n", "-----------------------------------------------------");
    
    for (int i = 0; i < count; i++) {
        printf("  %-4d %-20s %-12s %-12s %-8s %-8d%%\n",
               partitions[i].id,
               partitions[i].name,
               get_partition_type_name(partitions[i].type),
               partitions[i].size < (1024 * 1024) ?
                   "1 KB" : 
                   (partitions[i].size < (1024 * 1024 * 1024)) ?
                   "1 MB" : "1 GB",
               partitions[i].protected ? "Yes" : "No",
               partitions[i].usage);
    }
    
    return 0;
}

/**
 * partition_delete_example - Delete partitions
 * Return: 0 on success, negative error code on failure
 */
static int partition_delete_example(void)
{
    int ret;
    struct partition_info partitions[PARTITION_MAX];
    int count;
    int deleted = 0;
    
    print_header("Deleting Partitions");
    
    /* Get list of partitions */
    count = partition_list(partitions, PARTITION_MAX);
    if (count < 0) {
        print_error("Failed to list partitions: %d", count);
        return count;
    }
    
    if (count == 0) {
        print_info("No partitions to delete");
        return 0;
    }
    
    /* Delete non-protected partitions */
    for (int i = 0; i < count; i++) {
        if (!partitions[i].protected) {
            print_info("Deleting partition: %s (ID: %d)",
                       partitions[i].name, partitions[i].id);
            
            ret = partition_delete(partitions[i].id);
            if (ret) {
                print_error("Failed to delete partition %d: %d", 
                            partitions[i].id, ret);
            } else {
                deleted++;
                print_success("Partition deleted: %s", partitions[i].name);
            }
        } else {
            print_info("Skipping protected partition: %s", partitions[i].name);
        }
    }
    
    print_success("Deleted %d partitions", deleted);
    return 0;
}

/**
 * partition_test - Run partition tests
 * Return: 0 on success, negative error code on failure
 */
static int partition_test(void)
{
    int ret;
    int test_id;
    struct partition_info info;
    void *test_data;
    size_t test_size = PARTITION_SIZE_MB;  /* 1 MB */
    
    print_header("Partition Tests");
    
    /* Create test partition */
    print_info("Creating test partition...");
    test_id = partition_create("test_partition", 0x60000000, test_size,
                               PARTITION_TYPE_CUSTOM,
                               PARTITION_FLAG_NONE);
    if (test_id < 0) {
        print_error("Failed to create test partition: %d", test_id);
        return test_id;
    }
    print_success("Test partition created (ID: %d)", test_id);
    
    /* Get partition info */
    ret = partition_info(test_id, &info);
    if (ret) {
        print_error("Failed to get partition info: %d", ret);
        partition_delete(test_id);
        return ret;
    }
    
    printf("\n  Test Partition Info:\n");
    print_partition_info(&info);
    
    /* Write test data */
    print_info("Writing test data to partition...");
    test_data = malloc(test_size);
    if (!test_data) {
        print_error("Failed to allocate test data");
        partition_delete(test_id);
        return -ENOMEM;
    }
    
    /* Fill with pattern */
    memset(test_data, 0xAA, test_size);
    *(uint32_t *)test_data = 0xDEADBEEF;
    
    /* Write to partition (simulated) */
    ret = partition_write(test_id, 0, test_data, test_size);
    if (ret) {
        print_error("Failed to write test data: %d", ret);
        free(test_data);
        partition_delete(test_id);
        return ret;
    }
    print_success("Test data written: %zu bytes", test_size);
    
    /* Read back test data */
    print_info("Reading test data from partition...");
    ret = partition_read(test_id, 0, test_data, test_size);
    if (ret) {
        print_error("Failed to read test data: %d", ret);
        free(test_data);
        partition_delete(test_id);
        return ret;
    }
    print_success("Test data read: %zu bytes", test_size);
    
    /* Verify data */
    if (*(uint32_t *)test_data == 0xDEADBEEF) {
        print_success("Data verification passed!");
    } else {
        print_error("Data verification failed!");
        free(test_data);
        partition_delete(test_id);
        return -EIO;
    }
    
    /* Test resize */
    print_info("Testing partition resize...");
    ret = partition_resize(test_id, test_size * 2);
    if (ret) {
        print_error("Failed to resize partition: %d", ret);
        free(test_data);
        partition_delete(test_id);
        return ret;
    }
    print_success("Partition resized: %zu MB -> %zu MB",
                  test_size / (1024 * 1024),
                  (test_size * 2) / (1024 * 1024));
    
    /* Test protect */
    print_info("Testing partition protection...");
    ret = partition_protect(test_id);
    if (ret) {
        print_error("Failed to protect partition: %d", ret);
    } else {
        print_success("Partition protected");
        
        /* Try to delete protected partition (should fail) */
        ret = partition_delete(test_id);
        if (ret == -EPERM) {
            print_success("Protection works (delete prevented)");
        } else {
            print_error("Protection test failed: %d", ret);
        }
        
        /* Unprotect */
        ret = partition_unprotect(test_id);
        if (ret) {
            print_error("Failed to unprotect partition: %d", ret);
        } else {
            print_success("Partition unprotected");
        }
    }
    
    /* Clean up */
    free(test_data);
    ret = partition_delete(test_id);
    if (ret) {
        print_error("Failed to delete test partition: %d", ret);
        return ret;
    }
    print_success("Test partition deleted");
    
    return 0;
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int ret;
    int run_create = 0;
    int run_delete = 0;
    int run_list = 0;
    int run_test = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     RK3568 Custom Partition Example                 ║\n");
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--create") == 0) {
            run_create = 1;
        } else if (strcmp(argv[i], "--delete") == 0) {
            run_delete = 1;
        } else if (strcmp(argv[i], "--list") == 0) {
            run_list = 1;
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
    
    /* Initialize partition subsystem */
    ret = partition_init_example();
    if (ret) {
        print_error("Partition initialization failed: %d", ret);
        return 1;
    }
    
    /* Run requested operations */
    if (run_create) {
        ret = partition_create_example();
        if (ret) {
            print_error("Partition creation failed: %d", ret);
            return 1;
        }
    }
    
    if (run_list) {
        ret = partition_list_example();
        if (ret) {
            print_error("Partition listing failed: %d", ret);
            return 1;
        }
    }
    
    if (run_test) {
        ret = partition_test();
        if (ret) {
            print_error("Partition test failed: %d", ret);
            return 1;
        }
    }
    
    if (run_delete) {
        ret = partition_delete_example();
        if (ret) {
            print_error("Partition deletion failed: %d", ret);
            return 1;
        }
    }
    
    /* If no operation specified, show help */
    if (!run_create && !run_delete && !run_list && !run_test) {
        show_help();
        return 0;
    }
    
    printf("\n");
    print_header("Example Completed Successfully");
    printf("\n");
    
    return 0;
}
