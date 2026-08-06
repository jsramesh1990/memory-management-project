/**
 * npu_memory_setup.c - NPU Memory Setup Example
 * 
 * This example demonstrates how to allocate, manage, and use NPU
 * (Neural Processing Unit) memory on RK3568-based systems.
 * It covers memory allocation, model loading, and inference.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o npu_memory_setup npu_memory_setup.c -lddr_manager -lrknnrt
 * 
 * Usage:
 *   ./npu_memory_setup [options]
 * 
 * Options:
 *   --model <path>    Path to RKNN model file
 *   --image <path>    Path to input image
 *   --test            Run inference test
 *   --help            Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_npu.h>
#include <ddr_debug.h>

/* RKNN Headers */
#include <rknn_api.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define NPU_MEMORY_POOL_SIZE   (128 * 1024 * 1024)  /* 128 MB */
#define NPU_MODEL_SIZE         (64 * 1024 * 1024)   /* 64 MB max model size */
#define NPU_INPUT_SIZE         (1024 * 1024)        /* 1 MB input buffer */
#define NPU_OUTPUT_SIZE        (1024 * 1024)        /* 1 MB output buffer */

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

/**
 * npu_context - NPU execution context
 */
typedef struct {
    int initialized;
    void *memory_pool;
    size_t pool_size;
    rknn_context rknn_ctx;
    rknn_tensor_attr input_attr;
    rknn_tensor_attr output_attr;
    void *input_buffer;
    void *output_buffer;
    uint32_t input_size;
    uint32_t output_size;
} npu_context_t;

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
 * print_hexdump - Print memory hexdump
 * @data: Data pointer
 * @size: Size to dump
 */
