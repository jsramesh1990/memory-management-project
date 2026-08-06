/**
 * ddr_info_tool.c - DDR Information Tool
 * 
 * This tool provides comprehensive DDR memory information and
 * diagnostics for RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o ddr_info_tool ddr_info_tool.c -lddr_manager -lm
 * 
 * Usage:
 *   ./ddr_info_tool [options]
 *   ./ddr_info_tool --info
 *   ./ddr_info_tool --memory
 *   ./ddr_info_tool --config
 *   ./ddr_info_tool --stats
 *   ./ddr_info_tool --test
 *   ./ddr_info_tool --watch
 *   ./ddr_info_tool --json
 *   ./ddr_info_tool --help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <getopt.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include <ddr_manager.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

#define WATCH_INTERVAL_DEFAULT  1.0
#define MAX_HISTORY             100

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static int g_running = 1;
static struct ddr_info g_last_info;
static struct ddr_stats g_last_stats;
static struct ddr_config g_last_config;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * print_header - Print section header
 */
static void print_header(const char *title)
{
    printf("\n%s%s%s\n", COLOR_BLUE, "========================================", COLOR_RESET);
    printf("%s%s%s\n", COLOR_BLUE, "  ", title, COLOR_RESET);
    printf("%s%s%s\n", COLOR_BLUE, "========================================", COLOR_RESET);
}

/**
 * print_info - Print info message
 */
