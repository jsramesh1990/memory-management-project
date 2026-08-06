/**
 * ddr_driver.c - DDR Memory Manager Platform Driver for RK3568
 * 
 * This platform driver provides comprehensive DDR memory management
 * for RK3568-based systems. It implements a character device interface
 * for user-space access to DDR configuration and management functions.
 * 
 * Version: 1.0.0
 * Author: Sebastian
 * Date: 2024-08-06
 * 
 * License: GPL v2
 * 
 * Device: /dev/ddr_manager
 * 
 * Compilation:
 *   make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 * 
 * Usage:
 *   sudo insmod ddr_driver.ko
 *   sudo rmmod ddr_driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/types.h>
#include <linux/err.h>

#include "ddr_driver.h"
#include "ddr_ioctl.h"

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DDR Memory Manager Platform Driver for RK3568");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define DRIVER_NAME             "ddr_manager"
#define DRIVER_CLASS            "ddr_class"
#define DRIVER_VERSION          "1.0.0"

#define DDR_MAX_DEVICES         1
#define DDR_MINOR_BASE          0
#define DDR_DEVICE_COUNT        1

#define DDR_REG_SIZE            0x10000
#define DDR_TIMEOUT_MS          1000

/* DDR Register Offsets */
#define DDR_REG_VERSION         0x00
#define DDR_REG_MEMORY_SIZE     0x04
#define DDR_REG_FREQUENCY       0x08
#define DDR_REG_VOLTAGE         0x0C
#define DDR_REG_ECC_STATUS      0x10
#define DDR_REG_TIMINGS         0x14
#define DDR_REG_REFRESH         0x18
#define DDR_REG_STATUS          0x1C
#define DDR_REG_ERROR           0x20
#define DDR_REG_CONTROL         0x24
#define DDR_REG_TEMPERATURE     0x28
#define DDR_REG_POWER           0x2C
#define DDR_REG_BANDWIDTH       0x30
#define DDR_REG_LATENCY         0x34
#define DDR_REG_CALIBRATION     0x38
#define DDR_REG_DEBUG           0x3C

/* DDR Status Flags */
#define DDR_STATUS_READY        BIT(0)
#define DDR_STATUS_INIT         BIT(1)
#define DDR_STATUS_ECC          BIT(2)
#define DDR_STATUS_ERROR        BIT(3)
#define DDR_STATUS_CALIBRATED   BIT(4)
#define DDR_STATUS_LOW_POWER    BIT(5)
#define DDR_STATUS_OVER_TEMP    BIT(6)
#define DDR_STATUS_UNDER_VOLT   BIT(7)

/* DDR Error Flags */
#define DDR_ERROR_ECC          BIT(0)
#define DDR_ERROR_TIMEOUT      BIT(1)
#define DDR_ERROR_OVERFLOW     BIT(2)
#define DDR_ERROR_UNDERFLOW    BIT(3)
#define DDR_ERROR_PARITY       BIT(4)
#define DDR_ERROR_BUS          BIT(5)
#define DDR_ERROR_TEMP         BIT(6)
#define DDR_ERROR_VOLT         BIT(7)

/* DDR Control Bits */
#define DDR_CTRL_ENABLE        BIT(0)
#define DDR_CTRL_ECC_ENABLE    BIT(1)
#define DDR_CTRL_CALIBRATE     BIT(2)
#define DDR_CTRL_RESET         BIT(3)
#define DDR_CTRL_LOW_POWER     BIT(4)
#define DDR_CTRL_PERF_MODE     BIT(5)
#define DDR_CTRL_DEBUG_MODE    BIT(6)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct ddr_device - DDR device private data
 */
struct ddr_device {
    struct device *dev;
    struct platform_device *pdev;
    
    /* Hardware resources */
    void __iomem *base;
    struct clk *clk;
    struct regulator *regulator;
    int irq;
    
    /* Character device */
    dev_t dev_num;
    struct cdev cdev;
    struct class *class;
    
    /* Memory management */
    struct mutex lock;
    spinlock_t spinlock;
    struct list_head allocations;
    unsigned long total_memory;
    unsigned long used_memory;
    unsigned long peak_memory;
    
    /* Configuration */
    struct ddr_config config;
    struct ddr_stats stats;
    struct ddr_info info;
    
    /* Power management */
    struct pm_domain_data *pm_data;
    bool suspended;
    bool low_power_mode;
    
    /* Threads */
    struct task_struct *monitor_thread;
    struct task_struct *health_thread;
    bool stop_threads;
    
    /* Workqueues */
    struct workqueue_struct *wq;
    struct delayed_work calibration_work;
    struct delayed_work refresh_work;
    
    /* Timers */
    struct timer_list monitor_timer;
    struct timer_list health_timer;
    
    /* Debug */
    struct dentry *debugfs_root;
    struct proc_dir_entry *proc_entry;
    
    /* Callbacks */
    void (*irq_handler)(struct ddr_device *dev);
    void (*error_handler)(struct ddr_device *dev, unsigned int error);
};

/**
 * struct ddr_allocation - DDR memory allocation tracking
 */
struct ddr_allocation {
    struct list_head list;
    void *virt_addr;
    dma_addr_t phys_addr;
    size_t size;
    unsigned int flags;
    unsigned long timestamp;
    pid_t pid;
    char comm[16];
};

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct ddr_device *ddr_global_dev;
static struct class *ddr_class_global;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int ddr_probe(struct platform_device *pdev);
static int ddr_remove(struct platform_device *pdev);
static void ddr_shutdown(struct platform_device *pdev);
static int ddr_suspend(struct device *dev);
static int ddr_resume(struct device *dev);
static int ddr_pm_suspend(struct device *dev);
static int ddr_pm_resume(struct device *dev);
static int ddr_proc_show(struct seq_file *m, void *v);
static int ddr_proc_open(struct inode *inode, struct file *file);

/* ============================================================================
 * Device Tree Match Table
 * ============================================================================ */

static const struct of_device_id ddr_of_match[] = {
    { .compatible = "rockchip,rk3568-ddr", .data = NULL },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, ddr_of_match);

/* ============================================================================
 * Power Management Operations
 * ============================================================================ */

static const struct dev_pm_ops ddr_pm_ops = {
    .suspend = ddr_pm_suspend,
    .resume = ddr_pm_resume,
    .runtime_suspend = ddr_suspend,
    .runtime_resume = ddr_resume,
};

/* ============================================================================
 * Platform Driver Structure
 * ============================================================================ */