static void print_hexdump(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t i, j;
    
    for (i = 0; i < size; i += 16) {
        printf(COLOR_CYAN "  %08zx: " COLOR_RESET, i);
        for (j = 0; j < 16 && i + j < size; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\n");
    }
}

/**
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("NPU Memory Setup Example\n\n");
    printf("Usage: ./npu_memory_setup [options]\n\n");
    printf("Options:\n");
    printf("  --model <path>    Path to RKNN model file\n");
    printf("  --image <path>    Path to input image\n");
    printf("  --test            Run inference test\n");
    printf("  --help            Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./npu_memory_setup --model model.rknn --test\n");
    printf("  ./npu_memory_setup --model yolov5.rknn --image test.jpg\n");
}

/* ============================================================================
 * NPU Core Functions
 * ============================================================================ */

/**
 * npu_init - Initialize NPU subsystem
 * @ctx: NPU context
 * Return: 0 on success, negative error code on failure
 */
static int npu_init(npu_context_t *ctx)
{
    int ret;
    
    print_header("NPU Initialization");
    
    /* Clear context */
    memset(ctx, 0, sizeof(npu_context_t));
    
    /* Initialize DDR NPU memory pool */
    print_info("Allocating NPU memory pool (%zu MB)",
               NPU_MEMORY_POOL_SIZE / (1024 * 1024));
    
    ctx->memory_pool = ddr_npu_alloc(NPU_MEMORY_POOL_SIZE);
    if (!ctx->memory_pool) {
        print_error("Failed to allocate NPU memory pool");
        return -ENOMEM;
    }
    ctx->pool_size = NPU_MEMORY_POOL_SIZE;
    print_success("NPU memory pool allocated at %p", ctx->memory_pool);
    
    /* Initialize RKNN context */
    ret = rknn_init(&ctx->rknn_ctx, NULL, 0, 0, NULL);
    if (ret != RKNN_SUCC) {
        print_error("Failed to initialize RKNN context: %d", ret);
        ddr_npu_free(ctx->memory_pool);
        return -EIO;
    }
    print_success("RKNN context initialized");
    
    /* Allocate input buffer */
    ctx->input_buffer = ddr_npu_alloc(NPU_INPUT_SIZE);
    if (!ctx->input_buffer) {
        print_error("Failed to allocate input buffer");
        rknn_destroy(ctx->rknn_ctx);
        ddr_npu_free(ctx->memory_pool);
        return -ENOMEM;
    }
    ctx->input_size = NPU_INPUT_SIZE;
    print_success("Input buffer allocated: %zu bytes", ctx->input_size);
    
    /* Allocate output buffer */
    ctx->output_buffer = ddr_npu_alloc(NPU_OUTPUT_SIZE);
    if (!ctx->output_buffer) {
        print_error("Failed to allocate output buffer");
        ddr_npu_free(ctx->input_buffer);
        rknn_destroy(ctx->rknn_ctx);
        ddr_npu_free(ctx->memory_pool);
        return -ENOMEM;
    }
    ctx->output_size = NPU_OUTPUT_SIZE;
    print_success("Output buffer allocated: %zu bytes", ctx->output_size);
    
    ctx->initialized = 1;
    print_success("NPU subsystem initialized successfully");
    
    /* Print memory info */
    printf("\n  NPU Memory Information:\n");
    printf("  - Pool address: %p\n", ctx->memory_pool);
    printf("  - Pool size: %zu MB\n", ctx->pool_size / (1024 * 1024));
    printf("  - Input buffer: %p (%zu KB)\n", 
           ctx->input_buffer, ctx->input_size / 1024);
    printf("  - Output buffer: %p (%zu KB)\n",
           ctx->output_buffer, ctx->output_size / 1024);
    
    return 0;
}

/**
 * npu_load_model - Load RKNN model
 * @ctx: NPU context
 * @model_path: Path to model file
 * Return: 0 on success, negative error code on failure
 */
static int npu_load_model(npu_context_t *ctx, const char *model_path)
{
    int ret;
    FILE *fp;
    void *model_data;
    size_t model_size;
    struct stat st;
    
    print_header("NPU Model Loading");
    
    if (!ctx->initialized) {
        print_error("NPU not initialized");
        return -ENODEV;
    }
    
    /* Check if model file exists */
    if (stat(model_path, &st) != 0) {
        print_error("Model file not found: %s", model_path);
        return -ENOENT;
    }
    model_size = st.st_size;
    print_info("Model size: %zu bytes", model_size);
    
    /* Read model file */
    fp = fopen(model_path, "rb");
    if (!fp) {
        print_error("Failed to open model file: %s", model_path);
        return -EIO;
    }
    
    model_data = malloc(model_size);
    if (!model_data) {
        print_error("Failed to allocate model buffer");
        fclose(fp);
        return -ENOMEM;
    }
    
    if (fread(model_data, 1, model_size, fp) != model_size) {
        print_error("Failed to read model file");
        free(model_data);
        fclose(fp);
        return -EIO;
    }
    fclose(fp);
    print_success("Model file loaded: %s", model_path);
    
    /* Load model to NPU */
    print_info("Loading model to NPU...");
    ret = rknn_load_model(ctx->rknn_ctx, model_data, model_size);
    if (ret != RKNN_SUCC) {
        print_error("Failed to load model: %d", ret);
        free(model_data);
        return -EIO;
    }
    free(model_data);
    print_success("Model loaded successfully");
    
    /* Query input tensor attributes */
    ret = rknn_query(ctx->rknn_ctx, RKNN_QUERY_INPUT_ATTR, 
                     &ctx->input_attr, sizeof(ctx->input_attr));
    if (ret != RKNN_SUCC) {
        print_error("Failed to query input attributes: %d", ret);
        return -EIO;
    }
    
    /* Query output tensor attributes */
    ret = rknn_query(ctx->rknn_ctx, RKNN_QUERY_OUTPUT_ATTR,
                     &ctx->output_attr, sizeof(ctx->output_attr));
    if (ret != RKNN_SUCC) {
        print_error("Failed to query output attributes: %d", ret);
        return -EIO;
    }
    
    /* Print model info */
    printf("\n  Model Information:\n");
    printf("  - Input: %s (%u bytes)\n", 
           ctx->input_attr.name, ctx->input_attr.n_elems * 4);
    printf("  - Output: %s (%u bytes)\n",
           ctx->output_attr.name, ctx->output_attr.n_elems * 4);
    
    return 0;
}

/**
 * npu_inference - Run NPU inference
 * @ctx: NPU context
 * @input_data: Input data
 * @input_size: Input size
 * @output_data: Output data pointer
 * @output_size: Output size pointer
 * Return: 0 on success, negative error code on failure
 */
static int npu_inference(npu_context_t *ctx, void *input_data, 
                         size_t input_size, void **output_data, 
                         size_t *output_size)
{
    int ret;
    rknn_input inputs[1];
    rknn_output outputs[1];
    struct timespec start, end;
    double elapsed;
    
    print_header("NPU Inference");
    
    if (!ctx->initialized) {
        print_error("NPU not initialized");
        return -ENODEV;
    }
    
    /* Check input size */
    if (input_size > ctx->input_size) {
        print_error("Input data too large: %zu > %zu", 
                    input_size, ctx->input_size);
        return -EINVAL;
    }
    
    /* Copy input data */
    memcpy(ctx->input_buffer, input_data, input_size);
    print_info("Input data copied: %zu bytes", input_size);
    
    /* Set up inputs */
    inputs[0].type = RKNN_TENSOR_FLOAT32;
    inputs[0].buf = ctx->input_buffer;
    inputs[0].size = ctx->input_size;
    inputs[0].pass_through = 0;
    inputs[0].index = 0;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    
    /* Set up outputs */
    outputs[0].want_float = 1;
    outputs[0].is_prealloc = 1;
    outputs[0].buf = ctx->output_buffer;
    outputs[0].size = ctx->output_size;
    outputs[0].index = 0;
    
    /* Run inference */
    print_info("Running inference...");
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    ret = rknn_run(ctx->rknn_ctx, inputs, 1, outputs, 1);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    
    if (ret != RKNN_SUCC) {
        print_error("Inference failed: %d", ret);
        return -EIO;
    }
    print_success("Inference completed in %.3f ms", elapsed * 1000);
    
    /* Return output */
    if (output_data) {
        *output_data = ctx->output_buffer;
    }
    if (output_size) {
        *output_size = ctx->output_size;
    }
    
    /* Print output sample */
    printf("\n  Output Sample (first 16 bytes):\n");
    print_hexdump(ctx->output_buffer, 16);
    
    return 0;
}

/**
 * npu_cleanup - Clean up NPU resources
 * @ctx: NPU context
 */
static void npu_cleanup(npu_context_t *ctx)
{
    print_header("NPU Cleanup");
    
    if (ctx->output_buffer) {
        ddr_npu_free(ctx->output_buffer);
        print_info("Output buffer freed");
    }
    
    if (ctx->input_buffer) {
        ddr_npu_free(ctx->input_buffer);
        print_info("Input buffer freed");
    }
    
    if (ctx->rknn_ctx) {
        rknn_destroy(ctx->rknn_ctx);
        print_info("RKNN context destroyed");
    }
    
    if (ctx->memory_pool) {
        ddr_npu_free(ctx->memory_pool);
        print_info("NPU memory pool freed");
    }
    
    memset(ctx, 0, sizeof(npu_context_t));
    print_success("NPU cleanup completed");
}

/**
 * npu_test_inference - Test inference with random data
 * @ctx: NPU context
 * Return: 0 on success, negative error code on failure
 */
static int npu_test_inference(npu_context_t *ctx)
{
    int ret;
    void *test_input;
    void *output_data;
    size_t output_size;
    
    print_header("NPU Test Inference");
    
    /* Create test input */
    test_input = malloc(NPU_INPUT_SIZE);
    if (!test_input) {
        print_error("Failed to allocate test input");
        return -ENOMEM;
    }
    
    /* Fill with random data */
    srand(time(NULL));
    for (size_t i = 0; i < NPU_INPUT_SIZE / 4; i++) {
        ((uint32_t *)test_input)[i] = rand();
    }
    print_info("Test input generated: %zu bytes", NPU_INPUT_SIZE);
    
    /* Run inference */
    ret = npu_inference(ctx, test_input, NPU_INPUT_SIZE, 
                        &output_data, &output_size);
    if (ret) {
        print_error("Test inference failed: %d", ret);
        free(test_input);
        return ret;
    }
    
    print_success("Test inference completed");
    free(test_input);
    return 0;
}

/**
 * npu_memory_stats - Print NPU memory statistics
 * @ctx: NPU context
 */
static void npu_memory_stats(npu_context_t *ctx)
{
    struct ddr_npu_stats stats;
    
    print_header("NPU Memory Statistics");
    
    ddr_npu_get_stats(&stats);
    
    printf("\n  NPU Memory Usage:\n");
    printf("  - Total pool: %zu MB\n", stats.total_pool / (1024 * 1024));
    printf("  - Used: %zu MB\n", stats.used_memory / (1024 * 1024));
    printf("  - Free: %zu MB\n", stats.free_memory / (1024 * 1024));
    printf("  - Usage: %d%%\n", stats.usage_percent);
    
    printf("\n  Allocation Statistics:\n");
    printf("  - Allocations: %u\n", stats.alloc_count);
    printf("  - Deallocations: %u\n", stats.free_count);
    printf("  - Peak usage: %zu MB\n", stats.peak_usage / (1024 * 1024));
    printf("  - Fragmentation: %d%%\n", stats.fragmentation);
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int ret;
    npu_context_t ctx;
    const char *model_path = NULL;
    const char *image_path = NULL;
    int run_test = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     RK3568 NPU Memory Setup Example                 ║\n");
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 < argc) {
                model_path = argv[++i];
            }
        } else if (strcmp(argv[i], "--image") == 0) {
            if (i + 1 < argc) {
                image_path = argv[++i];
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
    
    /* Initialize NPU */
    ret = npu_init(&ctx);
    if (ret) {
        print_error("NPU initialization failed: %d", ret);
        return 1;
    }
    
    /* Load model if specified */
    if (model_path) {
        ret = npu_load_model(&ctx, model_path);
        if (ret) {
            print_error("Model loading failed: %d", ret);
            npu_cleanup(&ctx);
            return 1;
        }
        
        /* Run inference test */
        if (run_test) {
            ret = npu_test_inference(&ctx);
            if (ret) {
                print_error("Test inference failed: %d", ret);
            }
        }
    } else {
        print_info("No model specified. Run with --model to load a model.");
    }
    
    /* Print memory statistics */
    npu_memory_stats(&ctx);
    
    /* Cleanup */
    npu_cleanup(&ctx);
    
    printf("\n");
    print_header("Example Completed Successfully");
    printf("\n");
    
    return 0;
}
