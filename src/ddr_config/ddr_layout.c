/**
 * ddr_layout.c - DDR Memory Layout Management
 * 
 * This file implements DDR memory layout management for RK3568.
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/genalloc.h>
#include "ddr_config.h"
#include "ddr_layout.h"

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct ddr_layout *current_layout;
static struct gen_pool *ddr_pool;
static DEFINE_MUTEX(layout_mutex);

/* ============================================================================
 * Layout Validation
 * ============================================================================ */

/**
 * ddr_layout_validate_region - Validate a memory region
 */
static int ddr_layout_validate_region(struct ddr_region *region)
{
    if (!region || !region->name) {
        return -EINVAL;
    }
    
    if (region->size == 0) {
        return -EINVAL;
    }
    
    if (region->start == 0) {
        return -EINVAL;
    }
    
    /* Check for overflow */
    if (region->start + region->size < region->start) {
        return -EOVERFLOW;
    }
    
    return 0;
}

/**
 * ddr_layout_validate - Validate entire memory layout
 */
static int ddr_layout_validate(struct ddr_layout *layout)
{
    int ret;
    
    if (!layout) {
        return -EINVAL;
    }
    
    /* Validate each region */
    ret = ddr_layout_validate_region(&layout->bootloader);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->uboot);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->kernel);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->dtb);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->reserved);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->npu);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->gpu);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->vpu);
    if (ret) return ret;
    
    ret = ddr_layout_validate_region(&layout->system);
    if (ret) return ret;
    
    return 0;
}

/**
 * ddr_layout_check_overlap - Check for overlapping regions
 */
static int ddr_layout_check_overlap(struct ddr_layout *layout)
{
    struct ddr_region *regions[] = {
        &layout->bootloader,
        &layout->uboot,
        &layout->kernel,
        &layout->dtb,
        &layout->reserved,
        &layout->npu,
        &layout->gpu,
        &layout->vpu,
        &layout->system,
        &layout->secure,
        &layout->dma,
        &layout->user,
    };
    
    int i, j;
    phys_addr_t r1_start, r1_end, r2_start, r2_end;
    
    for (i = 0; i < ARRAY_SIZE(regions); i++) {
        if (regions[i]->size == 0) continue;
        
        r1_start = regions[i]->start;
        r1_end = regions[i]->start + regions[i]->size;
        
        for (j = i + 1; j < ARRAY_SIZE(regions); j++) {
            if (regions[j]->size == 0) continue;
            
            r2_start = regions[j]->start;
            r2_end = regions[j]->start + regions[j]->size;
            
            /* Check for overlap */
            if (!(r1_end <= r2_start || r2_end <= r1_start)) {
                pr_err("Region overlap: %s and %s\n",
                       regions[i]->name, regions[j]->name);
                return -EINVAL;
            }
        }
    }
    
    return 0;
}

/* ============================================================================
 * Layout Operations
 * ============================================================================ */

/**
 * ddr_layout_init - Initialize DDR layout
 */
int ddr_layout_init(enum board_type board_type)
{
    int ret;
    struct ddr_layout *layout;
    
    pr_info("DDR Layout: Initializing for board %d\n", board_type);
    
    /* Allocate layout structure */
    layout = kzalloc(sizeof(*layout), GFP_KERNEL);
    if (!layout) {
        pr_err("DDR Layout: Failed to allocate layout\n");
        return -ENOMEM;
    }
    
    /* Get default layout for board */
    ret = ddr_board_get_default_layout(board_type, layout);
    if (ret) {
        pr_err("DDR Layout: Failed to get default layout\n");
        kfree(layout);
        return ret;
    }
    
    /* Validate layout */
    ret = ddr_layout_validate(layout);
    if (ret) {
        pr_err("DDR Layout: Invalid layout\n");
        kfree(layout);
        return ret;
    }
    
    /* Check for overlaps */
    ret = ddr_layout_check_overlap(layout);
    if (ret) {
        pr_err("DDR Layout: Overlapping regions detected\n");
        kfree(layout);
        return ret;
    }
    
    /* Set current layout */
    mutex_lock(&layout_mutex);
    current_layout = layout;
    mutex_unlock(&layout_mutex);
    
    pr_info("DDR Layout: Initialized successfully\n");
    
    /* Print layout info */
    pr_info("  Bootloader: 0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->bootloader.start,
            (unsigned long)(layout->bootloader.start + layout->bootloader.size),
            (unsigned long)layout->bootloader.size / (1024 * 1024));
    pr_info("  U-Boot:     0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->uboot.start,
            (unsigned long)(layout->uboot.start + layout->uboot.size),
            (unsigned long)layout->uboot.size / (1024 * 1024));
    pr_info("  Kernel:     0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->kernel.start,
            (unsigned long)(layout->kernel.start + layout->kernel.size),
            (unsigned long)layout->kernel.size / (1024 * 1024));
    pr_info("  DTB:        0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->dtb.start,
            (unsigned long)(layout->dtb.start + layout->dtb.size),
            (unsigned long)layout->dtb.size / (1024 * 1024));
    pr_info("  NPU:        0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->npu.start,
            (unsigned long)(layout->npu.start + layout->npu.size),
            (unsigned long)layout->npu.size / (1024 * 1024));
    pr_info("  GPU:        0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->gpu.start,
            (unsigned long)(layout->gpu.start + layout->gpu.size),
            (unsigned long)layout->gpu.size / (1024 * 1024));
    pr_info("  VPU:        0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->vpu.start,
            (unsigned long)(layout->vpu.start + layout->vpu.size),
            (unsigned long)layout->vpu.size / (1024 * 1024));
    pr_info("  System:     0x%08lx - 0x%08lx (%lu MB)\n",
            (unsigned long)layout->system.start,
            (unsigned long)(layout->system.start + layout->system.size),
            (unsigned long)layout->system.size / (1024 * 1024));
    
    return 0;
}
EXPORT_SYMBOL_GPL(ddr_layout_init);

