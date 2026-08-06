/**
 * board_memory.c - Board-Specific Memory Configuration
 * 
 * This file implements board-specific memory configurations for
 * RK3568-based systems.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "ddr_config.h"
#include "ddr_layout.h"
#include "memory_table.h"

/* ============================================================================
 * Board Definitions
 * ============================================================================ */

/**
 * struct board_definition - Board definition structure
 */
struct board_definition {
    enum board_type type;
    const char *name;
    const char *vendor;
    const char *model;
    struct ddr_config default_config;
    struct ddr_layout default_layout;
};

/* ============================================================================
 * Default DDR Configurations
 * ============================================================================ */

/* Mixtile Edge 2 - Default Configuration */
static struct ddr_config edge2_default_config = {
    .name = "Mixtile Edge 2",
    .type = DDR_TYPE_LPDDR4,
    .size_mb = 4096,
    .channels = DDR_CHANNEL_DUAL,
    .frequency_mhz = 1800,
    .voltage_mv = DDR_VOLTAGE_1_1V,
    .timings = {
        .tCL = 18,
        .tRCD = 18,
        .tRP = 18,
        .tRAS = 42,
        .tRFC = 350,
        .tRRD = 4,
        .tWTR = 4,
        .tFAW = 16,
        .tWR = 16,
        .tRTP = 8,
        .tCWL = 16,
        .tXP = 6,
        .tXPDLL = 24,
        .tZQ = 512,
        .tMOD = 24,
        .tMRD = 4,
        .tCCD = 4,
        .tRRD_L = 6,
        .tRAS_MAX = 72,
        .tRAS_MIN = 36,
        .tRC = 60,
        .tREFI = 7800,
        .tRFC_MIN = 260,
        .tRFC_MAX = 350,
    },
    .odt = {
        .wr_enabled = 1,
        .rd_enabled = 1,
        .wr_value = 40,
        .rd_value = 40,
        .park_enabled = 1,
        .park_value = 40,
        .dynamic_enabled = 1,
    },
    .ecc_enabled = 0,
    .power_save = 0,
    .performance_mode = 1,
    .self_refresh = 1,
    .auto_refresh = 1,
    .bank_interleaving = 1,
    .rank_interleaving = 1,
};

/* Radxa ROCK 3B - Default Configuration */
static struct ddr_config rock3b_default_config = {
    .name = "Radxa ROCK 3B",
    .type = DDR_TYPE_DDR4,
    .size_mb = 8192,
    .channels = DDR_CHANNEL_DUAL,
    .frequency_mhz = 1600,
    .voltage_mv = DDR_VOLTAGE_1_2V,
    .timings = {
        .tCL = 16,
        .tRCD = 16,
        .tRP = 16,
        .tRAS = 36,
        .tRFC = 350,
        .tRRD = 4,
        .tWTR = 4,
        .tFAW = 16,
        .tWR = 16,
        .tRTP = 8,
        .tCWL = 14,
        .tXP = 6,
        .tXPDLL = 24,
        .tZQ = 512,
        .tMOD = 24,
        .tMRD = 4,
        .tCCD = 4,
        .tRRD_L = 6,
        .tRAS_MAX = 72,
        .tRAS_MIN = 36,
        .tRC = 60,
        .tREFI = 7800,
        .tRFC_MIN = 260,
        .tRFC_MAX = 350,
    },
    .odt = {
        .wr_enabled = 1,
        .rd_enabled = 1,
        .wr_value = 34,
        .rd_value = 34,
        .park_enabled = 1,
        .park_value = 40,
        .dynamic_enabled = 1,
    },
    .ecc_enabled = 0,
    .power_save = 0,
    .performance_mode = 1,
    .self_refresh = 1,
    .auto_refresh = 1,
    .bank_interleaving = 1,
    .rank_interleaving = 1,
};