static struct platform_driver ddr_driver = {
    .probe = ddr_probe,
    .remove = ddr_remove,
    .shutdown = ddr_shutdown,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = ddr_of_match,
        .pm = &ddr_pm_ops,
    },
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * ddr_read - Read DDR register
 */
static inline unsigned int ddr_read(struct ddr_device *dev, unsigned int reg)
{
    return readl(dev->base + reg);
}

/**
 * ddr_write - Write DDR register
 */
static inline void ddr_write(struct ddr_device *dev, unsigned int reg,
                             unsigned int value)
{
    writel(value, dev->base + reg);
}

/**
 * ddr_set_bit - Set bit in DDR register
 */
static inline void ddr_set_bit(struct ddr_device *dev, unsigned int reg,
                               unsigned int bit)
{
    unsigned int val = ddr_read(dev, reg);
    ddr_write(dev, reg, val | bit);
}

/**
 * ddr_clear_bit - Clear bit in DDR register
 */
static inline void ddr_clear_bit(struct ddr_device *dev, unsigned int reg,
                                 unsigned int bit)
{
    unsigned int val = ddr_read(dev, reg);
    ddr_write(dev, reg, val & ~bit);
}

/**
 * ddr_is_bit_set - Check if bit is set in DDR register
 */
static inline bool ddr_is_bit_set(struct ddr_device *dev, unsigned int reg,
                                  unsigned int bit)
{
    return !!(ddr_read(dev, reg) & bit);
}

/**
 * ddr_alloc_tracking - Track memory allocation
 */
static int ddr_alloc_tracking(struct ddr_device *dev, void *virt,
                              dma_addr_t phys, size_t size,
                              unsigned int flags)
{
    struct ddr_allocation *alloc;
    
    alloc = kzalloc(sizeof(*alloc), GFP_KERNEL);
    if (!alloc)
        return -ENOMEM;
    
    alloc->virt_addr = virt;
    alloc->phys_addr = phys;
    alloc->size = size;
    alloc->flags = flags;
    alloc->timestamp = jiffies;
    alloc->pid = current->pid;
    strncpy(alloc->comm, current->comm, sizeof(alloc->comm) - 1);
    
    spin_lock(&dev->spinlock);
    list_add_tail(&alloc->list, &dev->allocations);
    spin_unlock(&dev->spinlock);
    
    return 0;
}

/**
 * ddr_free_tracking - Remove memory allocation tracking
 */
static int ddr_free_tracking(struct ddr_device *dev, void *virt)
{
    struct ddr_allocation *alloc, *tmp;
    int found = 0;
    
    spin_lock(&dev->spinlock);
    list_for_each_entry_safe(alloc, tmp, &dev->allocations, list) {
        if (alloc->virt_addr == virt) {
            list_del(&alloc->list);
            kfree(alloc);
            found = 1;
            break;
        }
    }
    spin_unlock(&dev->spinlock);
    
    return found ? 0 : -ENOENT;
}

/* ============================================================================
 * Hardware Initialization
 * ============================================================================ */

/**
 * ddr_hw_init - Initialize DDR hardware
 */
static int ddr_hw_init(struct ddr_device *dev)
{
    unsigned int val;
    int ret;
    
    dev_info(dev->dev, "Initializing DDR hardware...\n");
    
    /* Read hardware version */
    val = ddr_read(dev, DDR_REG_VERSION);
    dev_info(dev->dev, "DDR version: 0x%08x\n", val);
    
    /* Get memory size */
    val = ddr_read(dev, DDR_REG_MEMORY_SIZE);
    dev->total_memory = val;
    dev_info(dev->dev, "Total memory: %lu MB\n",
             dev->total_memory / (1024 * 1024));
    
    /* Get current frequency */
    val = ddr_read(dev, DDR_REG_FREQUENCY);
    dev->config.frequency = val;
    dev_info(dev->dev, "Current frequency: %u MHz\n", val);
    
    /* Get current voltage */
    val = ddr_read(dev, DDR_REG_VOLTAGE);
    dev->config.voltage = val;
    dev_info(dev->dev, "Current voltage: %u mV\n", val);
    
    /* Check ECC status */
    val = ddr_read(dev, DDR_REG_ECC_STATUS);
    if (val & 0x1) {
        dev_info(dev->dev, "ECC enabled\n");
        dev->config.ecc_enabled = 1;
    }
    
    /* Read timings */
    val = ddr_read(dev, DDR_REG_TIMINGS);
    dev->config.tCL = (val >> 24) & 0xFF;
    dev->config.tRCD = (val >> 16) & 0xFF;
    dev->config.tRP = (val >> 8) & 0xFF;
    dev->config.tRAS = val & 0xFF;
    dev_info(dev->dev, "Timings: CL%d-tRCD%d-tRP%d-tRAS%d\n",
             dev->config.tCL, dev->config.tRCD,
             dev->config.tRP, dev->config.tRAS);
    
    /* Read refresh rate */
    val = ddr_read(dev, DDR_REG_REFRESH);
    dev->config.tRFC = val;
    dev_info(dev->dev, "Refresh cycle: %u ns\n", val);
    
    /* Enable DDR */
    ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    mdelay(10);
    
    /* Check status */
    val = ddr_read(dev, DDR_REG_STATUS);
    if (!(val & DDR_STATUS_READY)) {
        dev_err(dev->dev, "DDR not ready\n");
        return -EIO;
    }
    
    /* Calibrate if needed */
    if (!(val & DDR_STATUS_CALIBRATED)) {
        dev_info(dev->dev, "Calibrating DDR...\n");
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_CALIBRATE);
        mdelay(100);
        
        val = ddr_read(dev, DDR_REG_STATUS);
        if (!(val & DDR_STATUS_CALIBRATED)) {
            dev_err(dev->dev, "Calibration failed\n");
            return -EIO;
        }
        dev_info(dev->dev, "Calibration complete\n");
    }
    
    /* Configure ECC */
    if (dev->config.ecc_enabled) {
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ECC_ENABLE);
        dev_info(dev->dev, "ECC enabled\n");
    }
    
    /* Set up interrupts */
    if (dev->irq > 0) {
        ret = request_irq(dev->irq, ddr_irq_handler,
                         IRQF_SHARED, DRIVER_NAME, dev);
        if (ret) {
            dev_err(dev->dev, "Failed to request IRQ %d\n", dev->irq);
            return ret;
        }
        dev_info(dev->dev, "IRQ %d registered\n", dev->irq);
    }
    
    dev_info(dev->dev, "DDR hardware initialized successfully\n");
    return 0;
}

/**
 * ddr_hw_config - Configure DDR hardware
 */
