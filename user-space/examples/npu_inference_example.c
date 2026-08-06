/**
 * npu_inference_example.c - NPU Inference Example
 * 
 * This example demonstrates NPU inference using RKNN on RK3568.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o npu_inference_example npu_inference_example.c -lddr_manager -lrknnrt -lm
 * 
 * Usage:
 *   ./npu_inference_example [options]
 * 
 * Options:
 *   --model <path>      Path to RKNN model file
 *   --image <path>      Path to input image
 *   --output <path>     Path to output file
 *   --verbose           Verbose output
 *   --test              Run test inference
 *   --help              Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_config.h>
#include <ddr_npu.h>

/* RKNN Headers */
#include <rknn_api.h>

/* Image Processing Headers */
#include <jpeglib.h>
#include <png.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define NPU_MEMORY_POOL_SIZE    (128 * 1024 * 1024)  /* 128 MB */
#define INPUT_SIZE              640
#define OUTPUT_SIZE             640
#define MAX_DETECTIONS          20

/* Colors for terminal output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

/* COCO Class Names */
static const char *class_names[] = {
    "person", "bicycle", "car", "motorcycle", "airplane",
    "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench",
    "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack",
    "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat",
    "baseball glove", "skateboard", "surfboard", "tennis racket",
    "bottle", "wine glass", "cup", "fork", "knife",
    "spoon", "bowl", "banana", "apple", "sandwich",
    "orange", "broccoli", "carrot", "hot dog", "pizza",
    "donut", "cake", "chair", "couch", "potted plant",
    "bed", "dining table", "toilet", "tv", "laptop",
    "mouse", "remote", "keyboard", "cell phone", "microwave",
    "oven", "toaster", "sink", "refrigerator", "book",
    "clock", "vase", "scissors", "teddy bear", "hair drier",
    "toothbrush"
};

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct {
    float x1, y1, x2, y2;
    float score;
    int class_id;
    char class_name[64];
} detection_t;

typedef struct {
    rknn_context ctx;
    rknn_tensor_attr input_attr;
    rknn_tensor_attr output_attr;
    void *input_buffer;
    void *output_buffer;
    uint32_t input_size;
    uint32_t output_size;
    int initialized;
    char model_path[256];
} npu_context_t;

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
 * print_detection - Print detection result
 */
static void print_detection(detection_t *det)
{
    printf("  %s: %.2f%% [%.1f, %.1f, %.1f, %.1f]\n",
           det->class_name, det->score * 100,
           det->x1, det->y1, det->x2, det->y2);
}

/**
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("NPU Inference Example\n\n");
    printf("Usage: ./npu_inference_example [options]\n\n");
    printf("Options:\n");
    printf("  --model <path>      Path to RKNN model file\n");
    printf("  --image <path>      Path to input image\n");
    printf("  --output <path>     Path to output file\n");
    printf("  --verbose           Verbose output\n");
    printf("  --test              Run test inference\n");
    printf("  --help              Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./npu_inference_example --model model.rknn --image test.jpg\n");
    printf("  ./npu_inference_example --test\n");
}

/* ============================================================================
 * Image Processing Functions
 * ============================================================================ */

/**
 * read_jpeg - Read JPEG image
 */
static unsigned char *read_jpeg(const char *filename, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned char *image_data;
    unsigned char *row_ptr;
    int row_stride;
    
    FILE *infile = fopen(filename, "rb");
    if (!infile) {
        print_error("Failed to open JPEG file: %s", filename);
        return NULL;
    }
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    
    row_stride = cinfo.output_width * cinfo.output_components;
    image_data = malloc(row_stride * cinfo.output_height);
    if (!image_data) {
        print_error("Failed to allocate image buffer");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return NULL;
    }
    
    while (cinfo.output_scanline < cinfo.output_height) {
        row_ptr = image_data + cinfo.output_scanline * row_stride;
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
    }
    
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    
    return image_data;
}

/**
 * read_png - Read PNG image
 */
