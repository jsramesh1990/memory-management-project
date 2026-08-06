/**
 * dma_allocator.c - DMA Memory Allocator for RK3568
 * 
 * This module provides DMA-capable memory allocation for the RK3568
 * DDR memory manager. It handles contiguous memory allocation for
 * DMA operations with proper cache coherency.
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
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/genalloc.h>
#include <linux/bitmap.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/cache.h>
#include <linux/dma-attrs.h>

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("DMA Memory Allocator for RK3568");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define DMA_ALLOCATOR_NAME      "dma_allocator"
#define DMA_ALLOCATOR_VERSION   "1.0.0"

#define DMA_POOL_SIZE_DEFAULT   (256 * 1024 * 1024)  /* 256 MB */
#define DMA_POOL_SIZE_MAX       (1024 * 1024 * 1024) /* 1 GB */
#define DMA_POOL_SIZE_MIN       (4 * 1024 * 1024)    /* 4 MB */

#define DMA_ALIGNMENT           4096                  /* 4 KB */
#define DMA_MAX_ALLOC_SIZE      (32 * 1024 * 1024)   /* 32 MB */
#define DMA_MIN_ALLOC_SIZE      64

#define DMA_CACHE_LINE_SIZE     64

/* DMA Allocation Flags */
#define DMA_FLAG_NONE           0x0000
#define DMA_FLAG_CACHED         0x0001
#define DMA_FLAG_WRITE_COMBINE  0x0002
#define DMA_FLAG_READ_ONLY      0x0004
#define DMA_FLAG_WRITE_ONLY     0x0008
#define DMA_FLAG_SECURE         0x0010
#define DMA_FLAG_CONTIGUOUS     0x0020
#define DMA_FLAG_ZERO           0x0040
#define DMA_FLAG_FIXED          0x0080
#define DMA_FLAG_ALLOCATED      0x8000

/* DMA Status */
#define DMA_STATUS_OK           0x0000
#define DMA_STATUS_ERROR        0x0001
#define DMA_STATUS_BUSY         0x0002
#define DMA_STATUS_FULL         0x0004
#define DMA_STATUS_FRAGMENTED   0x0008
#define DMA_STATUS_LOW_MEM      0x0010

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct dma_block - DMA memory block descriptor
 */
struct dma_block {
    struct list_head list;
    unsigned long phys_addr;
    void *virt_addr;
    size_t size;
    unsigned int flags;
    unsigned int order;
    unsigned int ref_count;
    unsigned long timestamp;
    pid_t pid;
    char comm[16];
    dma_addr_t dma_handle;
    struct page *pages;
    int page_count;
};

/**
 * struct dma_pool - DMA memory pool descriptor
 */
struct dma_pool {
    struct list_head list;
    char name[32];
    phys_addr_t phys_start;
    phys_addr_t phys_end;
    size_t size;
    void *virt_start;
    unsigned int flags;
    unsigned int total_blocks;
    unsigned int free_blocks;
    unsigned int used_blocks;
    unsigned long total_allocated;
    unsigned long total_freed;
    unsigned long current_allocated;
    unsigned long peak_allocated;
    spinlock_t lock;
    struct mutex mutex;
    struct gen_pool *gen_pool;
    struct device *dev;
    struct dma_block *blocks;
    unsigned int block_count;
    struct list_head free_list;
    struct list_head used_list;
    struct timer_list defrag_timer;
    struct work_struct defrag_work;
    struct task_struct *monitor_thread;
    bool stop_monitor;
    struct dentry *debugfs_dir;
};

/**
 * struct dma_allocator - DMA allocator main structure
 */
