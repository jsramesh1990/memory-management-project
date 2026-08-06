/**
 * ddr_monitor.c - DDR Memory Monitor
 * 
 * This tool provides real-time monitoring of DDR memory usage,
 * performance metrics, and system health.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * Compilation:
 *   gcc -o ddr_monitor ddr_monitor.c -lddr_manager -lncurses -lm
 * 
 * Usage:
 *   ./ddr_monitor [options]
 *   ./ddr_monitor --live
 *   ./ddr_monitor --daemon
 *   ./ddr_monitor --log
 *   ./ddr_monitor --threshold 85
 *   ./ddr_monitor --help
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
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#ifdef HAVE_NCURSES
#include <ncurses.h>
#endif

#include <ddr_manager.h>

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define MAX_HISTORY             1000
#define DEFAULT_INTERVAL        1.0
#define DEFAULT_THRESHOLD       85.0
#define LOG_FILE_DEFAULT        "/var/log/ddr_monitor.log"
#define PID_FILE_DEFAULT        "/var/run/ddr_monitor.pid"

/* Colors (ANSI) */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
    double timestamp;
    unsigned int used_memory;
    unsigned int total_memory;
    unsigned int frequency;
    unsigned int temperature;
    unsigned int errors;
    float usage_percent;
} memory_sample_t;

typedef struct {
    memory_sample_t samples[MAX_HISTORY];
    int count;
    int head;
    int tail;
    pthread_mutex_t lock;
} history_t;

typedef struct {
    int running;
    int daemon;
    int live;
    int log_enabled;
    int verbose;
    double interval;
    float threshold;
    char log_file[256];
    char pid_file[256];
    history_t history;
} monitor_config_t;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static monitor_config_t g_config = {
    .running = 1,
    .daemon = 0,
    .live = 1,
    .log_enabled = 0,
    .verbose = 0,
    .interval = DEFAULT_INTERVAL,
    .threshold = DEFAULT_THRESHOLD,
    .log_file = LOG_FILE_DEFAULT,
    .pid_file = PID_FILE_DEFAULT,
};

static pthread_t g_monitor_thread;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

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
 * get_timestamp - Get timestamp string
 */
static void get_timestamp(char *buffer, size_t size)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
}

/**
 * log_message - Write to log file
 */
static void log_message(const char *level, const char *message)
{
    if (!g_config.log_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_log_mutex);
    
    FILE *fp = fopen(g_config.log_file, "a");
    if (fp) {
        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(fp, "[%s] [%s] %s\n", timestamp, level, message);
        fclose(fp);
    }
    
    pthread_mutex_unlock(&g_log_mutex);
}

/**
 * check_threshold - Check if threshold is exceeded
 */
static void check_threshold(float usage, const char *timestamp)
{
    if (usage >= g_config.threshold) {
        char message[256];
        snprintf(message, sizeof(message),
                "Memory usage %.1f%% exceeds threshold %.1f%%",
                usage, g_config.threshold);
        
        if (usage >= 95.0) {
            print_error("%s", message);
            log_message("CRITICAL", message);
        } else if (usage >= 90.0) {
            print_warning("%s", message);
            log_message("WARNING", message);
        } else {
            print_warning("%s", message);
            log_message("WARNING", message);
        }
    }
}

/* ============================================================================
 * History Functions
 * ============================================================================ */

/**
 * history_init - Initialize history
 */
static void history_init(history_t *history)
{
    memset(history, 0, sizeof(*history));
    history->count = 0;
    history->head = 0;
    history->tail = 0;
    pthread_mutex_init(&history->lock, NULL);
}

/**
 * history_add - Add sample to history
 */
static void history_add(history_t *history, memory_sample_t *sample)
{
    pthread_mutex_lock(&history->lock);
    
    if (history->count < MAX_HISTORY) {
        history->samples[history->tail] = *sample;
        history->tail = (history->tail + 1) % MAX_HISTORY;
        history->count++;
    } else {
        history->samples[history->head] = *sample;
        history->head = (history->head + 1) % MAX_HISTORY;
        history->tail = (history->tail + 1) % MAX_HISTORY;
    }
    
    pthread_mutex_unlock(&history->lock);
}