static unsigned char *read_png(const char *filename, int *width, int *height)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned char *image_data;
    png_bytepp row_pointers;
    int bit_depth, color_type;
    int row_bytes;
    
    FILE *infile = fopen(filename, "rb");
    if (!infile) {
        print_error("Failed to open PNG file: %s", filename);
        return NULL;
    }
    
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        print_error("Failed to create PNG read struct");
        fclose(infile);
        return NULL;
    }
    
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        print_error("Failed to create PNG info struct");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(infile);
        return NULL;
    }
    
    if (setjmp(png_jmpbuf(png_ptr))) {
        print_error("Error reading PNG");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(infile);
        return NULL;
    }
    
    png_init_io(png_ptr, infile);
    png_read_info(png_ptr, info_ptr);
    
    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        png_set_strip_alpha(png_ptr);
    }
    
    row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    image_data = malloc(row_bytes * *height);
    if (!image_data) {
        print_error("Failed to allocate image buffer");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(infile);
        return NULL;
    }
    
    row_pointers = malloc(sizeof(png_bytep) * *height);
    if (!row_pointers) {
        print_error("Failed to allocate row pointers");
        free(image_data);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(infile);
        return NULL;
    }
    
    for (int y = 0; y < *height; y++) {
        row_pointers[y] = image_data + y * row_bytes;
    }
    
    png_read_image(png_ptr, row_pointers);
    
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(infile);
    
    return image_data;
}

/**
 * preprocess_image - Preprocess image for inference
 */
static float *preprocess_image(unsigned char *image, int width, int height, 
                               int *out_width, int *out_height)
{
    float *input_data;
    int target_size = 640;
    float scale;
    
    *out_width = target_size;
    *out_height = target_size;
    
    input_data = malloc(target_size * target_size * 3 * sizeof(float));
    if (!input_data) {
        print_error("Failed to allocate input buffer");
        return NULL;
    }
    
    /* Resize and normalize image */
    scale = (float)target_size / (width > height ? width : height);
    int new_width = (int)(width * scale);
    int new_height = (int)(height * scale);
    int offset_x = (target_size - new_width) / 2;
    int offset_y = (target_size - new_height) / 2;
    
    /* Simple nearest neighbor resize */
    for (int y = 0; y < target_size; y++) {
        for (int x = 0; x < target_size; x++) {
            int src_x, src_y;
            if (x < offset_x || x >= offset_x + new_width ||
                y < offset_y || y >= offset_y + new_height) {
                /* Black padding */
                for (int c = 0; c < 3; c++) {
                    input_data[(y * target_size + x) * 3 + c] = 0.0f;
                }
            } else {
                src_x = (x - offset_x) / scale;
                src_y = (y - offset_y) / scale;
                if (src_x >= width) src_x = width - 1;
                if (src_y >= height) src_y = height - 1;
                
                for (int c = 0; c < 3; c++) {
                    input_data[(y * target_size + x) * 3 + c] = 
                        image[(src_y * width + src_x) * 3 + c] / 255.0f;
                }
            }
        }
    }
    
    return input_data;
}

/* ============================================================================
 * NPU Functions
 * ============================================================================ */

/**
 * npu_init - Initialize NPU context
 */
static int npu_init(npu_context_t *ctx, const char *model_path)
{
    int ret;
    FILE *fp;
    void *model_data;
    size_t model_size;
    struct stat st;
    
    print_info("Initializing NPU context...");
    
    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->model_path, model_path, sizeof(ctx->model_path) - 1);
    
    /* Read model file */
    if (stat(model_path, &st) != 0) {
        print_error("Model file not found: %s", model_path);
        return -ENOENT;
    }
    model_size = st.st_size;
    print_info("Model size: %zu bytes", model_size);
    
    fp = fopen(model_path, "rb");
    if (!fp) {
        print_error("Failed to open model: %s", model_path);
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
    
    /* Initialize RKNN context */
    ret = rknn_init(&ctx->ctx, model_data, model_size, 0, NULL);
    free(model_data);
    
    if (ret != RKNN_SUCC) {
        print_error("Failed to initialize RKNN: %d", ret);
        return -EIO;
    }
    print_success("RKNN context initialized");
    
    /* Query input attributes */
    ret = rknn_query(ctx->ctx, RKNN_QUERY_INPUT_ATTR, 
                     &ctx->input_attr, sizeof(ctx->input_attr));
    if (ret != RKNN_SUCC) {
        print_error("Failed to query input attributes: %d", ret);
        rknn_destroy(ctx->ctx);
        return -EIO;
    }
    ctx->input_size = ctx->input_attr.n_elems * sizeof(float);
    print_info("Input: %s (%u bytes)", 
               ctx->input_attr.name, ctx->input_size);
    
    /* Query output attributes */
    ret = rknn_query(ctx->ctx, RKNN_QUERY_OUTPUT_ATTR, 
                     &ctx->output_attr, sizeof(ctx->output_attr));
    if (ret != RKNN_SUCC) {
        print_error("Failed to query output attributes: %d", ret);
        rknn_destroy(ctx->ctx);
        return -EIO;
    }
    ctx->output_size = ctx->output_attr.n_elems * sizeof(float);
    print_info("Output: %s (%u bytes)", 
               ctx->output_attr.name, ctx->output_size);
    
    /* Allocate input buffer */
    ctx->input_buffer = malloc(ctx->input_size);
    if (!ctx->input_buffer) {
        print_error("Failed to allocate input buffer");
        rknn_destroy(ctx->ctx);
        return -ENOMEM;
    }
    
    /* Allocate output buffer */
    ctx->output_buffer = malloc(ctx->output_size);
    if (!ctx->output_buffer) {
        print_error("Failed to allocate output buffer");
        free(ctx->input_buffer);
        rknn_destroy(ctx->ctx);
        return -ENOMEM;
    }
    
    ctx->initialized = 1;
    print_success("NPU context initialized successfully");
    
    return 0;
}

