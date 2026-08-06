/**
 * multi_camera_example.c - Multi-Camera NVR Example
 * 
 * This example demonstrates multiple camera support with DDR memory management
 * and NPU-based object detection.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o multi_camera_example multi_camera_example.c -lddr_manager -lrknnrt -lpthread -lm
 * 
 * Usage:
 *   ./multi_camera_example [options]
 * 
 * Options:
 *   --cameras <n>       Number of cameras (default: 4)
 *   --resolution <w>x<h> Resolution (default: 1920x1080)
 *   --fps <n>           Frames per second (default: 30)
 *   --detection         Enable object detection
 *   --model <path>      RKNN model path
 *   --record            Enable recording
 *   --output <dir>      Output directory
 *   --duration <s>      Run duration in seconds
 *   --verbose           Verbose output
 *   --help              Show this help message
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

/* DDR Manager Headers */
#include <ddr_manager.h>
#include <ddr_config.h>
#include <ddr_npu.h>

/* RKNN Headers */
#include <rknn_api.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define MAX_CAMERAS             16
#define DEFAULT_CAMERAS         4
#define DEFAULT_WIDTH           1920
#define DEFAULT_HEIGHT          1080
#define DEFAULT_FPS             30
#define FRAME_BUFFER_SIZE       (DEFAULT_WIDTH * DEFAULT_HEIGHT * 3)
#define MAX_FRAME_BUFFERS       32

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
    int id;
    int width;
    int height;
    int fps;
    int enabled;
    int detection_enabled;
    int recording_enabled;
    char name[64];
    char output_dir[256];
    void *frame_buffer;
    size_t frame_size;
    unsigned long frame_count;
    unsigned long detection_count;
    unsigned long recording_count;
    double fps_actual;
    struct timespec last_frame_time;
    pthread_t thread;
    int running;
} camera_t;

typedef struct {
    int num_cameras;
    camera_t cameras[MAX_CAMERAS];
    int resolution_width;
    int resolution_height;
    int fps;
    int detection_enabled;
    char model_path[256];
    int recording_enabled;
    char output_dir[256];
    int duration;
    int verbose;
    int running;
    rknn_context rknn_ctx;
    void *npu_memory;
    size_t npu_size;
    pthread_mutex_t mutex;
} system_t;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static system_t g_system;
static volatile int g_running = 1;

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
 * get_time_ms - Get current time in milliseconds
 */
static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

/* ============================================================================
 * Camera Functions
 * ============================================================================ */

/**
 * camera_init - Initialize camera
 */
static int camera_init(camera_t *cam, int id, int width, int height, int fps)
{
    memset(cam, 0, sizeof(*cam));
    cam->id = id;
    cam->width = width;
    cam->height = height;
    cam->fps = fps;
    cam->enabled = 1;
    cam->frame_size = width * height * 3;
    snprintf(cam->name, sizeof(cam->name), "Camera_%d", id);
    
    /* Allocate frame buffer */
    cam->frame_buffer = ddr_alloc(cam->frame_size, 0);
    if (!cam->frame_buffer) {
        print_error("Failed to allocate frame buffer for camera %d", id);
        return -ENOMEM;
    }
    
    /* Initialize frame buffer with test pattern */
    unsigned char *fb = (unsigned char *)cam->frame_buffer;
    for (int i = 0; i < cam->frame_size; i++) {
        fb[i] = (i + id) & 0xFF;
    }
    
    print_info("Camera %d initialized: %dx%d @ %dfps", id, width, height, fps);
    
    return 0;
}

/**
 * camera_capture - Simulate camera capture
 */
static int camera_capture(camera_t *cam)
{
    /* Simulate capturing a frame */
    unsigned char *fb = (unsigned char *)cam->frame_buffer;
    
    /* Update frame with moving pattern */
    static int frame_counter = 0;
    frame_counter++;
    
    for (int y = 0; y < cam->height; y++) {
        for (int x = 0; x < cam->width; x++) {
            int idx = (y * cam->width + x) * 3;
            fb[idx] = (x + frame_counter) & 0xFF;
            fb[idx+1] = (y + frame_counter) & 0xFF;
            fb[idx+2] = ((x + y + frame_counter) & 0xFF);
        }
    }
    
    cam->frame_count++;
    cam->fps_actual = cam->frame_count / (get_time_ms() / 1000.0);
    
    return 0;
}