/**
 * history_get_stats - Get statistics from history
 */
static void history_get_stats(history_t *history, float *min, float *max, float *avg)
{
    pthread_mutex_lock(&history->lock);
    
    if (history->count == 0) {
        *min = *max = *avg = 0;
        pthread_mutex_unlock(&history->lock);
        return;
    }
    
    *min = 100.0;
    *max = 0.0;
    *avg = 0.0;
    
    for (int i = 0; i < history->count; i++) {
        float usage = history->samples[i].usage_percent;
        if (usage < *min) *min = usage;
        if (usage > *max) *max = usage;
        *avg += usage;
    }
    
    *avg /= history->count;
    
    pthread_mutex_unlock(&history->lock);
}

/* ============================================================================
 * Monitor Functions
 * ============================================================================ */

/**
 * monitor_collect - Collect memory sample
 */
static int monitor_collect(memory_sample_t *sample)
{
    struct ddr_info info;
    int ret;
    
    ret = ddr_get_info(&info);
    if (ret < 0) {
        return ret;
    }
    
    sample->timestamp = (double)time(NULL);
    sample->used_memory = info.used_memory;
    sample->total_memory = info.total_memory;
    sample->frequency = info.frequency;
    sample->temperature = info.temperature;
    sample->errors = info.errors;
    sample->usage_percent = (float)info.used_memory / info.total_memory * 100.0;
    
    return 0;
}

/**
 * monitor_loop - Main monitoring loop
 */
static void *monitor_loop(void *arg)
{
    memory_sample_t sample;
    char timestamp[32];
    int ret;
    
    print_info("Monitor started (interval: %.1f s)", g_config.interval);
    log_message("INFO", "Monitor started");
    
    while (g_config.running) {
        ret = monitor_collect(&sample);
        if (ret < 0) {
            print_error("Failed to collect sample: %s", ddr_get_error_string(ret));
            sleep(1);
            continue;
        }
        
        get_timestamp(timestamp, sizeof(timestamp));
        
        /* Add to history */
        history_add(&g_config.history, &sample);
        
        /* Check threshold */
        check_threshold(sample.usage_percent, timestamp);
        
        /* Print update */
        if (g_config.verbose) {
            printf("\r%s: %.1f%% used, %u MB/%u MB, %u C, %u errors",
                   timestamp,
                   sample.usage_percent,
                   sample.used_memory,
                   sample.total_memory,
                   sample.temperature,
                   sample.errors);
            fflush(stdout);
        }
        
        /* Log if enabled */
        if (g_config.log_enabled) {
            char message[256];
            snprintf(message, sizeof(message),
                    "Memory: %.1f%% (%u/%u MB), Temp: %u C, Errors: %u",
                    sample.usage_percent,
                    sample.used_memory,
                    sample.total_memory,
                    sample.temperature,
                    sample.errors);
            log_message("INFO", message);
        }
        
        /* Sleep */
        usleep((useconds_t)(g_config.interval * 1000000));
    }
    
    print_info("Monitor stopped");
    log_message("INFO", "Monitor stopped");
    
    return NULL;
}

/**
 * monitor_start - Start monitoring
 */
static int monitor_start(void)
{
    int ret;
    
    ret = pthread_create(&g_monitor_thread, NULL, monitor_loop, NULL);
    if (ret != 0) {
        print_error("Failed to create monitor thread: %s", strerror(ret));
        return -1;
    }
    
    return 0;
}

/**
 * monitor_stop - Stop monitoring
 */
static void monitor_stop(void)
{
    g_config.running = 0;
    if (g_monitor_thread) {
        pthread_join(g_monitor_thread, NULL);
        g_monitor_thread = 0;
    }
}

/**
 * monitor_daemonize - Run as daemon
 */