/* Orange Pi 5 - Default Configuration */
static struct ddr_config orange5_default_config = {
    .name = "Orange Pi 5",
    .type = DDR_TYPE_LPDDR4X,
    .size_mb = 4096,
    .channels = DDR_CHANNEL_DUAL,
    .frequency_mhz = 2133,
    .voltage_mv = DDR_VOLTAGE_1_05V,
    .timings = {
        .tCL = 20,
        .tRCD = 20,
        .tRP = 20,
        .tRAS = 48,
        .tRFC = 350,
        .tRRD = 4,
        .tWTR = 4,
        .tFAW = 16,
        .tWR = 16,
        .tRTP = 8,
        .tCWL = 18,
        .tXP = 6,
        .tXPDLL = 24,
        .tZQ = 512,
        .tMOD = 24,
        .tMRD = 4,
        .tCCD = 4,
        .tRRD_L = 6,
        .tRAS_MAX = 72,
        .tRAS_MIN = 36,
        .tRC = 60,
        .tREFI = 7800,
        .tRFC_MIN = 260,
        .tRFC_MAX = 350,
    },
    .odt = {
        .wr_enabled = 1,
        .rd_enabled = 1,
        .wr_value = 40,
        .rd_value = 40,
        .park_enabled = 1,
        .park_value = 40,
        .dynamic_enabled = 1,
    },
    .ecc_enabled = 0,
    .power_save = 0,
    .performance_mode = 1,
    .self_refresh = 1,
    .auto_refresh = 1,
    .bank_interleaving = 1,
    .rank_interleaving = 1,
};

/* ============================================================================
 * Board Memory Layouts
 * ============================================================================ */