/**
 * ddr_layout_get - Get current DDR layout
 */
int ddr_layout_get(struct ddr_layout *layout)
{
    if (!layout) {
        return -EINVAL;
    }
    
    mutex_lock(&layout_mutex);
    if (!current_layout) {
        mutex_unlock(&layout_mutex);
        return -ENODEV;
    }
    
    memcpy(layout, current_layout, sizeof(*layout));
    mutex_unlock(&layout_mutex);
    
    return 0;
}
EXPORT_SYMBOL_GPL(ddr_layout_get);

/**
 * ddr_layout_set - Set DDR layout
 */
int ddr_layout_set(struct ddr_layout *layout)
{
    int ret;
    struct ddr_layout *new_layout;
    
    if (!layout) {
        return -EINVAL;
    }
    
    /* Validate layout */
    ret = ddr_layout_validate(layout);
    if (ret) {
        pr_err("DDR Layout: Invalid layout\n");
        return ret;
    }
    
    /* Check for overlaps */
    ret = ddr_layout_check_overlap(layout);
    if (ret) {
        pr_err("DDR Layout: Overlapping regions detected\n");
        return ret;
    }
    
    /* Allocate new layout */
    new_layout = kmemdup(layout, sizeof(*layout), GFP_KERNEL);
    if (!new_layout) {
        pr_err("DDR Layout: Failed to allocate new layout\n");
        return -ENOMEM;
    }
    
    /* Replace current layout */
    mutex_lock(&layout_mutex);
    if (current_layout) {
        kfree(current_layout);
    }
    current_layout = new_layout;
    mutex_unlock(&layout_mutex);
    
    pr_info("DDR Layout: Updated successfully\n");
    return 0;
}
EXPORT_SYMBOL_GPL(ddr_layout_set);

/**
 * ddr_layout_region_info - Get information about a region
 */
int ddr_layout_region_info(const char *name, phys_addr_t *start, phys_size_t *size)
{
    struct ddr_layout *layout;
    struct ddr_region *region = NULL;
    int ret = -ENOENT;
    
    if (!name) {
        return -EINVAL;
    }
    
    mutex_lock(&layout_mutex);
    layout = current_layout;
    if (!layout) {
        mutex_unlock(&layout_mutex);
        return -ENODEV;
    }
    
    /* Search for region */
    if (strcmp(name, "bootloader") == 0) {
        region = &layout->bootloader;
    } else if (strcmp(name, "uboot") == 0) {
        region = &layout->uboot;
    } else if (strcmp(name, "kernel") == 0) {
        region = &layout->kernel;
    } else if (strcmp(name, "dtb") == 0) {
        region = &layout->dtb;
    } else if (strcmp(name, "reserved") == 0) {
        region = &layout->reserved;
    } else if (strcmp(name, "npu") == 0) {
        region = &layout->npu;
    } else if (strcmp(name, "gpu") == 0) {
        region = &layout->gpu;
    } else if (strcmp(name, "vpu") == 0) {
        region = &layout->vpu;
    } else if (strcmp(name, "system") == 0) {
        region = &layout->system;
    } else if (strcmp(name, "secure") == 0) {
        region = &layout->secure;
    } else if (strcmp(name, "dma") == 0) {
        region = &layout->dma;
    } else if (strcmp(name, "user") == 0) {
        region = &layout->user;
    }
    
    if (region) {
        if (start) *start = region->start;
        if (size) *size = region->size;
        ret = 0;
    }
    
    mutex_unlock(&layout_mutex);
    return ret;
}
EXPORT_SYMBOL_GPL(ddr_layout_region_info);