static void monitor_daemonize(void)
{
    pid_t pid, sid;
    int fd;
    
    /* Fork */
    pid = fork();
    if (pid < 0) {
        print_error("Failed to fork: %s", strerror(errno));
        exit(1);
    }
    
    if (pid > 0) {
        /* Parent exits */
        exit(0);
    }
    
    /* Child: Create new session */
    sid = setsid();
    if (sid < 0) {
        print_error("Failed to create session: %s", strerror(errno));
        exit(1);
    }
    
    /* Change working directory */
    chdir("/");
    
    /* Close file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    /* Open log file if enabled */
    if (g_config.log_enabled) {
        fd = open(g_config.log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
    }
    
    /* Write PID file */
    if (g_config.pid_file[0]) {
        FILE *fp = fopen(g_config.pid_file, "w");
        if (fp) {
            fprintf(fp, "%d\n", getpid());
            fclose(fp);
        }
    }
    
    print_info("Daemon started (PID: %d)", getpid());
    log_message("INFO", "Daemon started");
}

/* ============================================================================
 * Display Functions (NCURSES)
 * ============================================================================ */

#ifdef HAVE_NCURSES

/**
 * display_live - Live display with ncurses
 */
static void display_live(void)
{
    memory_sample_t sample;
    char timestamp[32];
    int ch;
    int ret;
    float min, max, avg;
    
    /* Initialize ncurses */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100);
    
    print_info("Live display started (Press 'q' to quit)");
    
    while (g_config.running) {
        /* Get sample */
        ret = monitor_collect(&sample);
        if (ret < 0) {
            mvprintw(0, 0, "Error: %s", ddr_get_error_string(ret));
            refresh();
            continue;
        }
        
        get_timestamp(timestamp, sizeof(timestamp));
        history_add(&g_config.history, &sample);
        history_get_stats(&g_config.history, &min, &max, &avg);
        
        /* Clear screen */
        clear();
        
        /* Header */
        attron(A_BOLD);
        mvprintw(0, 0, "DDR Memory Monitor - %s", timestamp);
        attroff(A_BOLD);
        mvprintw(1, 0, "Press 'q' to quit, 'r' to reset, 'h' for help");
        
        /* Memory usage */
        mvprintw(3, 0, "Memory Usage:");
        mvprintw(4, 0, "  Total: %u MB", sample.total_memory);
        mvprintw(5, 0, "  Used: %u MB (%.1f%%)", 
                sample.used_memory, sample.usage_percent);
        mvprintw(6, 0, "  Free: %u MB", 
                sample.total_memory - sample.used_memory);
        
        /* Progress bar */
        int bar_width = 40;
        int filled = (int)((sample.usage_percent / 100.0) * bar_width);
        mvprintw(8, 0, "  [");
        attron(COLOR_PAIR(1));
        for (int i = 0; i < filled; i++) mvprintw(8, 3 + i, " ");
        attroff(COLOR_PAIR(1));
        for (int i = filled; i < bar_width; i++) mvprintw(8, 3 + i, ".");
        mvprintw(8, 3 + bar_width, "] %.1f%%", sample.usage_percent);
        
        /* System info */
        mvprintw(10, 0, "System Information:");
        mvprintw(11, 0, "  Frequency: %u MHz", sample.frequency);
        mvprintw(12, 0, "  Temperature: %u C", sample.temperature);
        mvprintw(13, 0, "  Errors: %u", sample.errors);
        
        /* Statistics */
        mvprintw(15, 0, "Statistics:");
        mvprintw(16, 0, "  Min: %.1f%%", min);
        mvprintw(17, 0, "  Max: %.1f%%", max);
        mvprintw(18, 0, "  Avg: %.1f%%", avg);
        
        /* History graph (simple) */
        mvprintw(20, 0, "History:");
        int graph_height = 8;
        int graph_width = 40;
        for (int row = 0; row < graph_height; row++) {
            mvprintw(21 + row, 0, "  ");
            int threshold = 100 - (row + 1) * (100 / graph_height);
            for (int col = 0; col < graph_width; col++) {
                int idx = (g_config.history.head + col) % MAX_HISTORY;
                if (idx < g_config.history.count) {
                    float usage = g_config.history.samples[idx].usage_percent;
                    if (usage >= threshold) {
                        mvprintw(21 + row, 2 + col, "*");
                    } else {
                        mvprintw(21 + row, 2 + col, " ");
                    }
                }
            }
        }
        
        /* Refresh */
        refresh();
        
        /* Handle input */
        ch = getch();
        switch (ch) {
            case 'q':
            case 'Q':
                g_config.running = 0;
                break;
            case 'r':
            case 'R':
                history_init(&g_config.history);
                break;
            case 'h':
            case 'H':
                mvprintw(0, 40, "Help: q=quit, r=reset, h=help");
                break;
        }
    }
    
    /* Cleanup ncurses */
    endwin();
}

