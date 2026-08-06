/**
 * memory_benchmark.c - DDR Memory Benchmark Tool
 * 
 * This tool performs comprehensive memory benchmarking including
 * bandwidth, latency, and throughput tests.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o memory_benchmark memory_benchmark.c -lddr_manager -lm -lpthread
 * 
 * Usage:
 *   ./memory_benchmark [options]
 *   ./memory_benchmark --bandwidth
 *   ./memory_benchmark --latency
 *   ./memory_benchmark --throughput
 *   ./memory_benchmark --all
 *   ./memory_benchmark --size 1M --iterations 100
 *   ./memory_benchmark --help
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
#include <pthread.h>
#include <sys/time.h>

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

#define DEFAULT_SIZE        (1024 * 1024)      /* 1 MB */
#define DEFAULT_ITERATIONS  100
#define DEFAULT_WARMUP      10
#define DEFAULT_THREADS     1
#define MAX_THREADS         16

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
    size_t size;
    int iterations;
    int warmup;
    int threads;
    int bandwidth;
    int latency;
    int throughput;
    int all;
    int verbose;
    char *pattern;
} benchmark_config_t;

typedef struct {
    double bandwidth_read;
    double bandwidth_write;
    double bandwidth_copy;
    double latency_read;
    double latency_write;
    double throughput_malloc;
    double throughput_free;
    double throughput_memcpy;
    double throughput_memset;
} benchmark_result_t;

typedef struct {
    void *buffer;
    size_t size;
    int id;
    benchmark_result_t result;
} thread_data_t;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static benchmark_config_t g_config = {
    .size = DEFAULT_SIZE,
    .iterations = DEFAULT_ITERATIONS,
    .warmup = DEFAULT_WARMUP,
    .threads = DEFAULT_THREADS,
    .bandwidth = 0,
    .latency = 0,
    .throughput = 0,
    .all = 0,
    .verbose = 0,
    .pattern = "sequential",
};

static pthread_mutex_t g_result_mutex = PTHREAD_MUTEX_INITIALIZER;
static benchmark_result_t g_combined_result;

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
 * get_time_ns - Get time in nanoseconds
 */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/**
 * format_size - Format size
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

/**
 * format_speed - Format speed
 */
static void format_speed(double bytes_per_sec, char *buffer, size_t size)
{
    const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    int unit = 0;
    double value = bytes_per_sec;
    
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        unit++;
    }
    
    snprintf(buffer, size, "%.2f %s", value, units[unit]);
}

/* ============================================================================
 * Benchmark Functions
 * ============================================================================ */

/**
 * benchmark_bandwidth_read - Benchmark read bandwidth
 */