static int ddr_hw_config(struct ddr_device *dev, struct ddr_config *config)
{
    unsigned int val;
    int ret = 0;
    
    dev_info(dev->dev, "Configuring DDR hardware...\n");
    
    mutex_lock(&dev->lock);
    
    /* Disable DDR during configuration */
    ddr_clear_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    mdelay(5);
    
    /* Set frequency */
    if (config->frequency != dev->config.frequency) {
        ddr_write(dev, DDR_REG_FREQUENCY, config->frequency);
        mdelay(10);
        val = ddr_read(dev, DDR_REG_FREQUENCY);
        if (val != config->frequency) {
            dev_err(dev->dev, "Failed to set frequency\n");
            ret = -EIO;
            goto out;
        }
        dev->config.frequency = config->frequency;
    }
    
    /* Set voltage */
    if (config->voltage != dev->config.voltage) {
        ddr_write(dev, DDR_REG_VOLTAGE, config->voltage);
        mdelay(10);
        val = ddr_read(dev, DDR_REG_VOLTAGE);
        if (val != config->voltage) {
            dev_err(dev->dev, "Failed to set voltage\n");
            ret = -EIO;
            goto out;
        }
        dev->config.voltage = config->voltage;
    }
    
    /* Set timings */
    val = (config->tCL << 24) | (config->tRCD << 16) |
          (config->tRP << 8) | config->tRAS;
    ddr_write(dev, DDR_REG_TIMINGS, val);
    mdelay(5);
    
    dev->config.tCL = config->tCL;
    dev->config.tRCD = config->tRCD;
    dev->config.tRP = config->tRP;
    dev->config.tRAS = config->tRAS;
    
    /* Set refresh */
    ddr_write(dev, DDR_REG_REFRESH, config->tRFC);
    dev->config.tRFC = config->tRFC;
    
    /* Re-enable DDR */
    ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    mdelay(10);
    
    /* Re-calibrate */
    ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_CALIBRATE);
    mdelay(100);
    
    val = ddr_read(dev, DDR_REG_STATUS);
    if (!(val & DDR_STATUS_CALIBRATED)) {
        dev_err(dev->dev, "Re-calibration failed\n");
        ret = -EIO;
        goto out;
    }
    
    dev_info(dev->dev, "DDR configuration complete\n");
    
out:
    mutex_unlock(&dev->lock);
    return ret;
}

/**
 * ddr_hw_reset - Reset DDR hardware
 */
static int ddr_hw_reset(struct ddr_device *dev)
{
    dev_info(dev->dev, "Resetting DDR hardware...\n");
    
    ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_RESET);
    mdelay(50);
    ddr_clear_bit(dev, DDR_REG_CONTROL, DDR_CTRL_RESET);
    mdelay(10);
    
    return ddr_hw_init(dev);
}

/* ============================================================================
 * Memory Management Functions
 * ============================================================================ */

/**
 * ddr_alloc - Allocate DDR memory
 */