/**
 * ddr_layout_find_free - Find free memory region
 */
int ddr_layout_find_free(size_t size, phys_addr_t *start, phys_addr_t limit)
{
    struct ddr_layout *layout;
    phys_addr_t current;
    int ret = -ENOSPC;
    
    if (!start || size == 0) {
        return -EINVAL;
    }
    
    mutex_lock(&layout_mutex);
    layout = current_layout;
    if (!layout) {
        mutex_unlock(&layout_mutex);
        return -ENODEV;
    }
    
    /* Start after reserved regions */
    current = layout->system.start;
    
    /* Check if within system region */
    if (current + size > layout->system.start + layout->system.size) {
        mutex_unlock(&layout_mutex);
        return -ENOSPC;
    }
    
    *start = current;
    ret = 0;
    
    mutex_unlock(&layout_mutex);
    return ret;
}
EXPORT_SYMBOL_GPL(ddr_layout_find_free);

/**
 * ddr_layout_print - Print layout information
 */
void ddr_layout_print(void)
{
    struct ddr_layout *layout;
    
    mutex_lock(&layout_mutex);
    layout = current_layout;
    if (!layout) {
        pr_info("DDR Layout: No layout configured\n");
        mutex_unlock(&layout_mutex);
        return;
    }
    
    pr_info("\n=== DDR Memory Layout ===\n");
    pr_info("Region         Start       End         Size\n");
    pr_info("-------         -----       ---         ----\n");
    pr_info("bootloader:     0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->bootloader.start,
            (unsigned long)(layout->bootloader.start + layout->bootloader.size),
            (unsigned long)layout->bootloader.size / (1024 * 1024));
    pr_info("uboot:          0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->uboot.start,
            (unsigned long)(layout->uboot.start + layout->uboot.size),
            (unsigned long)layout->uboot.size / (1024 * 1024));
    pr_info("kernel:         0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->kernel.start,
            (unsigned long)(layout->kernel.start + layout->kernel.size),
            (unsigned long)layout->kernel.size / (1024 * 1024));
    pr_info("dtb:            0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->dtb.start,
            (unsigned long)(layout->dtb.start + layout->dtb.size),
            (unsigned long)layout->dtb.size / (1024 * 1024));
    pr_info("npu:            0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->npu.start,
            (unsigned long)(layout->npu.start + layout->npu.size),
            (unsigned long)layout->npu.size / (1024 * 1024));
    pr_info("gpu:            0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->gpu.start,
            (unsigned long)(layout->gpu.start + layout->gpu.size),
            (unsigned long)layout->gpu.size / (1024 * 1024));
    pr_info("vpu:            0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->vpu.start,
            (unsigned long)(layout->vpu.start + layout->vpu.size),
            (unsigned long)layout->vpu.size / (1024 * 1024));
    pr_info("system:         0x%08lx - 0x%08lx (%6lu MB)\n",
            (unsigned long)layout->system.start,
            (unsigned long)(layout->system.start + layout->system.size),
            (unsigned long)layout->system.size / (1024 * 1024));
    pr_info("=========================\n");
    
    mutex_unlock(&layout_mutex);
}
EXPORT_SYMBOL_GPL(ddr_layout_print);

/**
 * ddr_layout_cleanup - Clean up DDR layout
 */
void ddr_layout_cleanup(void)
{
    mutex_lock(&layout_mutex);
    if (current_layout) {
        kfree(current_layout);
        current_layout = NULL;
    }
    mutex_unlock(&layout_mutex);
    
    if (ddr_pool) {
        gen_pool_destroy(ddr_pool);
        ddr_pool = NULL;
    }
    
    pr_info("DDR Layout: Cleaned up\n");
}
EXPORT_SYMBOL_GPL(ddr_layout_cleanup);

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DDR Memory Layout Management for RK3568");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");

static int __init ddr_layout_module_init(void)
{
    pr_info("DDR Layout module loaded\n");
    return 0;
}

static void __exit ddr_layout_module_exit(void)
{
    ddr_layout_cleanup();
    pr_info("DDR Layout module unloaded\n");
}

module_init(ddr_layout_module_init);
module_exit(ddr_layout_module_exit);