/**
 * npu_inference - Run NPU inference
 */
static int npu_inference(npu_context_t *ctx, float *input_data, float **output_data)
{
    rknn_input inputs[1];
    rknn_output outputs[1];
    int ret;
    struct timespec start, end;
    double elapsed;
    
    if (!ctx->initialized) {
        print_error("NPU context not initialized");
        return -ENODEV;
    }
    
    /* Copy input data */
    memcpy(ctx->input_buffer, input_data, ctx->input_size);
    
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
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    ret = rknn_run(ctx->ctx, inputs, 1, outputs, 1);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (end.tv_sec - start.tv_sec) + 
              (end.tv_nsec - start.tv_nsec) / 1e9;
    
    if (ret != RKNN_SUCC) {
        print_error("Inference failed: %d", ret);
        return -EIO;
    }
    
    print_info("Inference completed in %.2f ms", elapsed * 1000);
    
    *output_data = ctx->output_buffer;
    
    return 0;
}

/**
 * npu_cleanup - Clean up NPU context
 */
static void npu_cleanup(npu_context_t *ctx)
{
    if (ctx->initialized) {
        if (ctx->ctx) {
            rknn_destroy(ctx->ctx);
        }
        if (ctx->input_buffer) {
            free(ctx->input_buffer);
        }
        if (ctx->output_buffer) {
            free(ctx->output_buffer);
        }
        ctx->initialized = 0;
        print_success("NPU context cleaned up");
    }
}

/**
 * postprocess_detections - Postprocess detection results
 */