static void *ddr_alloc(struct ddr_device *dev, size_t size,
                       dma_addr_t *dma_handle, gfp_t flags)
{
    void *ptr;
    dma_addr_t phys;
    struct page *page;
    unsigned int order;
    int ret;
    
    if (size == 0 || size > DDR_MAX_ALLOC_SIZE) {
        dev_err(dev->dev, "Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    /* Check if enough memory available */
    if (dev->used_memory + size > dev->total_memory) {
        dev_err(dev->dev, "Insufficient memory: %lu + %zu > %lu\n",
                dev->used_memory, size, dev->total_memory);
        return NULL;
    }
    
    /* Allocate contiguous memory */
    order = get_order(size);
    if (order >= MAX_ORDER) {
        dev_err(dev->dev, "Order too large: %u\n", order);
        return NULL;
    }
    
    page = alloc_pages(flags | __GFP_ZERO, order);
    if (!page) {
        dev_err(dev->dev, "Failed to allocate pages\n");
        return NULL;
    }
    
    ptr = page_address(page);
    phys = page_to_phys(page);
    
    /* Track allocation */
    ret = ddr_alloc_tracking(dev, ptr, phys, size, flags);
    if (ret) {
        __free_pages(page, order);
        return NULL;
    }
    
    /* Update statistics */
    mutex_lock(&dev->lock);
    dev->used_memory += size;
    if (dev->used_memory > dev->peak_memory) {
        dev->peak_memory = dev->used_memory;
    }
    dev->stats.total_allocations++;
    dev->stats.current_allocations++;
    dev->stats.total_allocated += size;
    dev->stats.current_allocated += size;
    mutex_unlock(&dev->lock);
    
    if (dma_handle)
        *dma_handle = phys;
    
    dev_dbg(dev->dev, "Allocated %zu bytes at 0x%08lx\n",
            size, (unsigned long)phys);
    
    return ptr;
}

/**
 * ddr_free - Free DDR memory
 */
static void ddr_free(struct ddr_device *dev, void *ptr, size_t size)
{
    struct page *page;
    unsigned int order;
    int ret;
    
    if (!ptr || !size)
        return;
    
    /* Remove tracking */
    ret = ddr_free_tracking(dev, ptr);
    if (ret) {
        dev_warn(dev->dev, "Untracked memory free: %p\n", ptr);
    }
    
    /* Get page and free */
    page = virt_to_page(ptr);
    order = get_order(size);
    
    /* Update statistics */
    mutex_lock(&dev->lock);
    dev->used_memory -= size;
    dev->stats.total_frees++;
    dev->stats.current_allocations--;
    dev->stats.total_freed += size;
    dev->stats.current_allocated -= size;
    mutex_unlock(&dev->lock);
    
    __free_pages(page, order);
    
    dev_dbg(dev->dev, "Freed %zu bytes at %p\n", size, ptr);
}

/**
 * ddr_dma_alloc - Allocate DMA-capable DDR memory
 */
static void *ddr_dma_alloc(struct ddr_device *dev, size_t size,
                           dma_addr_t *dma_handle, gfp_t flags)
{
    void *ptr;
    int ret;
    
    if (size == 0 || size > DDR_MAX_ALLOC_SIZE) {
        dev_err(dev->dev, "Invalid DMA allocation size: %zu\n", size);
        return NULL;
    }
    
    /* Allocate DMA memory */
    ptr = dma_alloc_coherent(dev->dev, size, dma_handle, flags);
    if (!ptr) {
        dev_err(dev->dev, "Failed to allocate DMA memory\n");
        return NULL;
    }
    
    /* Track allocation */
    ret = ddr_alloc_tracking(dev, ptr, *dma_handle, size, flags | DDR_FLAG_DMA);
    if (ret) {
        dma_free_coherent(dev->dev, size, ptr, *dma_handle);
        return NULL;
    }
    
    /* Update statistics */
    mutex_lock(&dev->lock);
    dev->used_memory += size;
    if (dev->used_memory > dev->peak_memory) {
        dev->peak_memory = dev->used_memory;
    }
    dev->stats.total_allocations++;
    dev->stats.current_allocations++;
    dev->stats.total_allocated += size;
    dev->stats.current_allocated += size;
    mutex_unlock(&dev->lock);
    
    dev_dbg(dev->dev, "Allocated DMA %zu bytes at 0x%08lx\n",
            size, (unsigned long)*dma_handle);
    
    return ptr;
}

/**
 * ddr_dma_free - Free DMA-capable DDR memory
 */
static void ddr_dma_free(struct ddr_device *dev, size_t size,
                         void *ptr, dma_addr_t dma_handle)
{
    int ret;
    
    if (!ptr || !size)
        return;
    
    /* Remove tracking */
    ret = ddr_free_tracking(dev, ptr);
    if (ret) {
        dev_warn(dev->dev, "Untracked DMA memory free: %p\n", ptr);
    }
    
    /* Update statistics */
    mutex_lock(&dev->lock);
    dev->used_memory -= size;
    dev->stats.total_frees++;
    dev->stats.current_allocations--;
    dev->stats.total_freed += size;
    dev->stats.current_allocated -= size;
    mutex_unlock(&dev->lock);
    
    /* Free DMA memory */
    dma_free_coherent(dev->dev, size, ptr, dma_handle);
    
    dev_dbg(dev->dev, "Freed DMA %zu bytes\n", size);
}

/* ============================================================================
 * Device File Operations
 * ============================================================================ */

/**
 * ddr_open - Device open callback
 */
static int ddr_open(struct inode *inode, struct file *file)
{
    struct ddr_device *dev = container_of(inode->i_cdev,
                                          struct ddr_device, cdev);
    
    if (!dev) {
        pr_err("DDR device not found\n");
        return -ENODEV;
    }
    
    file->private_data = dev;
    
    if (!mutex_trylock(&dev->lock)) {
        return -EBUSY;
    }
    mutex_unlock(&dev->lock);
    
    dev_dbg(dev->dev, "Device opened by PID %d\n", current->pid);
    return 0;
}

/**
 * ddr_release - Device release callback
 */
static int ddr_release(struct inode *inode, struct file *file)
{
    struct ddr_device *dev = file->private_data;
    
    dev_dbg(dev->dev, "Device released by PID %d\n", current->pid);
    return 0;
}

/**
 * ddr_read - Device read callback
 */
static ssize_t ddr_read(struct file *file, char __user *buf,
                        size_t count, loff_t *offset)
{
    struct ddr_device *dev = file->private_data;
    char *kbuf;
    size_t kbuf_size;
    ssize_t ret;
    unsigned int val;
    struct ddr_info info;
    
    if (count == 0)
        return 0;
    
    /* Get DDR info */
    mutex_lock(&dev->lock);
    info = dev->info;
    mutex_unlock(&dev->lock);
    
    kbuf_size = PAGE_SIZE;
    kbuf = kmalloc(kbuf_size, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    /* Format info */
    ret = snprintf(kbuf, kbuf_size,
                   "=== DDR Memory Manager ===\n"
                   "Version: %s\n"
                   "Total Memory: %lu MB\n"
                   "Used Memory: %lu MB\n"
                   "Free Memory: %lu MB\n"
                   "Memory Usage: %u%%\n"
                   "Peak Memory: %lu MB\n"
                   "\n"
                   "Frequency: %u MHz\n"
                   "Voltage: %u mV\n"
                   "Temperature: %u C\n"
                   "Timings: CL%u-tRCD%u-tRP%u-tRAS%u\n"
                   "ECC Status: %s\n"
                   "\n"
                   "Allocations: %lu\n"
                   "Frees: %lu\n"
                   "Active Allocs: %lu\n"
                   "Errors: %u\n"
                   "Warnings: %u\n"
                   "Fragmentation: %u%%\n",
                   DRIVER_VERSION,
                   info.total_memory / (1024 * 1024),
                   info.used_memory / (1024 * 1024),
                   (info.total_memory - info.used_memory) / (1024 * 1024),
                   (int)((info.used_memory * 100) / info.total_memory),
                   info.peak_memory / (1024 * 1024),
                   info.frequency,
                   info.voltage,
                   info.temperature,
                   info.tCL, info.tRCD, info.tRP, info.tRAS,
                   info.ecc_enabled ? "Enabled" : "Disabled",
                   info.total_allocations,
                   info.total_frees,
                   info.current_allocations,
                   info.errors,
                   info.warnings,
                   info.fragmentation);
    
    /* Copy to user */
    if (*offset >= ret) {
        ret = 0;
        goto out;
    }
    
    if (count > ret - *offset)
        count = ret - *offset;
    
    if (copy_to_user(buf, kbuf + *offset, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    *offset += count;
    ret = count;
    
out:
    kfree(kbuf);
    return ret;
}

/**
 * ddr_write - Device write callback
 */
static ssize_t ddr_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *offset)
{
    struct ddr_device *dev = file->private_data;
    char *kbuf;
    int ret;
    unsigned int cmd;
    
    if (count == 0 || count > PAGE_SIZE)
        return -EINVAL;
    
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    kbuf[count] = '\0';
    
    /* Parse command */
    if (strncmp(kbuf, "reset", 5) == 0) {
        ret = ddr_hw_reset(dev);
        if (ret == 0)
            ret = count;
    } else if (strncmp(kbuf, "info", 4) == 0) {
        ret = count;
    } else if (strncmp(kbuf, "stats", 5) == 0) {
        ret = count;
    } else if (strncmp(kbuf, "debug", 5) == 0) {
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_DEBUG_MODE);
        ret = count;
    } else if (strncmp(kbuf, "perf", 4) == 0) {
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_PERF_MODE);
        ret = count;
    } else if (strncmp(kbuf, "lowpower", 8) == 0) {
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_LOW_POWER);
        dev->low_power_mode = true;
        ret = count;
    } else {
        dev_warn(dev->dev, "Unknown command: %s\n", kbuf);
        ret = -EINVAL;
    }
    
out:
    kfree(kbuf);
    return ret;
}

/**
 * ddr_ioctl - Device IOCTL callback
 */
static long ddr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct ddr_device *dev = file->private_data;
    void __user *argp = (void __user *)arg;
    struct ddr_config config;
    struct ddr_stats stats;
    struct ddr_alloc alloc;
    void *ptr;
    dma_addr_t dma_handle;
    int ret = 0;
    
    mutex_lock(&dev->lock);
    
    switch (cmd) {
    case DDR_IOCTL_GET_INFO:
        if (copy_to_user(argp, &dev->info, sizeof(dev->info))) {
            ret = -EFAULT;
            goto out;
        }
        break;
        
    case DDR_IOCTL_GET_STATS:
        stats = dev->stats;
        if (copy_to_user(argp, &stats, sizeof(stats))) {
            ret = -EFAULT;
            goto out;
        }
        break;
        
    case DDR_IOCTL_GET_CONFIG:
        if (copy_to_user(argp, &dev->config, sizeof(dev->config))) {
            ret = -EFAULT;
            goto out;
        }
        break;
        
    case DDR_IOCTL_SET_CONFIG:
        if (copy_from_user(&config, argp, sizeof(config))) {
            ret = -EFAULT;
            goto out;
        }
        ret = ddr_hw_config(dev, &config);
        break;
        
    case DDR_IOCTL_ALLOC:
        if (copy_from_user(&alloc, argp, sizeof(alloc))) {
            ret = -EFAULT;
            goto out;
        }
        
        ptr = ddr_alloc(dev, alloc.size, &dma_handle, GFP_KERNEL);
        if (!ptr) {
            ret = -ENOMEM;
            goto out;
        }
        
        alloc.address = (unsigned long)dma_handle;
        alloc.flags |= DDR_FLAG_ALLOCATED;
        
        if (copy_to_user(argp, &alloc, sizeof(alloc))) {
            ddr_free(dev, ptr, alloc.size);
            ret = -EFAULT;
            goto out;
        }
        break;
        
    case DDR_IOCTL_FREE:
        if (copy_from_user(&alloc, argp, sizeof(alloc))) {
            ret = -EFAULT;
            goto out;
        }
        
        ptr = phys_to_virt((phys_addr_t)alloc.address);
        ddr_free(dev, ptr, alloc.size);
        break;
        
    case DDR_IOCTL_DMA_ALLOC:
        if (copy_from_user(&alloc, argp, sizeof(alloc))) {
            ret = -EFAULT;
            goto out;
        }
        
        ptr = ddr_dma_alloc(dev, alloc.size, &dma_handle, GFP_KERNEL);
        if (!ptr) {
            ret = -ENOMEM;
            goto out;
        }
        
        alloc.address = (unsigned long)dma_handle;
        alloc.flags |= DDR_FLAG_DMA | DDR_FLAG_ALLOCATED;
        
        if (copy_to_user(argp, &alloc, sizeof(alloc))) {
            ddr_dma_free(dev, alloc.size, ptr, dma_handle);
            ret = -EFAULT;
            goto out;
        }
        break;
        
    case DDR_IOCTL_DMA_FREE:
        if (copy_from_user(&alloc, argp, sizeof(alloc))) {
            ret = -EFAULT;
            goto out;
        }
        
        ptr = phys_to_virt((phys_addr_t)alloc.address);
        ddr_dma_free(dev, alloc.size, ptr, (dma_addr_t)alloc.address);
        break;
        
    case DDR_IOCTL_RESET:
        ret = ddr_hw_reset(dev);
        break;
        
    case DDR_IOCTL_CALIBRATE:
        ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_CALIBRATE);
        mdelay(100);
        if (ddr_is_bit_set(dev, DDR_REG_STATUS, DDR_STATUS_CALIBRATED)) {
            ret = 0;
        } else {
            ret = -EIO;
        }
        break;
        
    case DDR_IOCTL_GET_STATUS:
        alloc.size = ddr_read(dev, DDR_REG_STATUS);
        if (copy_to_user(argp, &alloc, sizeof(alloc))) {
            ret = -EFAULT;
            goto out;
        }
        break;
        
    default:
        ret = -ENOTTY;
        break;
    }
    