#else

/**
 * display_live - Fallback if no ncurses
 */
static void display_live(void)
{
    print_error("NCURSES not available. Use --verbose for text output.");
}

#endif

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char *argv[])
{
    int opt;
    int ret;
    
    static struct option long_options[] = {
        {"daemon", no_argument, 0, 'd'},
        {"live", no_argument, 0, 'l'},
        {"log", no_argument, 0, 'L'},
        {"interval", required_argument, 0, 'i'},
        {"threshold", required_argument, 0, 't'},
        {"log-file", required_argument, 0, 'f'},
        {"pid-file", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    printf("\n");
    printf("%s╔══════════════════════════════════════════════════════╗%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s║     DDR Memory Monitor                             ║%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s╚══════════════════════════════════════════════════════╝%s\n",
           COLOR_BLUE, COLOR_RESET);
    
    /* Parse arguments */
    while ((opt = getopt_long(argc, argv, "dlLi:t:f:p:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                g_config.daemon = 1;
                g_config.live = 0;
                break;
            case 'l':
                g_config.live = 1;
                g_config.daemon = 0;
                break;
            case 'L':
                g_config.log_enabled = 1;
                break;
            case 'i':
                g_config.interval = atof(optarg);
                if (g_config.interval <= 0) {
                    g_config.interval = DEFAULT_INTERVAL;
                }
                break;
            case 't':
                g_config.threshold = atof(optarg);
                if (g_config.threshold <= 0 || g_config.threshold > 100) {
                    g_config.threshold = DEFAULT_THRESHOLD;
                }
                break;
            case 'f':
                strncpy(g_config.log_file, optarg, sizeof(g_config.log_file) - 1);
                break;
            case 'p':
                strncpy(g_config.pid_file, optarg, sizeof(g_config.pid_file) - 1);
                break;
            case 'v':
                g_config.verbose = 1;
                break;
            case 'h':
                printf("\nDDR Memory Monitor\n\n");
                printf("Usage: %s [options]\n\n", argv[0]);
                printf("Options:\n");
                printf("  -d, --daemon    Run as daemon\n");
                printf("  -l, --live      Live display (ncurses)\n");
                printf("  -L, --log       Enable logging\n");
                printf("  -i, --interval  Sampling interval (seconds)\n");
                printf("  -t, --threshold Memory usage threshold (%%)\n");
                printf("  -f, --log-file  Log file path\n");
                printf("  -p, --pid-file  PID file path\n");
                printf("  -v, --verbose   Verbose output\n");
                printf("  -h, --help      Show this help\n\n");
                printf("Examples:\n");
                printf("  %s --live\n", argv[0]);
                printf("  %s --daemon --log --threshold 90\n", argv[0]);
                printf("  %s --verbose --interval 5\n", argv[0]);
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
    
    /* Initialize history */
    history_init(&g_config.history);
    
    /* Handle daemon mode */
    if (g_config.daemon) {
        g_config.verbose = 0;
        monitor_daemonize();
    }
    
    /* Start monitor */
    ret = monitor_start();
    if (ret < 0) {
        print_error("Failed to start monitor");
        ddr_cleanup();
        return 1;
    }
    
    /* Live display */
    if (g_config.live && !g_config.daemon) {
        #ifdef HAVE_NCURSES
        display_live();
        #else
        print_warning("NCURSES not available, using text mode");
        while (g_config.running) {
            sleep(1);
        }
        #endif
    } else if (!g_config.daemon) {
        /* Text mode - wait for user input */
        print_info("Monitor running (Press Ctrl+C to stop)");
        while (g_config.running) {
            sleep(1);
        }
    }
    
    /* Stop monitor */
    monitor_stop();
    
    /* Cleanup */
    ddr_cleanup();
    
    print_success("Monitor stopped successfully");
    
    return 0;
}