static int postprocess_detections(float *output, int output_size, 
                                  detection_t *detections, int max_detections)
{
    int num_detections = 0;
    float *boxes = output;
    float *scores = output + 4 * max_detections;
    float *classes = output + 8 * max_detections;
    
    for (int i = 0; i < max_detections && i < output_size / 12; i++) {
        if (scores[i] > 0.5) {
            detections[num_detections].x1 = boxes[i * 4];
            detections[num_detections].y1 = boxes[i * 4 + 1];
            detections[num_detections].x2 = boxes[i * 4 + 2];
            detections[num_detections].y2 = boxes[i * 4 + 3];
            detections[num_detections].score = scores[i];
            detections[num_detections].class_id = (int)classes[i];
            
            int class_idx = (int)classes[i];
            if (class_idx >= 0 && class_idx < sizeof(class_names) / sizeof(class_names[0])) {
                strcpy(detections[num_detections].class_name, class_names[class_idx]);
            } else {
                snprintf(detections[num_detections].class_name, 
                        sizeof(detections[num_detections].class_name),
                        "class_%d", class_idx);
            }
            
            num_detections++;
        }
    }
    
    return num_detections;
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * test_inference - Run test inference
 */
static int test_inference(void)
{
    int ret;
    npu_context_t ctx;
    float *input_data;
    float *output_data;
    detection_t detections[MAX_DETECTIONS];
    int num_detections;
    int width, height;
    unsigned char *image_data;
    const char *test_model = "test_model.rknn";
    const char *test_image = "test.jpg";
    
    print_header("Test Inference");
    
    /* Initialize NPU */
    ret = npu_init(&ctx, test_model);
    if (ret) {
        print_error("Failed to initialize NPU: %d", ret);
        return ret;
    }
    
    /* Load test image */
    image_data = read_jpeg(test_image, &width, &height);
    if (!image_data) {
        /* Try PNG */
        image_data = read_png(test_image, &width, &height);
        if (!image_data) {
            /* Create synthetic image */
            print_info("Creating synthetic test image");
            width = 640;
            height = 480;
            image_data = malloc(width * height * 3);
            if (image_data) {
                for (int i = 0; i < width * height * 3; i++) {
                    image_data[i] = rand() & 0xFF;
                }
            }
        }
    }
    
    if (!image_data) {
        print_error("Failed to load/create test image");
        npu_cleanup(&ctx);
        return -ENOMEM;
    }
    
    print_info("Image: %dx%d", width, height);
    
    /* Preprocess image */
    int out_width, out_height;
    input_data = preprocess_image(image_data, width, height, &out_width, &out_height);
    free(image_data);
    
    if (!input_data) {
        print_error("Failed to preprocess image");
        npu_cleanup(&ctx);
        return -ENOMEM;
    }
    
    /* Run inference */
    ret = npu_inference(&ctx, input_data, &output_data);
    free(input_data);
    
    if (ret) {
        print_error("Inference failed: %d", ret);
        npu_cleanup(&ctx);
        return ret;
    }
    
    /* Postprocess detections */
    num_detections = postprocess_detections(output_data, ctx.output_size / sizeof(float),
                                            detections, MAX_DETECTIONS);
    
    printf("\n  Detections: %d\n", num_detections);
    for (int i = 0; i < num_detections; i++) {
        print_detection(&detections[i]);
    }
    
    /* Clean up */
    npu_cleanup(&ctx);
    
    print_success("Test inference completed");
    
    return 0;
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    const char *model_path = NULL;
    const char *image_path = NULL;
    const char *output_path = NULL;
    int verbose = 0;
    int run_test = 0;
    int ret = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     NPU Inference Example                          ║\n");
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
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
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
    
    /* Initialize NPU memory */
    ret = ddr_npu_init(NPU_MEMORY_POOL_SIZE);
    if (ret) {
        print_error("Failed to initialize NPU memory: %d", ret);
        return ret;
    }
    print_success("NPU memory initialized");
    
    /* Run test or examples */
    if (run_test) {
        ret = test_inference();
    } else if (model_path && image_path) {
        /* Run inference on image */
        npu_context_t ctx;
        unsigned char *image_data;
        float *input_data;
        float *output_data;
        detection_t detections[MAX_DETECTIONS];
        int num_detections;
        int width, height;
        
        print_header("Running Inference");
        print_info("Model: %s", model_path);
        print_info("Image: %s", image_path);
        
        ret = npu_init(&ctx, model_path);
        if (ret) {
            print_error("Failed to initialize NPU: %d", ret);
            goto cleanup;
        }
        
        /* Load image */
        image_data = read_jpeg(image_path, &width, &height);
        if (!image_data) {
            image_data = read_png(image_path, &width, &height);
        }
        
        if (!image_data) {
            print_error("Failed to load image: %s", image_path);
            ret = -EIO;
            npu_cleanup(&ctx);
            goto cleanup;
        }
        
        print_info("Image: %dx%d", width, height);
        
        /* Preprocess image */
        int out_width, out_height;
        input_data = preprocess_image(image_data, width, height, 
                                      &out_width, &out_height);
        free(image_data);
        
        if (!input_data) {
            print_error("Failed to preprocess image");
            ret = -ENOMEM;
            npu_cleanup(&ctx);
            goto cleanup;
        }
        
        /* Run inference */
        ret = npu_inference(&ctx, input_data, &output_data);
        free(input_data);
        
        if (ret) {
            print_error("Inference failed: %d", ret);
            npu_cleanup(&ctx);
            goto cleanup;
        }
        
        /* Postprocess detections */
        num_detections = postprocess_detections(output_data, 
                                                ctx.output_size / sizeof(float),
                                                detections, MAX_DETECTIONS);
        
        printf("\n  Detections: %d\n", num_detections);
        for (int i = 0; i < num_detections; i++) {
            print_detection(&detections[i]);
        }
        
        /* Save output if specified */
        if (output_path) {
            FILE *fp = fopen(output_path, "w");
            if (fp) {
                fprintf(fp, "Detections: %d\n", num_detections);
                for (int i = 0; i < num_detections; i++) {
                    fprintf(fp, "%s,%.3f,%.1f,%.1f,%.1f,%.1f\n",
                            detections[i].class_name,
                            detections[i].score,
                            detections[i].x1, detections[i].y1,
                            detections[i].x2, detections[i].y2);
                }
                fclose(fp);
                print_success("Output saved to: %s", output_path);
            } else {
                print_error("Failed to save output: %s", output_path);
            }
        }
        
        npu_cleanup(&ctx);
        print_success("Inference completed");
    } else {
        print_info("No model or image specified. Running test...");
        ret = test_inference();
    }

cleanup:
    /* Clean up NPU memory */
    ddr_npu_cleanup();
    
    if (ret == 0) {
        print_success("Example completed successfully!");
    } else {
        print_error("Example failed with error %d", ret);
    }
    
    return ret;
}