out:
    mutex_unlock(&dev->lock);
    return ret;
}

/**
 * ddr_mmap - Device mmap callback
 */
static int ddr_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct ddr_device *dev = file->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pfn;
    void *ptr;
    int ret;
    
    if (size == 0 || size > DDR_MAX_ALLOC_SIZE) {
        dev_err(dev->dev, "Invalid mmap size: %lu\n", size);
        return -EINVAL;
    }
    
    /* Allocate DMA memory */
    ptr = ddr_dma_alloc(dev, size, (dma_addr_t *)&pfn, GFP_KERNEL);
    if (!ptr) {
        dev_err(dev->dev, "Failed to allocate DMA memory for mmap\n");
        return -ENOMEM;
    }
    
    /* Convert to PFN */
    pfn = virt_to_phys(ptr) >> PAGE_SHIFT;
    
    /* Set VM flags */
    vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    
    /* Map memory */
    ret = remap_pfn_range(vma, vma->vm_start, pfn + offset,
                         size, vma->vm_page_prot);
    if (ret) {
        dev_err(dev->dev, "Failed to remap memory\n");
        ddr_dma_free(dev, size, ptr, (dma_addr_t)pfn);
        return ret;
    }
    
    /* Store allocation for cleanup */
    ret = ddr_alloc_tracking(dev, ptr, pfn << PAGE_SHIFT, size,
                             DDR_FLAG_DMA | DDR_FLAG_ALLOCATED);
    if (ret) {
        ddr_dma_free(dev, size, ptr, (dma_addr_t)pfn);
        return ret;
    }
    
    dev_dbg(dev->dev, "mmap: %lu bytes at 0x%08lx\n", size, pfn << PAGE_SHIFT);
    return 0;
}

/* ============================================================================
 * File Operations Structure
 * ============================================================================ */

static const struct file_operations ddr_fops = {
    .owner = THIS_MODULE,
    .open = ddr_open,
    .release = ddr_release,
    .read = ddr_read,
    .write = ddr_write,
    .unlocked_ioctl = ddr_ioctl,
    .mmap = ddr_mmap,
    .llseek = default_llseek,
};

/* ============================================================================
 * Proc Filesystem Interface
 * ============================================================================ */

/**
 * ddr_proc_show - Proc file show callback
 */