struct dma_allocator {
    struct device *dev;
    struct platform_device *pdev;
    struct list_head pools;
    unsigned int pool_count;
    unsigned long total_size;
    unsigned long used_size;
    unsigned long peak_size;
    unsigned int total_allocations;
    unsigned int total_frees;
    unsigned int current_allocations;
    unsigned int errors;
    unsigned int warnings;
    struct mutex global_mutex;
    spinlock_t global_lock;
    struct dentry *debugfs_root;
    struct proc_dir_entry *proc_entry;
    struct task_struct *health_thread;
    bool stop_health;
    struct timer_list stats_timer;
    struct list_head blocks;
};

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct dma_allocator *global_allocator;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int dma_allocator_probe(struct platform_device *pdev);
static int dma_allocator_remove(struct platform_device *pdev);
static void dma_stats_timer(struct timer_list *t);
static int dma_health_thread(void *data);
static int dma_pool_defrag(struct dma_pool *pool);

/* ============================================================================
 * Platform Driver
 * ============================================================================ */

static const struct of_device_id dma_allocator_of_match[] = {
    { .compatible = "rockchip,rk3568-dma-allocator", .data = NULL },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, dma_allocator_of_match);

static struct platform_driver dma_allocator_driver = {
    .probe = dma_allocator_probe,
    .remove = dma_allocator_remove,
    .driver = {
        .name = DMA_ALLOCATOR_NAME,
        .of_match_table = dma_allocator_of_match,
    },
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * dma_is_power_of_two - Check if number is power of two
 */
static inline bool dma_is_power_of_two(unsigned long x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

/**
 * dma_align_size - Align size to power of two
 */
static inline size_t dma_align_size(size_t size)
{
    if (size < DMA_MIN_ALLOC_SIZE)
        size = DMA_MIN_ALLOC_SIZE;
    
    if (!dma_is_power_of_two(size)) {
        size = roundup_pow_of_two(size);
    }
    
    return size;
}

/**
 * dma_get_order - Get allocation order
 */
static inline unsigned int dma_get_order(size_t size)
{
    if (size == 0)
        return 0;
    return get_order(size);
}

/**
 * dma_page_count - Get number of pages needed
 */
static inline unsigned int dma_page_count(size_t size)
{
    return (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

/**
 * dma_block_find - Find a DMA block by physical address
 */
static struct dma_block *dma_block_find(struct dma_pool *pool,
                                        unsigned long phys_addr)
{
    struct dma_block *block;
    
    spin_lock(&pool->lock);
    list_for_each_entry(block, &pool->used_list, list) {
        if (block->phys_addr == phys_addr) {
            spin_unlock(&pool->lock);
            return block;
        }
    }
    list_for_each_entry(block, &pool->free_list, list) {
        if (block->phys_addr == phys_addr) {
            spin_unlock(&pool->lock);
            return block;
        }
    }
    spin_unlock(&pool->lock);
    
    return NULL;
}

/**
 * dma_block_find_by_virt - Find a DMA block by virtual address
 */
static struct dma_block *dma_block_find_by_virt(struct dma_pool *pool,
                                                void *virt_addr)
{
    struct dma_block *block;
    
    spin_lock(&pool->lock);
    list_for_each_entry(block, &pool->used_list, list) {
        if (block->virt_addr == virt_addr) {
            spin_unlock(&pool->lock);
            return block;
        }
    }
    spin_unlock(&pool->lock);
    
    return NULL;
}

/**
 * dma_block_merge - Merge adjacent free blocks
 */
static int dma_block_merge(struct dma_pool *pool)
{
    struct dma_block *block, *next, *tmp;
    int merged = 0;
    
    spin_lock(&pool->lock);
    
    list_for_each_entry_safe(block, tmp, &pool->free_list, list) {
        if (!list_is_last(&block->list, &pool->free_list)) {
            next = list_next_entry(block, list);
            
            if (block->phys_addr + block->size == next->phys_addr) {
                /* Merge blocks */
                block->size += next->size;
                list_del(&next->list);
                kfree(next);
                pool->free_blocks--;
                merged++;
            }
        }
    }
    
    spin_unlock(&pool->lock);
    
    return merged;
}

/* ============================================================================
 * DMA Pool Management
 * ============================================================================ */

/**
 * dma_pool_create - Create a DMA memory pool
 */
static struct dma_pool *dma_pool_create(struct dma_allocator *allocator,
                                        const char *name,
                                        phys_addr_t start, size_t size,
                                        unsigned int flags)
{
    struct dma_pool *pool;
    int ret;
    
    pool = kzalloc(sizeof(*pool), GFP_KERNEL);
    if (!pool) {
        pr_err("Failed to allocate pool structure\n");
        return NULL;
    }
    
    strncpy(pool->name, name, sizeof(pool->name) - 1);
    pool->phys_start = start;
    pool->size = size;
    pool->phys_end = start + size;
    pool->flags = flags;
    pool->dev = allocator->dev;
    
    spin_lock_init(&pool->lock);
    mutex_init(&pool->mutex);
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    
    /* Create generic pool */
    pool->gen_pool = gen_pool_create(PAGE_SHIFT, -1);
    if (!pool->gen_pool) {
        pr_err("Failed to create generic pool\n");
        kfree(pool);
        return NULL;
    }
    
    /* Add memory to pool */
    ret = gen_pool_add(pool->gen_pool, start, size, -1);
    if (ret) {
        pr_err("Failed to add memory to pool\n");
        gen_pool_destroy(pool->gen_pool);
        kfree(pool);
        return NULL;
    }
    
    /* Initialize timer */
    timer_setup(&pool->defrag_timer, NULL, 0);
    INIT_WORK(&pool->defrag_work, NULL);
    
    /* Add to allocator */
    mutex_lock(&allocator->global_mutex);
    list_add_tail(&pool->list, &allocator->pools);
    allocator->pool_count++;
    allocator->total_size += size;
    mutex_unlock(&allocator->global_mutex);
    
    pr_info("DMA pool created: %s (0x%08lx - 0x%08lx, %lu MB)\n",
            pool->name, (unsigned long)start, (unsigned long)(start + size),
            size / (1024 * 1024));
    
    return pool;
}

/**
 * dma_pool_destroy - Destroy a DMA memory pool
 */
static void dma_pool_destroy(struct dma_pool *pool)
{
    struct dma_block *block, *tmp;
    
    if (!pool)
        return;
    
    mutex_lock(&pool->mutex);
    
    /* Free all blocks */
    list_for_each_entry_safe(block, tmp, &pool->used_list, list) {
        list_del(&block->list);
        if (block->virt_addr) {
            if (block->flags & DMA_FLAG_CONTIGUOUS) {
                dma_free_coherent(pool->dev, block->size,
                                 block->virt_addr, block->dma_handle);
            } else {
                __free_pages(block->pages, block->order);
            }
        }
        kfree(block);
    }
    
    list_for_each_entry_safe(block, tmp, &pool->free_list, list) {
        list_del(&block->list);
        kfree(block);
    }
    
    /* Destroy generic pool */
    if (pool->gen_pool) {
        gen_pool_destroy(pool->gen_pool);
    }
    
    mutex_unlock(&pool->mutex);
    
    /* Remove from allocator */
    if (global_allocator) {
        mutex_lock(&global_allocator->global_mutex);
        list_del(&pool->list);
        global_allocator->pool_count--;
        global_allocator->total_size -= pool->size;
        mutex_unlock(&global_allocator->global_mutex);
    }
    
    kfree(pool);
    pr_info("DMA pool destroyed\n");
}

/**
 * dma_pool_alloc - Allocate memory from DMA pool
 */
static void *dma_pool_alloc(struct dma_pool *pool, size_t size,
                            dma_addr_t *dma_handle, unsigned int flags)
{
    void *virt_addr;
    unsigned long phys_addr;
    struct page *page;
    struct dma_block *block;
    unsigned int order;
    unsigned int gfp_flags = GFP_KERNEL;
    int ret;
    
    if (!pool || size == 0 || size > DMA_MAX_ALLOC_SIZE) {
        pr_err("Invalid allocation parameters\n");
        return NULL;
    }
    
    /* Align size */
    size = dma_align_size(size);
    order = dma_get_order(size);
    
    /* Allocate block structure */
    block = kzalloc(sizeof(*block), GFP_KERNEL);
    if (!block) {
        pr_err("Failed to allocate block structure\n");
        return NULL;
    }
    
    mutex_lock(&pool->mutex);
    
    if (flags & DMA_FLAG_CONTIGUOUS) {
        /* Allocate contiguous memory */
        virt_addr = dma_alloc_coherent(pool->dev, size, dma_handle,
                                       GFP_KERNEL | __GFP_ZERO);
        if (!virt_addr) {
            pr_err("Failed to allocate contiguous DMA memory\n");
            mutex_unlock(&pool->mutex);
            kfree(block);
            return NULL;
        }
        
        phys_addr = *dma_handle;
        block->pages = NULL;
        block->page_count = 0;
    } else {
        /* Allocate from generic pool */
        phys_addr = gen_pool_alloc(pool->gen_pool, size);
        if (!phys_addr) {
            pr_err("Generic pool allocation failed\n");
            mutex_unlock(&pool->mutex);
            kfree(block);
            return NULL;
        }
        
        /* Map physical address */
        virt_addr = phys_to_virt(phys_addr);
        *dma_handle = phys_addr;
        
        /* Get pages */
        page = phys_to_page(phys_addr);
        block->pages = page;
        block->page_count = dma_page_count(size);
    }
    
    /* Initialize block */
    block->phys_addr = phys_addr;
    block->virt_addr = virt_addr;
    block->size = size;
    block->order = order;
    block->flags = flags | DMA_FLAG_ALLOCATED;
    block->timestamp = jiffies;
    block->pid = current->pid;
    block->dma_handle = *dma_handle;
    strncpy(block->comm, current->comm, sizeof(block->comm) - 1);
    
    /* Add to used list */
    spin_lock(&pool->lock);
    list_add_tail(&block->list, &pool->used_list);
    pool->used_blocks++;
    pool->total_blocks++;
    pool->current_allocated += size;
    if (pool->current_allocated > pool->peak_allocated) {
        pool->peak_allocated = pool->current_allocated;
    }
    spin_unlock(&pool->lock);
    
    mutex_unlock(&pool->mutex);
    
    pr_debug("DMA alloc: %zu bytes at 0x%08lx (virt: %p)\n",
             size, (unsigned long)phys_addr, virt_addr);
    
    return virt_addr;
}

/**
 * dma_pool_free - Free memory from DMA pool
 */
static int dma_pool_free(struct dma_pool *pool, void *virt_addr,
                         dma_addr_t dma_handle, size_t size)
{
    struct dma_block *block;
    int ret = 0;
    
    if (!pool || !virt_addr || size == 0) {
        pr_err("Invalid free parameters\n");
        return -EINVAL;
    }
    
    mutex_lock(&pool->mutex);
    
    /* Find block */
    block = dma_block_find_by_virt(pool, virt_addr);
    if (!block) {
        pr_err("Block not found for virt %p\n", virt_addr);
        mutex_unlock(&pool->mutex);
        return -ENOENT;
    }
    
    /* Remove from used list */
    spin_lock(&pool->lock);
    list_del(&block->list);
    pool->used_blocks--;
    pool->current_allocated -= block->size;
    spin_unlock(&pool->lock);
    
    /* Free memory */
    if (block->flags & DMA_FLAG_CONTIGUOUS) {
        dma_free_coherent(pool->dev, block->size,
                         block->virt_addr, block->dma_handle);
    } else {
        if (block->pages) {
            __free_pages(block->pages, block->order);
        } else {
            gen_pool_free(pool->gen_pool, block->phys_addr, block->size);
        }
    }
    
    /* Add to free list */
    spin_lock(&pool->lock);
    list_add_tail(&block->list, &pool->free_list);
    pool->free_blocks++;
    spin_unlock(&pool->lock);
    
    mutex_unlock(&pool->mutex);
    
    pr_debug("DMA free: %zu bytes at %p\n", size, virt_addr);
    
    return ret;
}

/**
 * dma_pool_defrag - Defragment DMA pool
 */
static int dma_pool_defrag(struct dma_pool *pool)
{
    int merged;
    int iterations = 0;
    unsigned long freed = 0;
    
    if (!pool)
        return -EINVAL;
    
    mutex_lock(&pool->mutex);
    
    do {
        merged = dma_block_merge(pool);
        if (merged > 0) {
            freed += merged * PAGE_SIZE;
            iterations++;
        }
    } while (merged > 0 && iterations < 100);
    
    mutex_unlock(&pool->mutex);
    
    if (freed > 0) {
        pr_info("Pool %s defragmented: freed %lu bytes\n",
                pool->name, freed);
    }
    
    return freed > 0 ? 0 : -ENOSPC;
}

/**
 * dma_pool_stats - Get DMA pool statistics
 */
static void dma_pool_stats(struct dma_pool *pool, struct seq_file *m)
{
    if (!pool || !m)
        return;
    
    seq_printf(m, "Pool: %s\n", pool->name);
    seq_printf(m, "  Size: %lu MB\n", pool->size / (1024 * 1024));
    seq_printf(m, "  Used: %lu MB\n", pool->current_allocated / (1024 * 1024));
    seq_printf(m, "  Free: %lu MB\n",
               (pool->size - pool->current_allocated) / (1024 * 1024));
    seq_printf(m, "  Peak: %lu MB\n", pool->peak_allocated / (1024 * 1024));
    seq_printf(m, "  Blocks: %u/%u\n",
               pool->used_blocks, pool->total_blocks);
    seq_printf(m, "  Free Blocks: %u\n", pool->free_blocks);
    seq_printf(m, "  Fragmentation: %u%%\n",
               pool->total_blocks > 0 ?
               (pool->free_blocks * 100) / pool->total_blocks : 0);
}

/* ============================================================================
 * Main Allocator Functions
 * ============================================================================ */

/**
 * dma_allocator_alloc - Allocate DMA memory
 */
void *dma_allocator_alloc(size_t size, dma_addr_t *dma_handle,
                          unsigned int flags)
{
    struct dma_allocator *allocator = global_allocator;
    struct dma_pool *pool;
    void *ptr = NULL;
    
    if (!allocator) {
        pr_err("Allocator not initialized\n");
        return NULL;
    }
    
    if (size == 0 || size > DMA_MAX_ALLOC_SIZE) {
        pr_err("Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    /* Align size */
    size = dma_align_size(size);
    
    /* Try to allocate from pools */
    mutex_lock(&allocator->global_mutex);
    
    list_for_each_entry(pool, &allocator->pools, list) {
        if (pool->flags & flags) {
            ptr = dma_pool_alloc(pool, size, dma_handle, flags);
            if (ptr) {
                allocator->used_size += size;
                if (allocator->used_size > allocator->peak_size) {
                    allocator->peak_size = allocator->used_size;
                }
                allocator->total_allocations++;
                allocator->current_allocations++;
                break;
            }
        }
    }
    
    mutex_unlock(&allocator->global_mutex);
    
    if (!ptr) {
        pr_err("Failed to allocate %zu bytes\n", size);
        allocator->errors++;
    }
    
    return ptr;
}
EXPORT_SYMBOL_GPL(dma_allocator_alloc);

/**
 * dma_allocator_free - Free DMA memory
 */
int dma_allocator_free(void *virt_addr, dma_addr_t dma_handle, size_t size)
{
    struct dma_allocator *allocator = global_allocator;
    struct dma_pool *pool;
    int ret = -ENOENT;
    
    if (!allocator) {
        pr_err("Allocator not initialized\n");
        return -ENODEV;
    }
    
    if (!virt_addr || size == 0) {
        pr_err("Invalid free parameters\n");
        return -EINVAL;
    }
    
    mutex_lock(&allocator->global_mutex);
    
    list_for_each_entry(pool, &allocator->pools, list) {
        ret = dma_pool_free(pool, virt_addr, dma_handle, size);
        if (ret == 0) {
            allocator->used_size -= size;
            allocator->total_frees++;
            allocator->current_allocations--;
            break;
        }
    }
    
    mutex_unlock(&allocator->global_mutex);
    
    if (ret) {
        pr_err("Failed to free memory at %p\n", virt_addr);
        allocator->errors++;
    }
    
    return ret;
}
EXPORT_SYMBOL_GPL(dma_allocator_free);

/**
 * dma_allocator_get_stats - Get allocator statistics
 */
void dma_allocator_get_stats(struct dma_stats *stats)
{
    struct dma_allocator *allocator = global_allocator;
    
    if (!allocator || !stats)
        return;
    
    mutex_lock(&allocator->global_mutex);
    
    stats->total_allocations = allocator->total_allocations;
    stats->total_frees = allocator->total_frees;
    stats->current_allocations = allocator->current_allocations;
    stats->total_allocated = allocator->total_size;
    stats->current_allocated = allocator->used_size;
    stats->peak_allocated = allocator->peak_size;
    stats->errors = allocator->errors;
    stats->warnings = allocator->warnings;
    
    mutex_unlock(&allocator->global_mutex);
}
EXPORT_SYMBOL_GPL(dma_allocator_get_stats);

/**
 * dma_allocator_defrag - Defragment all pools
 */
int dma_allocator_defrag(void)
{
    struct dma_allocator *allocator = global_allocator;
    struct dma_pool *pool;
    int ret = 0;
    
    if (!allocator)
        return -ENODEV;
    
    mutex_lock(&allocator->global_mutex);
    
    list_for_each_entry(pool, &allocator->pools, list) {
        int r = dma_pool_defrag(pool);
        if (r < 0 && ret == 0) {
            ret = r;
        }
    }
    
    mutex_unlock(&allocator->global_mutex);
    
    return ret;
}
EXPORT_SYMBOL_GPL(dma_allocator_defrag);

/* ============================================================================
 * Health Thread
 * ============================================================================ */

/**
 * dma_health_thread - Health monitoring thread
 */
static int dma_health_thread(void *data)
{
    struct dma_allocator *allocator = data;
    unsigned long last_used = 0;
    unsigned int error_count = 0;
    
    pr_info("DMA health thread started\n");
    
    while (!allocator->stop_health) {
        /* Check memory usage */
        if (allocator->used_size != last_used) {
            last_used = allocator->used_size;
            pr_debug("DMA usage: %lu MB\n",
                     allocator->used_size / (1024 * 1024));
        }
        
        /* Check for errors */
        if (allocator->errors != error_count) {
            error_count = allocator->errors;
            pr_warn("DMA errors: %u\n", error_count);
        }
        
        /* Check pool health */
        if (allocator->used_size > allocator->total_size * 0.9) {
            pr_warn("DMA usage high: %lu%%\n",
                    (allocator->used_size * 100) / allocator->total_size);
            dma_allocator_defrag();
        }
        
        /* Sleep */
        msleep_interruptible(10000);
        
        if (kthread_should_stop())
            break;
    }
    
    pr_info("DMA health thread stopped\n");
    return 0;
}

/**
 * dma_stats_timer - Statistics timer callback
 */
static void dma_stats_timer(struct timer_list *t)
{
    struct dma_allocator *allocator = from_timer(allocator, t, stats_timer);
    
    if (!allocator)
        return;
    
    /* Update statistics */
    if (allocator->used_size > 0) {
        pr_debug("DMA: %lu/%lu MB used (%.1f%%)\n",
                 allocator->used_size / (1024 * 1024),
                 allocator->total_size / (1024 * 1024),
                 (allocator->used_size * 100.0) / allocator->total_size);
    }
    
    /* Reschedule */
    mod_timer(&allocator->stats_timer, jiffies + msecs_to_jiffies(30000));
}

/* ============================================================================
 * DebugFS Interface
 * ============================================================================ */

/**
 * dma_debugfs_show - DebugFS show callback
 */
static int dma_debugfs_show(struct seq_file *m, void *v)
{
    struct dma_allocator *allocator = m->private;
    struct dma_pool *pool;
    
    if (!allocator)
        return 0;
    
    seq_printf(m, "=== DMA Allocator ===\n");
    seq_printf(m, "Version: %s\n", DMA_ALLOCATOR_VERSION);
    seq_printf(m, "Pools: %u\n", allocator->pool_count);
    seq_printf(m, "Total: %lu MB\n", allocator->total_size / (1024 * 1024));
    seq_printf(m, "Used: %lu MB\n", allocator->used_size / (1024 * 1024));
    seq_printf(m, "Peak: %lu MB\n", allocator->peak_size / (1024 * 1024));
    seq_printf(m, "Allocations: %u\n", allocator->total_allocations);
    seq_printf(m, "Frees: %u\n", allocator->total_frees);
    seq_printf(m, "Active: %u\n", allocator->current_allocations);
    seq_printf(m, "Errors: %u\n", allocator->errors);
    seq_printf(m, "Warnings: %u\n", allocator->warnings);
    seq_printf(m, "\n");
    
    seq_printf(m, "Pools:\n");
    seq_printf(m, "------\n");
    list_for_each_entry(pool, &allocator->pools, list) {
        dma_pool_stats(pool, m);
        seq_printf(m, "\n");
    }
    
    return 0;
}

static int dma_debugfs_open(struct inode *inode, struct file *file)
{
    return single_open(file, dma_debugfs_show, inode->i_private);
}

static const struct file_operations dma_debugfs_fops = {
    .owner = THIS_MODULE,
    .open = dma_debugfs_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * dma_debugfs_init - Initialize DebugFS
 */
static void dma_debugfs_init(struct dma_allocator *allocator)
{
    struct dentry *dir;
    
    dir = debugfs_create_dir(DMA_ALLOCATOR_NAME, NULL);
    if (IS_ERR_OR_NULL(dir)) {
        pr_warn("Failed to create debugfs directory\n");
        return;
    }
    
    allocator->debugfs_root = dir;
    
    debugfs_create_file("info", 0444, dir, allocator, &dma_debugfs_fops);
    debugfs_create_u32("allocations", 0444, dir, &allocator->total_allocations);
    debugfs_create_u32("frees", 0444, dir, &allocator->total_frees);
    debugfs_create_u32("errors", 0444, dir, &allocator->errors);
    debugfs_create_u64("used_size", 0444, dir, &allocator->used_size);
    debugfs_create_u64("peak_size", 0444, dir, &allocator->peak_size);
}

/**
 * dma_debugfs_exit - Clean up DebugFS
 */
static void dma_debugfs_exit(struct dma_allocator *allocator)
{
    debugfs_remove_recursive(allocator->debugfs_root);
}

/* ============================================================================
 * Proc Filesystem Interface
 * ============================================================================ */

/**
 * dma_proc_show - Proc file show callback
 */
static int dma_proc_show(struct seq_file *m, void *v)
{
    return dma_debugfs_show(m, v);
}

static int dma_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, dma_proc_show, PDE_DATA(inode));
}

static const struct file_operations dma_proc_fops = {
    .owner = THIS_MODULE,
    .open = dma_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/* ============================================================================
 * Device Initialization
 * ============================================================================ */

/**
 * dma_allocator_probe - Platform driver probe
 */
static int dma_allocator_probe(struct platform_device *pdev)
{
    struct dma_allocator *allocator;
    struct resource *res;
    struct dma_pool *pool;
    int ret;
    
    pr_info("Probing DMA allocator...\n");
    
    /* Allocate allocator structure */
    allocator = devm_kzalloc(&pdev->dev, sizeof(*allocator), GFP_KERNEL);
    if (!allocator) {
        dev_err(&pdev->dev, "Failed to allocate allocator\n");
        return -ENOMEM;
    }
    
    allocator->dev = &pdev->dev;
    allocator->pdev = pdev;
    platform_set_drvdata(pdev, allocator);
    
    mutex_init(&allocator->global_mutex);
    spin_lock_init(&allocator->global_lock);
    INIT_LIST_HEAD(&allocator->pools);
    INIT_LIST_HEAD(&allocator->blocks);
    
    /* Get memory region from device tree */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res) {
        /* Create pool from device tree region */
        pool = dma_pool_create(allocator, "dma_pool",
                               res->start, resource_size(res),
                               DMA_FLAG_CONTIGUOUS);
        if (!pool) {
            dev_err(&pdev->dev, "Failed to create DMA pool\n");
            return -ENOMEM;
        }
    } else {
        /* Create default pool */
        pool = dma_pool_create(allocator, "default_pool",
                               DMA_POOL_SIZE_DEFAULT, DMA_POOL_SIZE_DEFAULT,
                               DMA_FLAG_NONE);
        if (!pool) {
            dev_err(&pdev->dev, "Failed to create default pool\n");
            return -ENOMEM;
        }
    }
    
    /* Initialize timer */
    timer_setup(&allocator->stats_timer, dma_stats_timer, 0);
    mod_timer(&allocator->stats_timer, jiffies + msecs_to_jiffies(1000));
    
    /* Start health thread */
    allocator->stop_health = false;
    allocator->health_thread = kthread_run(dma_health_thread, allocator,
                                           "dma_health");
    if (IS_ERR(allocator->health_thread)) {
        dev_err(&pdev->dev, "Failed to create health thread\n");
        ret = PTR_ERR(allocator->health_thread);
        goto out_destroy_pools;
    }
    
    /* Create proc entry */
    allocator->proc_entry = proc_create_data(DMA_ALLOCATOR_NAME, 0444,
                                             NULL, &dma_proc_fops,
                                             allocator);
    if (!allocator->proc_entry) {
        dev_warn(&pdev->dev, "Failed to create proc entry\n");
    }
    
    /* Initialize debugfs */
    dma_debugfs_init(allocator);
    
    /* Set global pointer */
    global_allocator = allocator;
    
    dev_info(&pdev->dev, "DMA allocator probed successfully\n");
    return 0;

out_destroy_pools:
    list_for_each_entry(pool, &allocator->pools, list) {
        dma_pool_destroy(pool);
    }
    return ret;
}

/**
 * dma_allocator_remove - Platform driver remove
 */
static int dma_allocator_remove(struct platform_device *pdev)
{
    struct dma_allocator *allocator = platform_get_drvdata(pdev);
    struct dma_pool *pool, *tmp;
    
    if (!allocator)
        return 0;
    
    pr_info("Removing DMA allocator...\n");
    
    /* Stop health thread */
    allocator->stop_health = true;
    if (allocator->health_thread) {
        kthread_stop(allocator->health_thread);
    }
    
    /* Delete timer */
    del_timer_sync(&allocator->stats_timer);
    
    /* Remove proc entry */
    if (allocator->proc_entry) {
        remove_proc_entry(DMA_ALLOCATOR_NAME, NULL);
    }
    
    /* Clean up debugfs */
    dma_debugfs_exit(allocator);
    
    /* Destroy pools */
    list_for_each_entry_safe(pool, tmp, &allocator->pools, list) {
        dma_pool_destroy(pool);
    }
    
    global_allocator = NULL;
    
    dev_info(&pdev->dev, "DMA allocator removed\n");
    return 0;
}

/* ============================================================================
 * Module Initialization and Exit
 * ============================================================================ */

/**
 * dma_allocator_init - Module initialization
 */
static int __init dma_allocator_init(void)
{
    int ret;
    
    pr_info("DMA Allocator loading...\n");
    pr_info("Version: %s\n", DMA_ALLOCATOR_VERSION);
    
    ret = platform_driver_register(&dma_allocator_driver);
    if (ret) {
        pr_err("Failed to register platform driver: %d\n", ret);
        return ret;
    }
    
    pr_info("DMA Allocator loaded successfully\n");
    return 0;
}

/**
 * dma_allocator_exit - Module exit
 */
static void __exit dma_allocator_exit(void)
{
    pr_info("DMA Allocator unloading...\n");
    
    platform_driver_unregister(&dma_allocator_driver);
    
    pr_info("DMA Allocator unloaded\n");
}

module_init(dma_allocator_init);
module_exit(dma_allocator_exit);