static void print_info(const char *format, ...)
{
    va_list args;
    printf("%s[INFO]%s ", COLOR_CYAN, COLOR_RESET);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/**
 * print_success - Print success message
 */
static void print_success(const char *format, ...)
{
    va_list args;
    printf("%s[SUCCESS]%s ", COLOR_GREEN, COLOR_RESET);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/**
 * print_error - Print error message
 */
static void print_error(const char *format, ...)
{
    va_list args;
    printf("%s[ERROR]%s ", COLOR_RED, COLOR_RESET);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/**
 * print_warning - Print warning message
 */
static void print_warning(const char *format, ...)
{
    va_list args;
    printf("%s[WARNING]%s ", COLOR_YELLOW, COLOR_RESET);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/**
 * print_value - Print value with label
 */
static void print_value(const char *label, const char *value, const char *color)
{
    printf("  %s%-20s%s %s%s%s\n", 
           COLOR_WHITE, label, COLOR_RESET,
           color ? color : "", value ? value : "N/A", COLOR_RESET);
}

/**
 * print_bar - Print progress bar
 */
static void print_bar(int percent, int width, const char *label)
{
    int filled = (percent * width) / 100;
    int empty = width - filled;
    
    printf("  %s%-20s%s [", COLOR_WHITE, label, COLOR_RESET);
    
    for (int i = 0; i < filled; i++) {
        printf("%s█%s", COLOR_GREEN, COLOR_RESET);
    }
    for (int i = 0; i < empty; i++) {
        printf(".");
    }
    
    printf("] %3d%%\n", percent);
}

/**
 * signal_handler - Handle signals
 */
static void signal_handler(int sig)
{
    g_running = 0;
}

/**
 * get_timestamp - Get current timestamp string
 */
static void get_timestamp(char *buffer, size_t size)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
}

/**
 * format_size - Format size in human-readable format
 */
static void format_size(size_t bytes, char *buffer, size_t size)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double value = bytes;
    
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        unit++;
    }
    
    snprintf(buffer, size, "%.2f %s", value, units[unit]);
}

/* ============================================================================
 * Display Functions
 * ============================================================================ */

/**
 * display_info - Display DDR information
 */
static void display_info(void)
{
    struct ddr_info info;
    char buffer[32];
    int ret;
    
    ret = ddr_get_info(&info);
    if (ret < 0) {
        print_error("Failed to get DDR info: %s", ddr_get_error_string(ret));
        return;
    }
    
    memcpy(&g_last_info, &info, sizeof(info));
    
    print_header("DDR Information");
    
    format_size(info.total_memory * 1024 * 1024, buffer, sizeof(buffer));
    print_value("Total Memory", buffer, COLOR_WHITE);
    
    format_size(info.used_memory * 1024 * 1024, buffer, sizeof(buffer));
    print_value("Used Memory", buffer, COLOR_YELLOW);
    
    format_size(info.free_memory * 1024 * 1024, buffer, sizeof(buffer));
    print_value("Free Memory", buffer, COLOR_GREEN);
    
    print_bar((info.used_memory * 100) / info.total_memory, 40, "Usage");
    
    snprintf(buffer, sizeof(buffer), "%u MHz", info.frequency);
    print_value("Frequency", buffer, COLOR_CYAN);
    
    snprintf(buffer, sizeof(buffer), "%u mV", info.voltage);
    print_value("Voltage", buffer, COLOR_CYAN);
    
    snprintf(buffer, sizeof(buffer), "%u C", info.temperature);
    print_value("Temperature", buffer, info.temperature > 70 ? COLOR_RED : COLOR_GREEN);
    
    snprintf(buffer, sizeof(buffer), "0x%04x", info.status);
    print_value("Status", buffer, COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%u", info.errors);
    print_value("Errors", buffer, info.errors > 0 ? COLOR_RED : COLOR_GREEN);
}

/**
 * display_stats - Display DDR statistics
 */
static void display_stats(void)
{
    struct ddr_stats stats;
    char buffer[32];
    int ret;
    
    ret = ddr_get_stats(&stats);
    if (ret < 0) {
        print_error("Failed to get DDR stats: %s", ddr_get_error_string(ret));
        return;
    }
    
    memcpy(&g_last_stats, &stats, sizeof(stats));
    
    print_header("DDR Statistics");
    
    snprintf(buffer, sizeof(buffer), "%lu", stats.total_allocations);
    print_value("Total Allocations", buffer, COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%lu", stats.total_frees);
    print_value("Total Frees", buffer, COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%lu", stats.current_allocations);
    print_value("Current Allocations", buffer, COLOR_CYAN);
    
    snprintf(buffer, sizeof(buffer), "%lu", stats.peak_allocations);
    print_value("Peak Allocations", buffer, COLOR_YELLOW);
    
    format_size(stats.total_allocated, buffer, sizeof(buffer));
    print_value("Total Allocated", buffer, COLOR_WHITE);
    
    format_size(stats.total_freed, buffer, sizeof(buffer));
    print_value("Total Freed", buffer, COLOR_WHITE);
    
    format_size(stats.current_allocated, buffer, sizeof(buffer));
    print_value("Current Allocated", buffer, COLOR_CYAN);
    
    format_size(stats.peak_allocated, buffer, sizeof(buffer));
    print_value("Peak Allocated", buffer, COLOR_YELLOW);
    
    snprintf(buffer, sizeof(buffer), "%u", stats.errors);
    print_value("Errors", buffer, stats.errors > 0 ? COLOR_RED : COLOR_GREEN);
    
    snprintf(buffer, sizeof(buffer), "%u", stats.warnings);
    print_value("Warnings", buffer, stats.warnings > 0 ? COLOR_YELLOW : COLOR_GREEN);
    
    snprintf(buffer, sizeof(buffer), "%u%%", stats.fragmentation);
    print_value("Fragmentation", buffer, stats.fragmentation > 50 ? COLOR_RED : COLOR_GREEN);
}

/**
 * display_config - Display DDR configuration
 */
static void display_config(void)
{
    struct ddr_config config;
    char buffer[32];
    int ret;
    
    ret = ddr_get_config(&config);
    if (ret < 0) {
        print_error("Failed to get DDR config: %s", ddr_get_error_string(ret));
        return;
    }
    
    memcpy(&g_last_config, &config, sizeof(config));
    
    print_header("DDR Configuration");
    
    print_value("Name", config.name, COLOR_WHITE);
    print_value("Type", ddr_get_type_string(config.type), COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%u MB", config.size_mb);
    print_value("Size", buffer, COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%u", config.channels);
    print_value("Channels", buffer, COLOR_WHITE);
    
    snprintf(buffer, sizeof(buffer), "%u MHz", config.frequency_mhz);
    print_value("Frequency", buffer, COLOR_CYAN);
    
    snprintf(buffer, sizeof(buffer), "%u mV", config.voltage_mv);
    print_value("Voltage", buffer, COLOR_CYAN);
    
    snprintf(buffer, sizeof(buffer), "CL%u-tRCD%u-tRP%u-tRAS%u",
             config.timings.tCL, config.timings.tRCD,
             config.timings.tRP, config.timings.tRAS);
    print_value("Timings", buffer, COLOR_WHITE);
    
    print_value("ECC", config.ecc_enabled ? "Enabled" : "Disabled",
                config.ecc_enabled ? COLOR_GREEN : COLOR_YELLOW);
    
    print_value("Power Save", config.power_save ? "Enabled" : "Disabled",
                config.power_save ? COLOR_YELLOW : COLOR_GREEN);
    
    print_value("Performance Mode", config.performance_mode ? "Enabled" : "Disabled",
                config.performance_mode ? COLOR_GREEN : COLOR_YELLOW);
}

/**
 * display_memory_map - Display memory map
 */
static void display_memory_map(void)
{
    struct ddr_info info;
    int ret;
    
    ret = ddr_get_info(&info);
    if (ret < 0) {
        print_error("Failed to get DDR info: %s", ddr_get_error_string(ret));
        return;
    }
    
    print_header("Memory Map");
    
    printf("\n  %s0x00000000 - 0x0FFFFFFF%s  Bootloader/Reserved\n",
           COLOR_CYAN, COLOR_RESET);
    printf("  %s0x10000000 - 0x1FFFFFFF%s  Device Tree/Reserved\n",
           COLOR_CYAN, COLOR_RESET);
    printf("  %s0x20000000 - 0x2FFFFFFF%s  NPU/GPU Memory\n",
           COLOR_CYAN, COLOR_RESET);
    printf("  %s0x30000000 - 0x3FFFFFFF%s  VPU/Camera Buffers\n",
           COLOR_CYAN, COLOR_RESET);
    printf("  %s0x40000000 - 0x%08lX%s  System RAM (%u MB)\n",
           COLOR_GREEN, COLOR_RESET,
           (unsigned long)(0x40000000 + info.total_memory * 1024 * 1024),
           info.total_memory);
}

/**
 * display_test - Run memory test
 */
static void display_test(void)
{
    void *ptr;
    size_t size = 1024 * 1024;  /* 1 MB */
    int ret;
    
    print_header("Memory Test");
    
    print_info("Allocating %zu bytes...", size);
    ptr = ddr_alloc(size, DDR_ALLOC_FLAG_ZERO_INIT);
    if (!ptr) {
        print_error("Allocation failed");
        return;
    }
    print_success("Allocation successful at %p", ptr);
    
    print_info("Testing memory...");
    ret = ddr_memtest(ptr, size, 3);
    if (ret < 0) {
        print_error("Memory test failed");
    } else {
        print_success("Memory test passed");
    }
    
    print_info("Freeing memory...");
    ddr_free(ptr);
    print_success("Memory freed");
}

/**
 * display_watch - Watch memory usage
 */
static void display_watch(double interval)
{
    struct ddr_info info;
    char timestamp[32];
    int ret;
    
    print_header("Memory Watch (Press Ctrl+C to stop)");
    printf("\n");
    printf("  %-20s %-12s %-12s %-12s %-10s\n",
           "Timestamp", "Total (MB)", "Used (MB)", "Free (MB)", "Usage %");
    printf("  %s\n", "------------------------------------------------------------");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    while (g_running) {
        ret = ddr_get_info(&info);
        if (ret < 0) {
            print_error("Failed to get DDR info");
            break;
        }
        
        get_timestamp(timestamp, sizeof(timestamp));
        
        printf("\r  %-20s %-12u %-12u %-12u %-9.1f",
               timestamp,
               info.total_memory,
               info.used_memory,
               info.free_memory,
               (float)info.used_memory / info.total_memory * 100.0);
        fflush(stdout);
        
        usleep((useconds_t)(interval * 1000000));
    }
    
    printf("\n");
    print_success("Watch stopped");
}

/**
 * display_json - Display JSON output
 */
static void display_json(void)
{
    struct ddr_info info;
    struct ddr_stats stats;
    struct ddr_config config;
    int ret;
    
    ret = ddr_get_info(&info);
    if (ret < 0) {
        print_error("Failed to get DDR info");
        return;
    }
    
    ret = ddr_get_stats(&stats);
    if (ret < 0) {
        print_error("Failed to get DDR stats");
        return;
    }
    
    ret = ddr_get_config(&config);
    if (ret < 0) {
        print_error("Failed to get DDR config");
        return;
    }
    
    printf("{\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"info\": {\n");
    printf("    \"total_memory\": %u,\n", info.total_memory);
    printf("    \"used_memory\": %u,\n", info.used_memory);
    printf("    \"free_memory\": %u,\n", info.free_memory);
    printf("    \"frequency\": %u,\n", info.frequency);
    printf("    \"voltage\": %u,\n", info.voltage);
    printf("    \"temperature\": %u,\n", info.temperature);
    printf("    \"status\": %u,\n", info.status);
    printf("    \"errors\": %u\n", info.errors);
    printf("  },\n");
    printf("  \"stats\": {\n");
    printf("    \"total_allocations\": %lu,\n", stats.total_allocations);
    printf("    \"total_frees\": %lu,\n", stats.total_frees);
    printf("    \"current_allocations\": %lu,\n", stats.current_allocations);
    printf("    \"peak_allocations\": %lu,\n", stats.peak_allocations);
    printf("    \"total_allocated\": %lu,\n", stats.total_allocated);
    printf("    \"total_freed\": %lu,\n", stats.total_freed);
    printf("    \"current_allocated\": %lu,\n", stats.current_allocated);
    printf("    \"peak_allocated\": %lu,\n", stats.peak_allocated);
    printf("    \"errors\": %u,\n", stats.errors);
    printf("    \"warnings\": %u,\n", stats.warnings);
    printf("    \"fragmentation\": %u\n", stats.fragmentation);
    printf("  },\n");
    printf("  \"config\": {\n");
    printf("    \"name\": \"%s\",\n", config.name);
    printf("    \"type\": %d,\n", config.type);
    printf("    \"size_mb\": %u,\n", config.size_mb);
    printf("    \"channels\": %u,\n", config.channels);
    printf("    \"frequency_mhz\": %u,\n", config.frequency_mhz);
    printf("    \"voltage_mv\": %u,\n", config.voltage_mv);
    printf("    \"ecc_enabled\": %d,\n", config.ecc_enabled);
    printf("    \"power_save\": %d,\n", config.power_save);
    printf("    \"performance_mode\": %d\n", config.performance_mode);
    printf("  }\n");
    printf("}\n");
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int opt;
    int show_info = 0;
    int show_stats = 0;
    int show_config = 0;
    int show_memory = 0;
    int run_test = 0;
    int watch = 0;
    int json = 0;
    double watch_interval = WATCH_INTERVAL_DEFAULT;
    int ret;
    
    static struct option long_options[] = {
        {"info", no_argument, 0, 'i'},
        {"stats", no_argument, 0, 's'},
        {"config", no_argument, 0, 'c'},
        {"memory", no_argument, 0, 'm'},
        {"test", no_argument, 0, 't'},
        {"watch", optional_argument, 0, 'w'},
        {"json", no_argument, 0, 'j'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    printf("\n");
    printf("%s╔══════════════════════════════════════════════════════╗%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s║     DDR Information Tool                           ║%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s╚══════════════════════════════════════════════════════╝%s\n",
           COLOR_BLUE, COLOR_RESET);
    
    /* Parse arguments */
    while ((opt = getopt_long(argc, argv, "iscmtw::jh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                show_info = 1;
                break;
            case 's':
                show_stats = 1;
                break;
            case 'c':
                show_config = 1;
                break;
            case 'm':
                show_memory = 1;
                break;
            case 't':
                run_test = 1;
                break;
            case 'w':
                watch = 1;
                if (optarg) {
                    watch_interval = atof(optarg);
                    if (watch_interval <= 0) {
                        watch_interval = WATCH_INTERVAL_DEFAULT;
                    }
                }
                break;
            case 'j':
                json = 1;
                break;
            case 'h':
                printf("\nDDR Information Tool\n\n");
                printf("Usage: %s [options]\n\n", argv[0]);
                printf("Options:\n");
                printf("  -i, --info      Show DDR information\n");
                printf("  -s, --stats     Show DDR statistics\n");
                printf("  -c, --config    Show DDR configuration\n");
                printf("  -m, --memory    Show memory map\n");
                printf("  -t, --test      Run memory test\n");
                printf("  -w, --watch     Watch memory usage (interval in seconds)\n");
                printf("  -j, --json      Output in JSON format\n");
                printf("  -h, --help      Show this help\n\n");
                printf("Examples:\n");
                printf("  %s --info\n", argv[0]);
                printf("  %s --stats --config\n", argv[0]);
                printf("  %s --watch 2\n", argv[0]);
                printf("  %s --json\n", argv[0]);
                return 0;
            default:
                print_error("Unknown option: %c", opt);
                return 1;
        }
    }
    
    /* Initialize DDR Manager */
    ret = ddr_init();
    if (ret < 0) {
        print_error("Failed to initialize DDR Manager: %s", ddr_get_error_string(ret));
        return 1;
    }
    
    /* If no options, show default info */
    if (!show_info && !show_stats && !show_config && !show_memory &&
        !run_test && !watch && !json) {
        show_info = 1;
    }
    
    /* Execute commands */
    if (json) {
        display_json();
    } else {
        if (show_info) {
            display_info();
        }
        if (show_stats) {
            display_stats();
        }
        if (show_config) {
            display_config();
        }
        if (show_memory) {
            display_memory_map();
        }
        if (run_test) {
            display_test();
        }
        if (watch) {
            display_watch(watch_interval);
        }
    }
    
    /* Cleanup */
    ddr_cleanup();
    
    print_success("Tool completed successfully");
    
    return 0;
}