static int ddr_proc_show(struct seq_file *m, void *v)
{
    struct ddr_device *dev = m->private;
    struct ddr_allocation *alloc;
    struct ddr_region *region;
    int count = 0;
    
    seq_printf(m, "=== DDR Memory Manager ===\n");
    seq_printf(m, "Version: %s\n", DRIVER_VERSION);
    seq_printf(m, "Device: %s\n", DRIVER_NAME);
    seq_printf(m, "\n");
    
    seq_printf(m, "Memory Usage:\n");
    seq_printf(m, "  Total: %lu MB\n",
               dev->total_memory / (1024 * 1024));
    seq_printf(m, "  Used: %lu MB\n",
               dev->used_memory / (1024 * 1024));
    seq_printf(m, "  Free: %lu MB\n",
               (dev->total_memory - dev->used_memory) / (1024 * 1024));
    seq_printf(m, "  Peak: %lu MB\n",
               dev->peak_memory / (1024 * 1024));
    seq_printf(m, "  Usage: %u%%\n",
               (int)((dev->used_memory * 100) / dev->total_memory));
    seq_printf(m, "\n");
    
    seq_printf(m, "Configuration:\n");
    seq_printf(m, "  Frequency: %u MHz\n", dev->config.frequency);
    seq_printf(m, "  Voltage: %u mV\n", dev->config.voltage);
    seq_printf(m, "  Timings: CL%u-tRCD%u-tRP%u-tRAS%u\n",
               dev->config.tCL, dev->config.tRCD,
               dev->config.tRP, dev->config.tRAS);
    seq_printf(m, "  ECC: %s\n",
               dev->config.ecc_enabled ? "Enabled" : "Disabled");
    seq_printf(m, "\n");
    
    seq_printf(m, "Statistics:\n");
    seq_printf(m, "  Allocations: %lu\n", dev->stats.total_allocations);
    seq_printf(m, "  Frees: %lu\n", dev->stats.total_frees);
    seq_printf(m, "  Active: %lu\n", dev->stats.current_allocations);
    seq_printf(m, "  Errors: %u\n", dev->stats.errors);
    seq_printf(m, "  Warnings: %u\n", dev->stats.warnings);
    seq_printf(m, "  Fragmentation: %u%%\n", dev->stats.fragmentation);
    seq_printf(m, "\n");
    
    seq_printf(m, "Active Allocations:\n");
    spin_lock(&dev->spinlock);
    list_for_each_entry(alloc, &dev->allocations, list) {
        seq_printf(m, "  %s (PID %d): 0x%08lx, %zu bytes\n",
                   alloc->comm, alloc->pid,
                   (unsigned long)alloc->phys_addr, alloc->size);
        count++;
        if (count > 50) {
            seq_printf(m, "  ... and %d more\n",
                       list_is_last(&alloc->list, &dev->allocations) ? 0 : 1);
            break;
        }
    }
    spin_unlock(&dev->spinlock);
    seq_printf(m, "\n");
    
    seq_printf(m, "Status: %s\n",
               ddr_read(dev, DDR_REG_STATUS) & DDR_STATUS_READY ? "Ready" : "Not Ready");
    seq_printf(m, "Temperature: %u C\n",
               ddr_read(dev, DDR_REG_TEMPERATURE));
    seq_printf(m, "Power: %u mW\n",
               ddr_read(dev, DDR_REG_POWER));
    
    return 0;
}

/**
 * ddr_proc_open - Proc file open callback
 */
static int ddr_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, ddr_proc_show, PDE_DATA(inode));
}

static const struct file_operations ddr_proc_fops = {
    .owner = THIS_MODULE,
    .open = ddr_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/* ============================================================================
 * Interrupt Handler
 * ============================================================================ */

/**
 * ddr_irq_handler - DDR interrupt handler
 */
static irqreturn_t ddr_irq_handler(int irq, void *dev_id)
{
    struct ddr_device *dev = dev_id;
    unsigned int status, error;
    irqreturn_t ret = IRQ_NONE;
    
    if (!dev)
        return IRQ_NONE;
    
    /* Read status and error registers */
    status = ddr_read(dev, DDR_REG_STATUS);
    error = ddr_read(dev, DDR_REG_ERROR);
    
    if (status & DDR_STATUS_ERROR) {
        dev_err(dev->dev, "Error interrupt: 0x%08x\n", error);
        dev->stats.errors++;
        if (dev->error_handler) {
            dev->error_handler(dev, error);
        }
        ret = IRQ_HANDLED;
    }
    
    if (status & DDR_STATUS_OVER_TEMP) {
        dev_warn(dev->dev, "Over-temperature warning\n");
        ret = IRQ_HANDLED;
    }
    
    if (status & DDR_STATUS_UNDER_VOLT) {
        dev_warn(dev->dev, "Under-voltage warning\n");
        ret = IRQ_HANDLED;
    }
    
    /* Clear interrupts */
    ddr_write(dev, DDR_REG_STATUS, status);
    ddr_write(dev, DDR_REG_ERROR, error);
    
    return ret;
}

/* ============================================================================
 * Monitoring Functions
 * ============================================================================ */

/**
 * ddr_monitor_timer - Monitor timer callback
 */
static void ddr_monitor_timer(struct timer_list *t)
{
    struct ddr_device *dev = from_timer(dev, t, monitor_timer);
    unsigned int status, temp, power;
    unsigned long used;
    
    if (!dev || dev->stop_threads)
        return;
    
    /* Read hardware status */
    status = ddr_read(dev, DDR_REG_STATUS);
    temp = ddr_read(dev, DDR_REG_TEMPERATURE);
    power = ddr_read(dev, DDR_REG_POWER);
    
    /* Update info */
    used = dev->used_memory;
    dev->info.total_memory = dev->total_memory;
    dev->info.used_memory = used;
    dev->info.frequency = dev->config.frequency;
    dev->info.voltage = dev->config.voltage;
    dev->info.temperature = temp;
    dev->info.ecc_enabled = dev->config.ecc_enabled;
    dev->info.total_allocations = dev->stats.total_allocations;
    dev->info.total_frees = dev->stats.total_frees;
    dev->info.current_allocations = dev->stats.current_allocations;
    dev->info.errors = dev->stats.errors;
    dev->info.warnings = dev->stats.warnings;
    
    /* Check for issues */
    if (temp > 85) {
        dev_warn(dev->dev, "High temperature: %u C\n", temp);
        dev->stats.warnings++;
    }
    
    if (power > 5000) {
        dev_warn(dev->dev, "High power: %u mW\n", power);
        dev->stats.warnings++;
    }
    
    if (dev->used_memory > dev->total_memory * 0.9) {
        dev_warn(dev->dev, "Memory usage high: %lu%%\n",
                 (dev->used_memory * 100) / dev->total_memory);
        dev->stats.warnings++;
    }
    
    /* Reschedule timer */
    mod_timer(&dev->monitor_timer, jiffies + msecs_to_jiffies(5000));
}

/**
 * ddr_health_timer - Health check timer callback
 */
static void ddr_health_timer(struct timer_list *t)
{
    struct ddr_device *dev = from_timer(dev, t, health_timer);
    unsigned int status;
    
    if (!dev || dev->stop_threads)
        return;
    
    /* Check hardware health */
    status = ddr_read(dev, DDR_REG_STATUS);
    
    if (!(status & DDR_STATUS_READY)) {
        dev_err(dev->dev, "DDR not ready!\n");
        dev->stats.errors++;
    }
    
    if (!(status & DDR_STATUS_INIT)) {
        dev_err(dev->dev, "DDR not initialized!\n");
        dev->stats.errors++;
    }
    
    /* Reschedule timer */
    mod_timer(&dev->health_timer, jiffies + msecs_to_jiffies(10000));
}

/**
 * ddr_calibration_work - Calibration work handler
 */
static void ddr_calibration_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct ddr_device *dev = container_of(dwork,
                                          struct ddr_device,
                                          calibration_work);
    
    dev_info(dev->dev, "Running calibration...\n");
    
    ddr_set_bit(dev, DDR_REG_CONTROL, DDR_CTRL_CALIBRATE);
    mdelay(100);
    
    if (ddr_is_bit_set(dev, DDR_REG_STATUS, DDR_STATUS_CALIBRATED)) {
        dev_info(dev->dev, "Calibration successful\n");
    } else {
        dev_err(dev->dev, "Calibration failed\n");
        dev->stats.errors++;
    }
    
    /* Reschedule calibration */
    queue_delayed_work(dev->wq, &dev->calibration_work,
                       msecs_to_jiffies(3600000)); /* 1 hour */
}