/* Mixtile Edge 2 - Memory Layout */
static struct ddr_layout edge2_layout = {
    .bootloader = {
        .name = "bootloader",
        .start = 0x00000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .uboot = {
        .name = "uboot",
        .start = 0x01000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .kernel = {
        .name = "kernel",
        .start = 0x02000000,
        .size = 0x0E000000,      /* 224 MB */
        .flags = 0,
        .type = 0,
    },
    .dtb = {
        .name = "dtb",
        .start = 0x10000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .reserved = {
        .name = "reserved",
        .start = 0x11000000,
        .size = 0x0F000000,      /* 240 MB */
        .flags = 0,
        .type = 0,
    },
    .npu = {
        .name = "npu",
        .start = 0x20000000,
        .size = 0x08000000,      /* 128 MB */
        .flags = 0,
        .type = 0,
    },
    .gpu = {
        .name = "gpu",
        .start = 0x28000000,
        .size = 0x08000000,      /* 128 MB */
        .flags = 0,
        .type = 0,
    },
    .vpu = {
        .name = "vpu",
        .start = 0x30000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .system = {
        .name = "system",
        .start = 0x40000000,
        .size = 0xC0000000,      /* 3 GB */
        .flags = 0,
        .type = 0,
    },
    .secure = {
        .name = "secure",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .dma = {
        .name = "dma",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .user = {
        .name = "user",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .extra_regions = NULL,
    .num_extra_regions = 0,
};

/* Radxa ROCK 3B - Memory Layout */
static struct ddr_layout rock3b_layout = {
    .bootloader = {
        .name = "bootloader",
        .start = 0x00000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .uboot = {
        .name = "uboot",
        .start = 0x01000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .kernel = {
        .name = "kernel",
        .start = 0x02000000,
        .size = 0x0E000000,      /* 224 MB */
        .flags = 0,
        .type = 0,
    },
    .dtb = {
        .name = "dtb",
        .start = 0x10000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .reserved = {
        .name = "reserved",
        .start = 0x11000000,
        .size = 0x0F000000,      /* 240 MB */
        .flags = 0,
        .type = 0,
    },
    .npu = {
        .name = "npu",
        .start = 0x20000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .gpu = {
        .name = "gpu",
        .start = 0x30000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .vpu = {
        .name = "vpu",
        .start = 0x40000000,
        .size = 0x20000000,      /* 512 MB */
        .flags = 0,
        .type = 0,
    },
    .system = {
        .name = "system",
        .start = 0x60000000,
        .size = 0x1C0000000,     /* 7 GB */
        .flags = 0,
        .type = 0,
    },
    .secure = {
        .name = "secure",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .dma = {
        .name = "dma",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .user = {
        .name = "user",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .extra_regions = NULL,
    .num_extra_regions = 0,
};

/* Orange Pi 5 - Memory Layout */
static struct ddr_layout orange5_layout = {
    .bootloader = {
        .name = "bootloader",
        .start = 0x00000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .uboot = {
        .name = "uboot",
        .start = 0x01000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .kernel = {
        .name = "kernel",
        .start = 0x02000000,
        .size = 0x0E000000,      /* 224 MB */
        .flags = 0,
        .type = 0,
    },
    .dtb = {
        .name = "dtb",
        .start = 0x10000000,
        .size = 0x01000000,      /* 16 MB */
        .flags = 0,
        .type = 0,
    },
    .reserved = {
        .name = "reserved",
        .start = 0x11000000,
        .size = 0x0F000000,      /* 240 MB */
        .flags = 0,
        .type = 0,
    },
    .npu = {
        .name = "npu",
        .start = 0x20000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .gpu = {
        .name = "gpu",
        .start = 0x30000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .vpu = {
        .name = "vpu",
        .start = 0x40000000,
        .size = 0x10000000,      /* 256 MB */
        .flags = 0,
        .type = 0,
    },
    .system = {
        .name = "system",
        .start = 0x50000000,
        .size = 0xB0000000,      /* 2.75 GB */
        .flags = 0,
        .type = 0,
    },
    .secure = {
        .name = "secure",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .dma = {
        .name = "dma",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .user = {
        .name = "user",
        .start = 0,
        .size = 0,
        .flags = 0,
        .type = 0,
    },
    .extra_regions = NULL,
    .num_extra_regions = 0,
};

/* ============================================================================
 * Board Definitions Table
 * ============================================================================ */

static struct board_definition board_definitions[] = {
    {
        .type = BOARD_MIXTILE_EDGE2,
        .name = "Mixtile Edge 2",
        .vendor = "Mixtile",
        .model = "Edge 2",
        .default_config = edge2_default_config,
        .default_layout = edge2_layout,
    },
    {
        .type = BOARD_RADXA_ROCK3B,
        .name = "Radxa ROCK 3B",
        .vendor = "Radxa",
        .model = "ROCK 3B",
        .default_config = rock3b_default_config,
        .default_layout = rock3b_layout,
    },
    {
        .type = BOARD_ORANGE_PI_5,
        .name = "Orange Pi 5",
        .vendor = "Orange Pi",
        .model = "5",
        .default_config = orange5_default_config,
        .default_layout = orange5_layout,
    },
};

/* ============================================================================
 * Board Detection Functions
 * ============================================================================ */

/**
 * board_detect_from_dts - Detect board from Device Tree
 */
static enum board_type board_detect_from_dts(void)
{
    struct device_node *np;
    const char *model;
    
    np = of_find_node_by_path("/");
    if (!np) {
        return BOARD_UNKNOWN;
    }
    
    model = of_get_property(np, "model", NULL);
    of_node_put(np);
    
    if (!model) {
        return BOARD_UNKNOWN;
    }
    
    if (strstr(model, "Edge 2")) {
        return BOARD_MIXTILE_EDGE2;
    } else if (strstr(model, "ROCK 3B")) {
        return BOARD_RADXA_ROCK3B;
    } else if (strstr(model, "Orange Pi 5")) {
        return BOARD_ORANGE_PI_5;
    }
    
    return BOARD_UNKNOWN;
}

/**
 * board_detect_from_sysfs - Detect board from sysfs
 */
static enum board_type board_detect_from_sysfs(void)
{
    char buf[256];
    struct file *f;
    loff_t pos = 0;
    ssize_t ret;
    
    f = filp_open("/sys/devices/soc0/machine", O_RDONLY, 0);
    if (IS_ERR(f)) {
        return BOARD_UNKNOWN;
    }
    
    ret = kernel_read(f, buf, sizeof(buf) - 1, &pos);
    filp_close(f, NULL);
    
    if (ret < 0) {
        return BOARD_UNKNOWN;
    }
    
    buf[ret] = '\0';
    
    if (strstr(buf, "Edge 2")) {
        return BOARD_MIXTILE_EDGE2;
    } else if (strstr(buf, "ROCK 3B")) {
        return BOARD_RADXA_ROCK3B;
    } else if (strstr(buf, "Orange Pi 5")) {
        return BOARD_ORANGE_PI_5;
    }
    
    return BOARD_UNKNOWN;
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

/**
 * ddr_board_detect - Detect board type
 */
enum board_type ddr_board_detect(void)
{
    enum board_type board;
    
    /* Try DTS first */
    board = board_detect_from_dts();
    if (board != BOARD_UNKNOWN) {
        return board;
    }
    
    /* Try sysfs */
    board = board_detect_from_sysfs();
    if (board != BOARD_UNKNOWN) {
        return board;
    }
    
    /* Default to unknown */
    return BOARD_UNKNOWN;
}
EXPORT_SYMBOL_GPL(ddr_board_detect);

/**
 * ddr_board_get_info - Get board information
 */
const struct board_info *ddr_board_get_info(enum board_type board_type)
{
    static struct board_info info;
    struct board_definition *def;
    int i;
    
    for (i = 0; i < ARRAY_SIZE(board_definitions); i++) {
        if (board_definitions[i].type == board_type) {
            def = &board_definitions[i];
            memset(&info, 0, sizeof(info));
            info.name = def->name;
            info.vendor = def->vendor;
            info.model = def->model;
            info.ddr_type = def->default_config.type;
            info.max_freq = def->default_config.frequency_mhz;
            info.min_freq = def->default_config.frequency_mhz / 2;
            info.max_voltage = def->default_config.voltage_mv;
            info.min_voltage = def->default_config.voltage_mv - 100;
            info.max_size = def->default_config.size_mb;
            info.ecc_support = 0;
            info.npu_support = 1;
            info.gpu_support = 1;
            info.vpu_support = 1;
            return &info;
        }
    }
    
    return NULL;
}
EXPORT_SYMBOL_GPL(ddr_board_get_info);

/**
 * ddr_board_get_default_layout - Get default memory layout
 */
int ddr_board_get_default_layout(enum board_type board_type, struct ddr_layout *layout)
{
    struct board_definition *def;
    int i;
    
    if (!layout) {
        return -EINVAL;
    }
    
    for (i = 0; i < ARRAY_SIZE(board_definitions); i++) {
        if (board_definitions[i].type == board_type) {
            def = &board_definitions[i];
            memcpy(layout, &def->default_layout, sizeof(*layout));
            return 0;
        }
    }
    
    return -ENODEV;
}
EXPORT_SYMBOL_GPL(ddr_board_get_default_layout);

/**
 * ddr_board_get_default_config - Get default DDR configuration
 */
static struct ddr_config *ddr_board_get_default_config(enum board_type board_type)
{
    struct board_definition *def;
    int i;
    
    for (i = 0; i < ARRAY_SIZE(board_definitions); i++) {
        if (board_definitions[i].type == board_type) {
            def = &board_definitions[i];
            return &def->default_config;
        }
    }
    
    return NULL;
}

/**
 * ddr_config_get_default - Get default configuration for board
 */
int ddr_config_get_default(enum board_type board_type, struct ddr_config *config)
{
    struct ddr_config *default_config;
    
    if (!config) {
        return -EINVAL;
    }
    
    default_config = ddr_board_get_default_config(board_type);
    if (!default_config) {
        return -ENODEV;
    }
    
    memcpy(config, default_config, sizeof(*config));
    return 0;
}
EXPORT_SYMBOL_GPL(ddr_config_get_default);

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("Board Memory Configuration for RK3568");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");

static int __init board_memory_init(void)
{
    pr_info("Board memory configuration loaded\n");
    return 0;
}

static void __exit board_memory_exit(void)
{
    pr_info("Board memory configuration unloaded\n");
}

module_init(board_memory_init);
module_exit(board_memory_exit);
