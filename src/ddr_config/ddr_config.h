/**
 * ddr_config.h - DDR Configuration Header
 * 
 * This header defines the DDR configuration structures and functions
 * for RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#ifndef _DDR_CONFIG_H_
#define _DDR_CONFIG_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/* ============================================================================
 * DDR Type Definitions
 * ============================================================================ */

/**
 * enum ddr_type - DDR memory types
 */
enum ddr_type {
    DDR_TYPE_UNKNOWN = 0,
    DDR_TYPE_DDR3,
    DDR_TYPE_DDR4,
    DDR_TYPE_LPDDR3,
    DDR_TYPE_LPDDR4,
    DDR_TYPE_LPDDR4X,
    DDR_TYPE_MAX,
};

/**
 * enum ddr_channel - DDR channel configuration
 */
enum ddr_channel {
    DDR_CHANNEL_SINGLE = 1,
    DDR_CHANNEL_DUAL = 2,
    DDR_CHANNEL_QUAD = 4,
};

/**
 * enum ddr_voltage - DDR voltage levels
 */
enum ddr_voltage {
    DDR_VOLTAGE_1_2V = 1200,
    DDR_VOLTAGE_1_1V = 1100,
    DDR_VOLTAGE_1_05V = 1050,
    DDR_VOLTAGE_0_9V = 900,
};

/* ============================================================================
 * DDR Timing Structures
 * ============================================================================ */

/**
 * struct ddr_timings - DDR timing parameters
 * 
 * All timing values are in clock cycles unless specified otherwise
 */
struct ddr_timings {
    /* Core timings */
    unsigned int tCL;      /* CAS Latency */
    unsigned int tRCD;     /* RAS-to-CAS Delay */
    unsigned int tRP;      /* RAS Precharge */
    unsigned int tRAS;     /* Active to Precharge */
    unsigned int tRFC;     /* Refresh Cycle Time (ns) */
    unsigned int tRRD;     /* Row Active to Row Active Delay */
    unsigned int tWTR;     /* Write to Read Delay */
    unsigned int tFAW;     /* Four Active Window */
    
    /* Additional timings */
    unsigned int tWR;      /* Write Recovery Time */
    unsigned int tRTP;     /* Read to Precharge Delay */
    unsigned int tCWL;     /* CAS Write Latency */
    unsigned int tXP;      /* Exit Power Down Delay */
    unsigned int tXPDLL;   /* Exit Power Down with DLL */
    unsigned int tZQ;      /* ZQ Calibration Time */
    unsigned int tMOD;     /* Mode Register Set Time */
    unsigned int tMRD;     /* Mode Register Set Command Delay */
    
    /* Advanced timings */
    unsigned int tCCD;     /* CAS-to-CAS Delay */
    unsigned int tRRD_L;   /* Row Active to Row Active Delay (Long) */
    unsigned int tRAS_MAX; /* Maximum Active to Precharge */
    unsigned int tRAS_MIN; /* Minimum Active to Precharge */
    unsigned int tRC;      /* Active to Active/Refresh Delay */
    unsigned int tREFI;    /* Average Periodic Refresh Interval */
    unsigned int tRFC_MIN; /* Minimum Refresh Cycle Time */
    unsigned int tRFC_MAX; /* Maximum Refresh Cycle Time */
};

/**
 * struct ddr_odt - DDR On-Die Termination settings
 */
struct ddr_odt {
    unsigned int wr_enabled;     /* Write ODT enabled */
    unsigned int rd_enabled;     /* Read ODT enabled */
    unsigned int wr_value;       /* Write ODT value (ohms) */
    unsigned int rd_value;       /* Read ODT value (ohms) */
    unsigned int park_enabled;   /* Park mode enabled */
    unsigned int park_value;     /* Park mode value (ohms) */
    unsigned int dynamic_enabled;/* Dynamic ODT enabled */
};

/**
 * struct ddr_config - Complete DDR configuration
 */
struct ddr_config {
    /* Basic information */
    char name[64];
    enum ddr_type type;
    unsigned int size_mb;
    enum ddr_channel channels;
    unsigned int frequency_mhz;
    enum ddr_voltage voltage_mv;
    
    /* Timing parameters */
    struct ddr_timings timings;
    struct ddr_odt odt;
    
    /* Features */
    unsigned int ecc_enabled:1;
    unsigned int power_save:1;
    unsigned int performance_mode:1;
    unsigned int self_refresh:1;
    unsigned int auto_refresh:1;
    unsigned int bank_interleaving:1;
    unsigned int rank_interleaving:1;
    
    /* Reserved fields */
    unsigned int reserved[16];
};

/* ============================================================================
 * DDR Layout Structures
 * ============================================================================ */

/**
 * struct ddr_region - Memory region descriptor
 */
struct ddr_region {
    const char *name;          /* Region name */
    phys_addr_t start;         /* Start address */
    phys_size_t size;          /* Region size */
    unsigned int flags;        /* Region flags */
    unsigned int type;         /* Region type */
    unsigned int priority;     /* Access priority */
    unsigned int cache_policy; /* Cache policy */
};

/**
 * struct ddr_layout - Complete DDR memory layout
 */