/**
 * ddr_refresh_work - Refresh work handler
 */
static void ddr_refresh_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct ddr_device *dev = container_of(dwork,
                                          struct ddr_device,
                                          refresh_work);
    
    /* Trigger refresh if needed */
    if (ddr_read(dev, DDR_REG_REFRESH) > 1000) {
        ddr_write(dev, DDR_REG_REFRESH, dev->config.tRFC);
    }
    
    /* Reschedule refresh */
    queue_delayed_work(dev->wq, &dev->refresh_work,
                       msecs_to_jiffies(60000)); /* 1 minute */
}

/* ============================================================================
 * Device Initialization
 * ============================================================================ */

/**
 * ddr_create_device - Create DDR character device
 */
static int ddr_create_device(struct ddr_device *dev)
{
    int ret;
    
    /* Allocate device number */
    ret = alloc_chrdev_region(&dev->dev_num, DDR_MINOR_BASE,
                              DDR_DEVICE_COUNT, DRIVER_NAME);
    if (ret < 0) {
        dev_err(dev->dev, "Failed to allocate device number\n");
        return ret;
    }
    
    /* Create device class */
    dev->class = class_create(THIS_MODULE, DRIVER_CLASS);
    if (IS_ERR(dev->class)) {
        ret = PTR_ERR(dev->class);
        dev_err(dev->dev, "Failed to create device class\n");
        goto out_unregister;
    }
    
    /* Create device */
    dev->dev = device_create(dev->class, NULL, dev->dev_num,
                             NULL, DRIVER_NAME);
    if (IS_ERR(dev->dev)) {
        ret = PTR_ERR(dev->dev);
        dev_err(dev->dev, "Failed to create device\n");
        goto out_destroy_class;
    }
    
    /* Initialize character device */
    cdev_init(&dev->cdev, &ddr_fops);
    dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev->cdev, dev->dev_num, DDR_DEVICE_COUNT);
    if (ret < 0) {
        dev_err(dev->dev, "Failed to add character device\n");
        goto out_destroy_device;
    }
    
    dev_info(dev->dev, "Device created: /dev/%s\n", DRIVER_NAME);
    return 0;

out_destroy_device:
    device_destroy(dev->class, dev->dev_num);
out_destroy_class:
    class_destroy(dev->class);
out_unregister:
    unregister_chrdev_region(dev->dev_num, DDR_DEVICE_COUNT);
    return ret;
}

/**
 * ddr_destroy_device - Destroy DDR character device
 */
static void ddr_destroy_device(struct ddr_device *dev)
{
    cdev_del(&dev->cdev);
    device_destroy(dev->class, dev->dev_num);
    class_destroy(dev->class);
    unregister_chrdev_region(dev->dev_num, DDR_DEVICE_COUNT);
    dev_info(dev->dev, "Device destroyed\n");
}

/**
 * ddr_probe - Platform driver probe
 */
static int ddr_probe(struct platform_device *pdev)
{
    struct resource *res;
    struct ddr_device *dev;
    int ret;
    
    dev_info(&pdev->dev, "Probing DDR driver...\n");
    
    /* Allocate device structure */
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        dev_err(&pdev->dev, "Failed to allocate device memory\n");
        return -ENOMEM;
    }
    
    dev->pdev = pdev;
    dev->dev = &pdev->dev;
    platform_set_drvdata(pdev, dev);
    
    /* Get memory region */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource found\n");
        return -ENXIO;
    }
    
    /* Map memory region */
    dev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->base)) {
        ret = PTR_ERR(dev->base);
        dev_err(&pdev->dev, "Failed to map memory region\n");
        return ret;
    }
    
    /* Get clock */
    dev->clk = devm_clk_get(&pdev->dev, "ddr");
    if (IS_ERR(dev->clk)) {
        dev_warn(&pdev->dev, "Failed to get clock\n");
        dev->clk = NULL;
    }
    
    /* Get regulator */
    dev->regulator = devm_regulator_get(&pdev->dev, "ddr");
    if (IS_ERR(dev->regulator)) {
        dev_warn(&pdev->dev, "Failed to get regulator\n");
        dev->regulator = NULL;
    }
    
    /* Get IRQ */
    dev->irq = platform_get_irq(pdev, 0);
    if (dev->irq < 0) {
        dev_warn(&pdev->dev, "No IRQ found\n");
    }
    
    /* Initialize locks */
    mutex_init(&dev->lock);
    spin_lock_init(&dev->spinlock);
    INIT_LIST_HEAD(&dev->allocations);
    
    /* Initialize workqueue */
    dev->wq = create_singlethread_workqueue("ddr_wq");
    if (!dev->wq) {
        dev_err(&pdev->dev, "Failed to create workqueue\n");
        return -ENOMEM;
    }
    
    /* Initialize timers */
    timer_setup(&dev->monitor_timer, ddr_monitor_timer, 0);
    timer_setup(&dev->health_timer, ddr_health_timer, 0);
    
    /* Initialize work */
    INIT_DELAYED_WORK(&dev->calibration_work, ddr_calibration_work);
    INIT_DELAYED_WORK(&dev->refresh_work, ddr_refresh_work);
    
    /* Initialize hardware */
    ret = ddr_hw_init(dev);
    if (ret < 0) {
        dev_err(&pdev->dev, "Hardware initialization failed\n");
        goto out_destroy_wq;
    }
    
    /* Create character device */
    ret = ddr_create_device(dev);
    if (ret < 0) {
        dev_err(&pdev->dev, "Failed to create device\n");
        goto out_destroy_wq;
    }
    
    /* Create proc entry */
    dev->proc_entry = proc_create_data(DRIVER_NAME, 0444, NULL,
                                       &ddr_proc_fops, dev);
    if (!dev->proc_entry) {
        dev_warn(&pdev->dev, "Failed to create proc entry\n");
    }
    
    /* Start timers */
    mod_timer(&dev->monitor_timer, jiffies + msecs_to_jiffies(1000));
    mod_timer(&dev->health_timer, jiffies + msecs_to_jiffies(5000));
    
    /* Start work */
    queue_delayed_work(dev->wq, &dev->calibration_work,
                       msecs_to_jiffies(60000));
    queue_delayed_work(dev->wq, &dev->refresh_work,
                       msecs_to_jiffies(60000));
    
    /* Set global pointer */
    ddr_global_dev = dev;
    
    dev_info(&pdev->dev, "DDR driver probed successfully\n");
    return 0;