static double benchmark_bandwidth_read(void *buffer, size_t size, int iterations)
{
    uint64_t start, end;
    volatile uint64_t sum = 0;
    volatile uint8_t *buf = (volatile uint8_t *)buffer;
    size_t i, j;
    
    /* Warmup */
    for (j = 0; j < 10; j++) {
        for (i = 0; i < size; i++) {
            sum += buf[i];
        }
    }
    
    /* Measure */
    start = get_time_ns();
    for (j = 0; j < iterations; j++) {
        for (i = 0; i < size; i++) {
            sum += buf[i];
        }
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    double bytes = (double)size * iterations;
    return bytes / elapsed;
}

/**
 * benchmark_bandwidth_write - Benchmark write bandwidth
 */
static double benchmark_bandwidth_write(void *buffer, size_t size, int iterations)
{
    uint64_t start, end;
    volatile uint8_t *buf = (volatile uint8_t *)buffer;
    size_t i, j;
    
    /* Warmup */
    for (j = 0; j < 10; j++) {
        for (i = 0; i < size; i++) {
            buf[i] = (i + j) & 0xFF;
        }
    }
    
    /* Measure */
    start = get_time_ns();
    for (j = 0; j < iterations; j++) {
        for (i = 0; i < size; i++) {
            buf[i] = (i + j + 1) & 0xFF;
        }
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    double bytes = (double)size * iterations;
    return bytes / elapsed;
}

/**
 * benchmark_bandwidth_copy - Benchmark copy bandwidth
 */
static double benchmark_bandwidth_copy(void *dst, void *src, size_t size, int iterations)
{
    uint64_t start, end;
    size_t i, j;
    
    /* Warmup */
    for (j = 0; j < 10; j++) {
        memcpy(dst, src, size);
    }
    
    /* Measure */
    start = get_time_ns();
    for (j = 0; j < iterations; j++) {
        memcpy(dst, src, size);
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    double bytes = (double)size * iterations;
    return bytes / elapsed;
}

/**
 * benchmark_latency_read - Benchmark read latency
 */
static double benchmark_latency_read(void *buffer, size_t size, int iterations)
{
    uint64_t start, end;
    volatile uint64_t sum = 0;
    volatile uint8_t *buf = (volatile uint8_t *)buffer;
    size_t i;
    
    start = get_time_ns();
    for (i = 0; i < iterations; i++) {
        size_t offset = (i * 64) % size;
        sum += buf[offset];
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    return elapsed / iterations;
}

/**
 * benchmark_latency_write - Benchmark write latency
 */
static double benchmark_latency_write(void *buffer, size_t size, int iterations)
{
    uint64_t start, end;
    volatile uint8_t *buf = (volatile uint8_t *)buffer;
    size_t i;
    
    start = get_time_ns();
    for (i = 0; i < iterations; i++) {
        size_t offset = (i * 64) % size;
        buf[offset] = i & 0xFF;
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    return elapsed / iterations;
}

/**
 * benchmark_throughput_malloc - Benchmark malloc throughput
 */
static double benchmark_throughput_malloc(size_t size, int iterations)
{
    uint64_t start, end;
    void *ptr;
    size_t i;
    
    start = get_time_ns();
    for (i = 0; i < iterations; i++) {
        ptr = ddr_alloc(size, 0);
        if (ptr) {
            ddr_free(ptr);
        }
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    return iterations / elapsed;
}

/**
 * benchmark_throughput_free - Benchmark free throughput
 */
static double benchmark_throughput_free(size_t size, int iterations)
{
    uint64_t start, end;
    void *ptrs[1000];
    size_t i;
    
    /* Pre-allocate */
    for (i = 0; i < iterations && i < 1000; i++) {
        ptrs[i] = ddr_alloc(size, 0);
        if (!ptrs[i]) {
            break;
        }
    }
    
    start = get_time_ns();
    for (i = 0; i < iterations && i < 1000; i++) {
        ddr_free(ptrs[i]);
    }
    end = get_time_ns();
    
    double elapsed = (double)(end - start) / 1e9;
    return iterations / elapsed;
}

/**
 * benchmark_throughput_memcpy - Benchmark memcpy throughput
 */
static double benchmark_throughput_memcpy(size_t size, int iterations)
{
    void *src, *dst;
    uint64_t start, end;
    size_t i;
    
    src = ddr_alloc(size, 0);
    dst = ddr_alloc(size, 0);
    if (!src || !dst) {
        if (src) ddr_free(src);
        if (dst) ddr_free(dst);
        return 0;
    }
    
    memset(src, 0xAA, size);
    
    start = get_time_ns();
    for (i = 0; i < iterations; i++) {
        memcpy(dst, src, size);
    }
    end = get_time_ns();
    
    ddr_free(src);
    ddr_free(dst);
    
    double elapsed = (double)(end - start) / 1e9;
    return (double)size * iterations / elapsed;
}

/**
 * benchmark_throughput_memset - Benchmark memset throughput
 */
static double benchmark_throughput_memset(size_t size, int iterations)
{
    void *buf;
    uint64_t start, end;
    size_t i;
    
    buf = ddr_alloc(size, 0);
    if (!buf) {
        return 0;
    }
    
    start = get_time_ns();
    for (i = 0; i < iterations; i++) {
        memset(buf, i & 0xFF, size);
    }
    end = get_time_ns();
    
    ddr_free(buf);
    
    double elapsed = (double)(end - start) / 1e9;
    return (double)size * iterations / elapsed;
}

/* ============================================================================
 * Thread Functions
 * ============================================================================ */

/**
 * thread_benchmark - Thread benchmark function
 */
static void *thread_benchmark(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    benchmark_result_t *result = &data->result;
    char size_str[32];
    
    format_size(data->size, size_str, sizeof(size_str));
    
    print_info("Thread %d: Starting benchmark (%s)", data->id, size_str);
    
    /* Bandwidth tests */
    if (g_config.bandwidth || g_config.all) {
        result->bandwidth_read = benchmark_bandwidth_read(
            data->buffer, data->size, g_config.iterations);
        result->bandwidth_write = benchmark_bandwidth_write(
            data->buffer, data->size, g_config.iterations);
        result->bandwidth_copy = benchmark_bandwidth_copy(
            data->buffer, data->buffer + data->size / 2,
            data->size / 2, g_config.iterations);
    }
    
    /* Latency tests */
    if (g_config.latency || g_config.all) {
        result->latency_read = benchmark_latency_read(
            data->buffer, data->size, g_config.iterations * 10);
        result->latency_write = benchmark_latency_write(
            data->buffer, data->size, g_config.iterations * 10);
    }
    
    /* Throughput tests */
    if (g_config.throughput || g_config.all) {
        result->throughput_malloc = benchmark_throughput_malloc(
            data->size / 10, g_config.iterations / 10);
        result->throughput_free = benchmark_throughput_free(
            data->size / 10, g_config.iterations / 10);
        result->throughput_memcpy = benchmark_throughput_memcpy(
            data->size / 10, g_config.iterations / 10);
        result->throughput_memset = benchmark_throughput_memset(
            data->size / 10, g_config.iterations / 10);
    }
    
    /* Combine results */
    pthread_mutex_lock(&g_result_mutex);
    g_combined_result.bandwidth_read += result->bandwidth_read;
    g_combined_result.bandwidth_write += result->bandwidth_write;
    g_combined_result.bandwidth_copy += result->bandwidth_copy;
    g_combined_result.latency_read += result->latency_read;
    g_combined_result.latency_write += result->latency_write;
    g_combined_result.throughput_malloc += result->throughput_malloc;
    g_combined_result.throughput_free += result->throughput_free;
    g_combined_result.throughput_memcpy += result->throughput_memcpy;
    g_combined_result.throughput_memset += result->throughput_memset;
    pthread_mutex_unlock(&g_result_mutex);
    
    print_info("Thread %d: Benchmark complete", data->id);
    
    return NULL;
}

/**
 * run_benchmark - Run benchmark with multiple threads
 */
static void run_benchmark(void)
{
    pthread_t threads[MAX_THREADS];
    thread_data_t thread_data[MAX_THREADS];
    void *buffers[MAX_THREADS];
    char size_str[32];
    int ret;
    int i;
    
    print_header("DDR Memory Benchmark");
    
    format_size(g_config.size, size_str, sizeof(size_str));
    print_info("Size: %s", size_str);
    print_info("Iterations: %d", g_config.iterations);
    print_info("Warmup: %d", g_config.warmup);
    print_info("Threads: %d", g_config.threads);
    print_info("Pattern: %s", g_config.pattern);
    
    /* Allocate buffers */
    print_info("Allocating buffers...");
    for (i = 0; i < g_config.threads; i++) {
        buffers[i] = ddr_alloc(g_config.size * 2, DDR_ALLOC_FLAG_ZERO_INIT);
        if (!buffers[i]) {
            print_error("Failed to allocate buffer for thread %d", i);
            g_config.threads = i;
            break;
        }
    }
    
    /* Initialize thread data */
    memset(&g_combined_result, 0, sizeof(g_combined_result));
    for (i = 0; i < g_config.threads; i++) {
        thread_data[i].buffer = buffers[i];
        thread_data[i].size = g_config.size;
        thread_data[i].id = i + 1;
        memset(&thread_data[i].result, 0, sizeof(benchmark_result_t));
    }
    
    /* Create threads */
    print_info("Starting benchmark threads...");
    for (i = 0; i < g_config.threads; i++) {
        ret = pthread_create(&threads[i], NULL, thread_benchmark, &thread_data[i]);
        if (ret != 0) {
            print_error("Failed to create thread %d: %s", i, strerror(ret));
            g_config.threads = i;
            break;
        }
    }
    
    /* Wait for threads */
    for (i = 0; i < g_config.threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Average results */
    if (g_config.threads > 1) {
        g_combined_result.bandwidth_read /= g_config.threads;
        g_combined_result.bandwidth_write /= g_config.threads;
        g_combined_result.bandwidth_copy /= g_config.threads;
        g_combined_result.latency_read /= g_config.threads;
        g_combined_result.latency_write /= g_config.threads;
        g_combined_result.throughput_malloc /= g_config.threads;
        g_combined_result.throughput_free /= g_config.threads;
        g_combined_result.throughput_memcpy /= g_config.threads;
        g_combined_result.throughput_memset /= g_config.threads;
    }
    
    /* Free buffers */
    for (i = 0; i < g_config.threads; i++) {
        if (buffers[i]) {
            ddr_free(buffers[i]);
        }
    }
    
    /* Display results */
    display_results();
}

/**
 * display_results - Display benchmark results
 */
static void display_results(void)
{
    char buffer[32];
    
    print_header("Benchmark Results");
    
    if (g_config.bandwidth || g_config.all) {
        printf("\n%sBandwidth:%s\n", COLOR_BOLD, COLOR_RESET);
        
        format_speed(g_combined_result.bandwidth_read, buffer, sizeof(buffer));
        printf("  Read:  %s\n", buffer);
        
        format_speed(g_combined_result.bandwidth_write, buffer, sizeof(buffer));
        printf("  Write: %s\n", buffer);
        
        format_speed(g_combined_result.bandwidth_copy, buffer, sizeof(buffer));
        printf("  Copy:  %s\n", buffer);
    }
    
    if (g_config.latency || g_config.all) {
        printf("\n%sLatency:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("  Read:  %.2f ns\n", g_combined_result.latency_read * 1e9);
        printf("  Write: %.2f ns\n", g_combined_result.latency_write * 1e9);
    }
    
    if (g_config.throughput || g_config.all) {
        printf("\n%sThroughput:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("  malloc: %.0f ops/s\n", g_combined_result.throughput_malloc);
        printf("  free:   %.0f ops/s\n", g_combined_result.throughput_free);
        
        format_speed(g_combined_result.throughput_memcpy, buffer, sizeof(buffer));
        printf("  memcpy: %s\n", buffer);
        
        format_speed(g_combined_result.throughput_memset, buffer, sizeof(buffer));
        printf("  memset: %s\n", buffer);
    }
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int opt;
    int ret;
    char *endptr;
    
    static struct option long_options[] = {
        {"bandwidth", no_argument, 0, 'b'},
        {"latency", no_argument, 0, 'l'},
        {"throughput", no_argument, 0, 't'},
        {"all", no_argument, 0, 'a'},
        {"size", required_argument, 0, 's'},
        {"iterations", required_argument, 0, 'i'},
        {"warmup", required_argument, 0, 'w'},
        {"threads", required_argument, 0, 'T'},
        {"pattern", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    printf("\n");
    printf("%s╔══════════════════════════════════════════════════════╗%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s║     DDR Memory Benchmark Tool                      ║%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s╚══════════════════════════════════════════════════════╝%s\n",
           COLOR_BLUE, COLOR_RESET);
    
    /* Parse arguments */
    while ((opt = getopt_long(argc, argv, "bltas:i:w:T:p:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'b':
                g_config.bandwidth = 1;
                break;
            case 'l':
                g_config.latency = 1;
                break;
            case 't':
                g_config.throughput = 1;
                break;
            case 'a':
                g_config.all = 1;
                break;
            case 's':
                g_config.size = strtol(optarg, &endptr, 0);
                if (*endptr == 'K' || *endptr == 'k') {
                    g_config.size *= 1024;
                } else if (*endptr == 'M' || *endptr == 'm') {
                    g_config.size *= 1024 * 1024;
                } else if (*endptr == 'G' || *endptr == 'g') {
                    g_config.size *= 1024 * 1024 * 1024;
                }
                if (g_config.size == 0) {
                    print_error("Invalid size: %s", optarg);
                    return 1;
                }
                break;
            case 'i':
                g_config.iterations = atoi(optarg);
                if (g_config.iterations <= 0) {
                    g_config.iterations = DEFAULT_ITERATIONS;
                }
                break;
            case 'w':
                g_config.warmup = atoi(optarg);
                if (g_config.warmup < 0) {
                    g_config.warmup = DEFAULT_WARMUP;
                }
                break;
            case 'T':
                g_config.threads = atoi(optarg);
                if (g_config.threads <= 0 || g_config.threads > MAX_THREADS) {
                    g_config.threads = DEFAULT_THREADS;
                }
                break;
            case 'p':
                g_config.pattern = optarg;
                break;
            case 'v':
                g_config.verbose = 1;
                break;
            case 'h':
                printf("\nDDR Memory Benchmark Tool\n\n");
                printf("Usage: %s [options]\n\n", argv[0]);
                printf("Options:\n");
                printf("  -b, --bandwidth   Run bandwidth tests\n");
                printf("  -l, --latency     Run latency tests\n");
                printf("  -t, --throughput  Run throughput tests\n");
                printf("  -a, --all         Run all tests\n");
                printf("  -s, --size        Test size (e.g., 1M, 4M, 1G)\n");
                printf("  -i, --iterations  Number of iterations\n");
                printf("  -w, --warmup      Warmup iterations\n");
                printf("  -T, --threads     Number of threads\n");
                printf("  -p, --pattern     Access pattern\n");
                printf("  -v, --verbose     Verbose output\n");
                printf("  -h, --help        Show this help\n\n");
                printf("Examples:\n");
                printf("  %s --all\n", argv[0]);
                printf("  %s --bandwidth --size 1M --iterations 100\n", argv[0]);
                printf("  %s --latency --size 64K --threads 4\n", argv[0]);
                return 0;
            default:
                print_error("Unknown option: %c", opt);
                return 1;
        }
    }
    
    /* If no specific test, run all */
    if (!g_config.bandwidth && !g_config.latency && !g_config.throughput && !g_config.all) {
        g_config.all = 1;
    }
    
    /* Initialize DDR Manager */
    ret = ddr_init();
    if (ret < 0) {
        print_error("Failed to initialize DDR Manager: %s", ddr_get_error_string(ret));
        return 1;
    }
    
    /* Run benchmark */
    run_benchmark();
    
    /* Cleanup */
    ddr_cleanup();
    
    print_success("Benchmark completed successfully");
    
    return 0;
}