struct ddr_layout {
    struct ddr_region bootloader;   /* Bootloader region */
    struct ddr_region uboot;        /* U-Boot region */
    struct ddr_region kernel;       /* Kernel region */
    struct ddr_region dtb;          /* Device tree region */
    struct ddr_region reserved;     /* Reserved region */
    struct ddr_region npu;          /* NPU memory pool */
    struct ddr_region gpu;          /* GPU memory pool */
    struct ddr_region vpu;          /* VPU/camera buffers */
    struct ddr_region system;       /* System RAM */
    struct ddr_region secure;       /* Secure memory region */
    struct ddr_region dma;          /* DMA buffer region */
    struct ddr_region user;         /* User space region */
    
    /* Additional regions */
    struct ddr_region *extra_regions;
    unsigned int num_extra_regions;
};

/* ============================================================================
 * Board Information Structure
 * ============================================================================ */

/**
 * struct board_info - Board-specific information
 */
struct board_info {
    const char *name;          /* Board name */
    const char *vendor;        /* Vendor name */
    const char *model;         /* Model number */
    const char *revision;      /* Revision number */
    enum ddr_type ddr_type;    /* DDR type */
    unsigned int max_freq;     /* Maximum frequency (MHz) */
    unsigned int min_freq;     /* Minimum frequency (MHz) */
    unsigned int max_voltage;  /* Maximum voltage (mV) */
    unsigned int min_voltage;  /* Minimum voltage (mV) */
    unsigned int max_size;     /* Maximum memory size (MB) */
    unsigned int ecc_support;  /* ECC support flag */
    unsigned int npu_support;  /* NPU support flag */
    unsigned int gpu_support;  /* GPU support flag */
    unsigned int vpu_support;  /* VPU support flag */
};

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * ddr_config_init - Initialize DDR configuration
 * @board_type: Board type identifier
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_init(enum board_type board_type);

/**
 * ddr_config_get - Get current DDR configuration
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_get(struct ddr_config *config);

/**
 * ddr_config_set - Set DDR configuration
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_set(struct ddr_config *config);

/**
 * ddr_config_apply - Apply DDR configuration
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_apply(void);

/**
 * ddr_config_save - Save DDR configuration
 * @path: File path to save configuration
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_save(const char *path);

/**
 * ddr_config_load - Load DDR configuration
 * @path: File path to load configuration from
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_load(const char *path);

/**
 * ddr_config_validate - Validate DDR configuration
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_validate(struct ddr_config *config);

/**
 * ddr_config_get_default - Get default configuration for board
 * @board_type: Board type identifier
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_get_default(enum board_type board_type, struct ddr_config *config);

/**
 * ddr_config_detect - Detect DDR configuration from hardware
 * @config: Pointer to configuration structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_detect(struct ddr_config *config);

/**
 * ddr_config_optimize - Optimize DDR configuration for workload
 * @config: Pointer to configuration structure
 * @workload: Workload type identifier
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_config_optimize(struct ddr_config *config, unsigned int workload);

/* ============================================================================
 * Board Detection Functions
 * ============================================================================ */

/**
 * ddr_board_detect - Detect board type
 * 
 * Return: Board type identifier
 */
enum board_type ddr_board_detect(void);

/**
 * ddr_board_get_info - Get board information
 * @board_type: Board type identifier
 * 
 * Return: Pointer to board information structure
 */
const struct board_info *ddr_board_get_info(enum board_type board_type);

/**
 * ddr_board_get_default_layout - Get default memory layout for board
 * @board_type: Board type identifier
 * @layout: Pointer to layout structure
 * 
 * Return: 0 on success, negative error code on failure
 */
int ddr_board_get_default_layout(enum board_type board_type, struct ddr_layout *layout);

/* ============================================================================
 * IOCTL Definitions for User Space
 * ============================================================================ */

#define DDR_IOCTL_MAGIC         'D'

#define DDR_IOCTL_GET_CONFIG    _IOR(DDR_IOCTL_MAGIC, 1, struct ddr_config)
#define DDR_IOCTL_SET_CONFIG    _IOW(DDR_IOCTL_MAGIC, 2, struct ddr_config)
#define DDR_IOCTL_GET_LAYOUT    _IOR(DDR_IOCTL_MAGIC, 3, struct ddr_layout)
#define DDR_IOCTL_SET_LAYOUT    _IOW(DDR_IOCTL_MAGIC, 4, struct ddr_layout)
#define DDR_IOCTL_GET_INFO      _IOR(DDR_IOCTL_MAGIC, 5, struct board_info)
#define DDR_IOCTL_APPLY         _IO(DDR_IOCTL_MAGIC, 6)
#define DDR_IOCTL_SAVE          _IOW(DDR_IOCTL_MAGIC, 7, char *)
#define DDR_IOCTL_LOAD          _IOW(DDR_IOCTL_MAGIC, 8, char *)
#define DDR_IOCTL_VALIDATE      _IOW(DDR_IOCTL_MAGIC, 9, struct ddr_config)
#define DDR_IOCTL_OPTIMIZE      _IOW(DDR_IOCTL_MAGIC, 10, unsigned int)

#endif /* _DDR_CONFIG_H_ */