/**
 * camera_detect - Run object detection on camera frame
 */
static int camera_detect(camera_t *cam, system_t *sys)
{
    if (!sys->detection_enabled || !cam->detection_enabled) {
        return 0;
    }
    
    /* Simulate detection processing */
    usleep(10000);  /* 10ms processing time */
    cam->detection_count++;
    
    return 0;
}

/**
 * camera_record - Record camera frame
 */
static int camera_record(camera_t *cam)
{
    if (!cam->recording_enabled) {
        return 0;
    }
    
    /* Simulate recording */
    if (cam->frame_count % 30 == 0) {
        /* Write frame every 30 frames */
        char filename[256];
        snprintf(filename, sizeof(filename), 
                 "%s/camera_%d_frame_%lu.bin",
                 cam->output_dir, cam->id, cam->frame_count);
        
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            fwrite(cam->frame_buffer, 1, cam->frame_size, fp);
            fclose(fp);
            cam->recording_count++;
        }
    }
    
    return 0;
}

/**
 * camera_thread - Camera processing thread
 */
static void *camera_thread(void *arg)
{
    camera_t *cam = (camera_t *)arg;
    system_t *sys = &g_system;
    struct timespec frame_time;
    double frame_interval = 1000.0 / cam->fps;
    double next_frame_time;
    
    clock_gettime(CLOCK_MONOTONIC, &frame_time);
    next_frame_time = get_time_ms();
    
    print_info("Camera %d thread started", cam->id);
    
    while (cam->running && g_running) {
        double current_time = get_time_ms();
        
        if (current_time >= next_frame_time) {
            /* Capture frame */
            camera_capture(cam);
            
            /* Process frame */
            if (sys->detection_enabled) {
                pthread_mutex_lock(&sys->mutex);
                camera_detect(cam, sys);
                pthread_mutex_unlock(&sys->mutex);
            }
            
            if (cam->recording_enabled) {
                camera_record(cam);
            }
            
            /* Update next frame time */
            next_frame_time += frame_interval;
            if (next_frame_time < current_time) {
                next_frame_time = current_time + frame_interval;
            }
            
            /* Print status periodically */
            if (cam->frame_count % (cam->fps * 5) == 0) {
                printf("\rCamera %d: %lu frames, %.1f fps, %lu detections, %lu recordings",
                       cam->id, cam->frame_count, cam->fps_actual,
                       cam->detection_count, cam->recording_count);
                fflush(stdout);
            }
        } else {
            /* Small sleep to avoid busy waiting */
            usleep(1000);
        }
    }
    
    print_info("Camera %d thread stopped", cam->id);
    return NULL;
}

/**
 * camera_start - Start camera thread
 */
static int camera_start(camera_t *cam)
{
    cam->running = 1;
    int ret = pthread_create(&cam->thread, NULL, camera_thread, cam);
    if (ret) {
        print_error("Failed to create camera %d thread", cam->id);
        cam->running = 0;
        return -1;
    }
    return 0;
}

/**
 * camera_stop - Stop camera thread
 */
static void camera_stop(camera_t *cam)
{
    cam->running = 0;
    if (cam->thread) {
        pthread_join(cam->thread, NULL);
        cam->thread = 0;
    }
}

/* ============================================================================
 * System Functions
 * ============================================================================ */

/**
 * system_init - Initialize system
 */