out_destroy_wq:
    destroy_workqueue(dev->wq);
    return ret;
}

/**
 * ddr_remove - Platform driver remove
 */
static int ddr_remove(struct platform_device *pdev)
{
    struct ddr_device *dev = platform_get_drvdata(pdev);
    struct ddr_allocation *alloc, *tmp;
    
    if (!dev)
        return 0;
    
    dev_info(&pdev->dev, "Removing DDR driver...\n");
    
    /* Stop threads */
    dev->stop_threads = true;
    
    /* Delete timers */
    del_timer_sync(&dev->monitor_timer);
    del_timer_sync(&dev->health_timer);
    
    /* Cancel work */
    cancel_delayed_work_sync(&dev->calibration_work);
    cancel_delayed_work_sync(&dev->refresh_work);
    
    /* Destroy workqueue */
    destroy_workqueue(dev->wq);
    
    /* Remove proc entry */
    if (dev->proc_entry) {
        remove_proc_entry(DRIVER_NAME, NULL);
    }
    
    /* Destroy character device */
    ddr_destroy_device(dev);
    
    /* Free allocations */
    list_for_each_entry_safe(alloc, tmp, &dev->allocations, list) {
        list_del(&alloc->list);
        if (alloc->flags & DDR_FLAG_DMA) {
            dma_free_coherent(dev->dev, alloc->size,
                             alloc->virt_addr, alloc->phys_addr);
        } else {
            __free_pages(virt_to_page(alloc->virt_addr),
                        get_order(alloc->size));
        }
        kfree(alloc);
    }
    
    /* Free IRQ */
    if (dev->irq > 0) {
        free_irq(dev->irq, dev);
    }
    
    /* Disable DDR */
    ddr_clear_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    
    ddr_global_dev = NULL;
    
    dev_info(&pdev->dev, "DDR driver removed\n");
    return 0;
}

/**
 * ddr_shutdown - Platform driver shutdown
 */
static void ddr_shutdown(struct platform_device *pdev)
{
    struct ddr_device *dev = platform_get_drvdata(pdev);
    
    dev_info(&pdev->dev, "Shutting down DDR driver...\n");
    
    if (dev) {
        /* Disable DDR */
        ddr_clear_bit(dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    }
}

/* ============================================================================
 * Power Management
 * ============================================================================ */

/**
 * ddr_suspend - Suspend device
 */
static int ddr_suspend(struct device *dev)
{
    struct ddr_device *ddr_dev = dev_get_drvdata(dev);
    
    if (!ddr_dev)
        return 0;
    
    dev_info(dev, "Suspending DDR...\n");
    
    /* Save state */
    ddr_dev->suspended = true;
    
    /* Enter low power mode if enabled */
    if (ddr_dev->low_power_mode) {
        ddr_clear_bit(ddr_dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
        ddr_set_bit(ddr_dev, DDR_REG_CONTROL, DDR_CTRL_LOW_POWER);
    }
    
    return 0;
}

/**
 * ddr_resume - Resume device
 */
static int ddr_resume(struct device *dev)
{
    struct ddr_device *ddr_dev = dev_get_drvdata(dev);
    
    if (!ddr_dev)
        return 0;
    
    dev_info(dev, "Resuming DDR...\n");
    
    /* Restore state */
    ddr_dev->suspended = false;
    
    /* Exit low power mode */
    ddr_clear_bit(ddr_dev, DDR_REG_CONTROL, DDR_CTRL_LOW_POWER);
    ddr_set_bit(ddr_dev, DDR_REG_CONTROL, DDR_CTRL_ENABLE);
    mdelay(10);
    
    /* Re-calibrate */
    ddr_set_bit(ddr_dev, DDR_REG_CONTROL, DDR_CTRL_CALIBRATE);
    mdelay(100);
    
    return 0;
}

/**
 * ddr_pm_suspend - PM suspend callback
 */
static int ddr_pm_suspend(struct device *dev)
{
    int ret;
    
    ret = ddr_suspend(dev);
    if (ret)
        return ret;
    
    return pm_runtime_force_suspend(dev);
}

/**
 * ddr_pm_resume - PM resume callback
 */
static int ddr_pm_resume(struct device *dev)
{
    int ret;
    
    ret = pm_runtime_force_resume(dev);
    if (ret)
        return ret;
    
    return ddr_resume(dev);
}

/* ============================================================================
 * Module Initialization and Exit
 * ============================================================================ */

/**
 * ddr_init - Module initialization
 */
static int __init ddr_init(void)
{
    int ret;
    
    pr_info("DDR Driver loading...\n");
    pr_info("Version: %s\n", DRIVER_VERSION);
    
    ret = platform_driver_register(&ddr_driver);
    if (ret) {
        pr_err("Failed to register platform driver: %d\n", ret);
        return ret;
    }
    
    pr_info("DDR Driver loaded successfully\n");
    return 0;
}

/**
 * ddr_exit - Module exit
 */
static void __exit ddr_exit(void)
{
    pr_info("DDR Driver unloading...\n");
    
    platform_driver_unregister(&ddr_driver);
    
    pr_info("DDR Driver unloaded\n");
}

module_init(ddr_init);
module_exit(ddr_exit);