static int system_init(system_t *sys)
{
    int ret;
    
    memset(sys, 0, sizeof(*sys));
    sys->num_cameras = DEFAULT_CAMERAS;
    sys->resolution_width = DEFAULT_WIDTH;
    sys->resolution_height = DEFAULT_HEIGHT;
    sys->fps = DEFAULT_FPS;
    sys->running = 1;
    
    pthread_mutex_init(&sys->mutex, NULL);
    
    /* Initialize DDR subsystem */
    ret = ddr_config_init(BOARD_MIXTILE_EDGE2);
    if (ret) {
        print_error("Failed to initialize DDR: %d", ret);
        return ret;
    }
    print_success("DDR subsystem initialized");
    
    /* Initialize NPU memory */
    sys->npu_size = 128 * 1024 * 1024;  /* 128 MB */
    sys->npu_memory = ddr_npu_alloc(sys->npu_size);
    if (!sys->npu_memory) {
        print_error("Failed to allocate NPU memory");
        return -ENOMEM;
    }
    print_success("NPU memory allocated: %zu MB", sys->npu_size / (1024 * 1024));
    
    /* Initialize cameras */
    for (int i = 0; i < sys->num_cameras; i++) {
        ret = camera_init(&sys->cameras[i], i + 1, 
                         sys->resolution_width, sys->resolution_height,
                         sys->fps);
        if (ret) {
            print_error("Failed to initialize camera %d", i + 1);
            return ret;
        }
        
        /* Enable detection for some cameras */
        if (i % 2 == 0) {
            sys->cameras[i].detection_enabled = 1;
        }
        
        /* Enable recording for some cameras */
        if (i % 3 == 0) {
            sys->cameras[i].recording_enabled = 1;
            if (sys->output_dir[0]) {
                snprintf(sys->cameras[i].output_dir, 
                        sizeof(sys->cameras[i].output_dir),
                        "%s/camera_%d", sys->output_dir, i + 1);
                mkdir(sys->cameras[i].output_dir, 0755);
            }
        }
    }
    
    print_success("System initialized: %d cameras", sys->num_cameras);
    return 0;
}

/**
 * system_start - Start system
 */
static int system_start(system_t *sys)
{
    int ret = 0;
    
    print_info("Starting cameras...");
    
    for (int i = 0; i < sys->num_cameras; i++) {
        if (sys->cameras[i].enabled) {
            ret = camera_start(&sys->cameras[i]);
            if (ret) {
                print_error("Failed to start camera %d", i + 1);
                return ret;
            }
        }
    }
    
    print_success("All cameras started");
    return 0;
}

/**
 * system_stop - Stop system
 */
static void system_stop(system_t *sys)
{
    print_info("Stopping cameras...");
    
    sys->running = 0;
    
    for (int i = 0; i < sys->num_cameras; i++) {
        if (sys->cameras[i].enabled) {
            camera_stop(&sys->cameras[i]);
        }
    }
    
    print_success("All cameras stopped");
}

/**
 * system_cleanup - Clean up system
 */
static void system_cleanup(system_t *sys)
{
    /* Free NPU memory */
    if (sys->npu_memory) {
        ddr_npu_free(sys->npu_memory);
        sys->npu_memory = NULL;
    }
    
    /* Free camera buffers */
    for (int i = 0; i < sys->num_cameras; i++) {
        if (sys->cameras[i].frame_buffer) {
            ddr_free(sys->cameras[i].frame_buffer);
            sys->cameras[i].frame_buffer = NULL;
        }
    }
    
    /* Clean up DDR */
    ddr_allocator_cleanup();
    
    pthread_mutex_destroy(&sys->mutex);
}

/**
 * system_print_stats - Print system statistics
 */
static void system_print_stats(system_t *sys)
{
    double total_frames = 0;
    double total_detections = 0;
    double total_recordings = 0;
    
    print_header("System Statistics");
    
    printf("\n  Camera Statistics:\n");
    for (int i = 0; i < sys->num_cameras; i++) {
        camera_t *cam = &sys->cameras[i];
        printf("  Camera %d: %lu frames, %.1f fps, %lu detections, %lu recordings\n",
               cam->id, cam->frame_count, cam->fps_actual,
               cam->detection_count, cam->recording_count);
        total_frames += cam->frame_count;
        total_detections += cam->detection_count;
        total_recordings += cam->recording_count;
    }
    
    printf("\n  Totals:\n");
    printf("  Total Frames: %.0f\n", total_frames);
    printf("  Total Detections: %.0f\n", total_detections);
    printf("  Total Recordings: %.0f\n", total_recordings);
    
    /* Print memory usage */
    struct ddr_info info;
    if (ddr_config_get_info(&info) == 0) {
        printf("\n  Memory Usage:\n");
        printf("  Used: %u MB\n", info.used_memory);
        printf("  Free: %u MB\n", info.total_memory - info.used_memory);
        printf("  Usage: %.1f%%\n", 
               (float)info.used_memory / info.total_memory * 100.0);
    }
    
    printf("\n");
}

/* ============================================================================
 * Signal Handler
 * ============================================================================ */

static void signal_handler(int sig)
{
    print_info("Received signal %d, shutting down...", sig);
    g_running = 0;
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int ret = 0;
    int duration = 0;
    int verbose = 0;
    
    printf("\n");
    printf(COLOR_BLUE "╔══════════════════════════════════════════════════════╗\n");
    printf(COLOR_BLUE "║     Multi-Camera NVR Example                       ║\n");
    printf(COLOR_BLUE "╚══════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cameras") == 0 && i + 1 < argc) {
            g_system.num_cameras = atoi(argv[++i]);
            if (g_system.num_cameras > MAX_CAMERAS) {
                g_system.num_cameras = MAX_CAMERAS;
            }
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            int w, h;
            if (sscanf(argv[++i], "%dx%d", &w, &h) == 2) {
                g_system.resolution_width = w;
                g_system.resolution_height = h;
            }
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            g_system.fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--detection") == 0) {
            g_system.detection_enabled = 1;
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            strncpy(g_system.model_path, argv[++i], sizeof(g_system.model_path) - 1);
        } else if (strcmp(argv[i], "--record") == 0) {
            g_system.recording_enabled = 1;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            strncpy(g_system.output_dir, argv[++i], sizeof(g_system.output_dir) - 1);
            mkdir(g_system.output_dir, 0755);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else {
            print_error("Unknown option: %s", argv[i]);
            show_help();
            return 1;
        }
    }
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize system */
    ret = system_init(&g_system);
    if (ret) {
        print_error("System initialization failed: %d", ret);
        goto cleanup;
    }
    
    /* Start system */
    ret = system_start(&g_system);
    if (ret) {
        print_error("System start failed: %d", ret);
        goto cleanup;
    }
    
    print_info("System running with %d cameras", g_system.num_cameras);
    print_info("Press Ctrl+C to stop");
    
    /* Run for specified duration or until interrupted */
    if (duration > 0) {
        print_info("Running for %d seconds...", duration);
        int remaining = duration;
        while (remaining > 0 && g_running) {
            sleep(1);
            remaining--;
            if (remaining % 10 == 0) {
                printf("\rRemaining: %d seconds", remaining);
                fflush(stdout);
            }
        }
        printf("\n");
    } else {
        /* Run until interrupted */
        while (g_running) {
            sleep(1);
        }
    }
    
    print_info("Shutting down...");
    
    /* Stop system */
    system_stop(&g_system);
    
    /* Print statistics */
    system_print_stats(&g_system);

cleanup:
    /* Clean up */
    system_cleanup(&g_system);
    
    if (ret == 0) {
        print_success("Example completed successfully!");
    } else {
        print_error("Example failed with error %d", ret);
    }
    
    return ret;
}

/**
 * show_help - Display help message
 */
static void show_help(void)
{
    printf("Multi-Camera NVR Example\n\n");
    printf("Usage: ./multi_camera_example [options]\n\n");
    printf("Options:\n");
    printf("  --cameras <n>       Number of cameras (default: 4)\n");
    printf("  --resolution <w>x<h> Resolution (default: 1920x1080)\n");
    printf("  --fps <n>           Frames per second (default: 30)\n");
    printf("  --detection         Enable object detection\n");
    printf("  --model <path>      RKNN model path\n");
    printf("  --record            Enable recording\n");
    printf("  --output <dir>      Output directory\n");
    printf("  --duration <s>      Run duration in seconds\n");
    printf("  --verbose           Verbose output\n");
    printf("  --help              Show this help message\n\n");
    printf("Examples:\n");
    printf("  ./multi_camera_example --cameras 8 --detection\n");
    printf("  ./multi_camera_example --record --output /mnt/nvr --duration 60\n");
}
