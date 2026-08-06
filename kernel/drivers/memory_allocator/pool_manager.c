/**
 * pool_manager.c - Memory Pool Manager for RK3568
 * 
 * This module provides memory pool management for the RK3568
 * DDR memory manager. It handles multiple memory pools with
 * different characteristics and allocation strategies.
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
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/cache.h>
#include <linux/genalloc.h>

/* ============================================================================
 * Module Information
 * ============================================================================ */

MODULE_AUTHOR("Sebastian");
MODULE_DESCRIPTION("Memory Pool Manager for RK3568");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define POOL_MANAGER_NAME       "pool_manager"
#define POOL_MANAGER_VERSION    "1.0.0"

#define POOL_NAME_MAX           32
#define POOL_MAX_COUNT          64
#define POOL_DEFAULT_SIZE       (64 * 1024 * 1024)   /* 64 MB */
#define POOL_MIN_SIZE           (4 * 1024 * 1024)    /* 4 MB */
#define POOL_MAX_SIZE           (512 * 1024 * 1024)  /* 512 MB */

#define POOL_ALIGNMENT          4096                  /* 4 KB */

/* Pool Types */
#define POOL_TYPE_GENERAL       0
#define POOL_TYPE_DMA           1
#define POOL_TYPE_NPU           2
#define POOL_TYPE_GPU           3
#define POOL_TYPE_VPU           4
#define POOL_TYPE_SECURE        5
#define POOL_TYPE_CACHED        6
#define POOL_TYPE_UNCACHED      7
#define POOL_TYPE_CONTIGUOUS    8

/* Pool Flags */
#define POOL_FLAG_NONE          0x0000
#define POOL_FLAG_DMA           0x0001
#define POOL_FLAG_CACHED        0x0002
#define POOL_FLAG_SECURE        0x0004
#define POOL_FLAG_CONTIGUOUS    0x0008
#define POOL_FLAG_READ_ONLY     0x0010
#define POOL_FLAG_WRITE_ONLY    0x0020
#define POOL_FLAG_ZERO_INIT     0x0040
#define POOL_FLAG_GROWABLE      0x0080
#define POOL_FLAG_SHRINKABLE    0x0100

/* Pool Status */
#define POOL_STATUS_OK          0x0000
#define POOL_STATUS_FULL        0x0001
#define POOL_STATUS_EMPTY       0x0002
#define POOL_STATUS_FRAGMENTED  0x0004
#define POOL_STATUS_ERROR       0x0008
#define POOL_STATUS_LOW_MEM     0x0010

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * struct pool_stats - Pool statistics
 */
struct pool_stats {
    unsigned long total_size;
    unsigned long used_size;
    unsigned long free_size;
    unsigned long peak_used;
    unsigned long total_allocated;
    unsigned long total_freed;
    unsigned int  allocations;
    unsigned int  frees;
    unsigned int  active_allocations;
    unsigned int  errors;
    unsigned int  warnings;
    unsigned int  fragmentation;
    unsigned long timestamp;
};

/**
 * struct memory_pool - Memory pool descriptor
 */
struct memory_pool {
    struct list_head list;
    char name[POOL_NAME_MAX];
    unsigned int id;
    unsigned int type;
    unsigned int flags;
    unsigned int status;
    
    /* Memory region */
    phys_addr_t phys_start;
    phys_addr_t phys_end;
    size_t size;
    void *virt_start;
    
    /* Allocation */
    void *(*alloc)(struct memory_pool *pool, size_t size, gfp_t flags);
    void (*free)(struct memory_pool *pool, void *ptr, size_t size);
    void *(*alloc_aligned)(struct memory_pool *pool, size_t size,
                          size_t align, gfp_t flags);
    
    /* Management */
    struct gen_pool *gen_pool;
    struct list_head free_list;
    struct list_head used_list;
    spinlock_t lock;
    struct mutex mutex;
    
    /* Statistics */
    struct pool_stats stats;
    struct pool_stats history[64];
    unsigned int history_index;
    
    /* Monitoring */
    struct timer_list monitor_timer;
    struct work_struct shrink_work;
    struct task_struct *monitor_thread;
    bool stop_monitor;
    
    /* Debug */
    struct dentry *debugfs_dir;
};

/**
 * struct pool_manager - Pool manager main structure
 */
struct pool_manager {
    struct device *dev;
    struct list_head pools;
    unsigned int pool_count;
    unsigned int next_id;
    struct mutex global_mutex;
    spinlock_t global_lock;
    
    /* Statistics */
    unsigned long total_size;
    unsigned long used_size;
    unsigned long peak_size;
    unsigned int total_allocations;
    unsigned int total_frees;
    unsigned int active_allocations;
    unsigned int errors;
    
    /* Threading */
    struct task_struct *manager_thread;
    bool stop_manager;
    
    /* Debug */
    struct dentry *debugfs_root;
    struct proc_dir_entry *proc_entry;
    struct timer_list stats_timer;
};

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static struct pool_manager *global_manager;

/* ============================================================================
 * Pool Operations
 * ============================================================================ */

/**
 * pool_alloc_general - General allocation from pool
 */
static void *pool_alloc_general(struct memory_pool *pool, size_t size,
                                gfp_t gfp_flags)
{
    void *ptr;
    unsigned long phys;
    
    if (!pool || size == 0)
        return NULL;
    
    /* Allocate from generic pool */
    phys = gen_pool_alloc(pool->gen_pool, size);
    if (!phys)
        return NULL;
    
    ptr = phys_to_virt(phys);
    
    /* Zero if requested */
    if (pool->flags & POOL_FLAG_ZERO_INIT) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

/**
 * pool_free_general - General free from pool
 */
static void pool_free_general(struct memory_pool *pool, void *ptr, size_t size)
{
    unsigned long phys;
    
    if (!pool || !ptr || size == 0)
        return;
    
    phys = virt_to_phys(ptr);
    gen_pool_free(pool->gen_pool, phys, size);
}

/**
 * pool_alloc_aligned - Aligned allocation from pool
 */
static void *pool_alloc_aligned(struct memory_pool *pool, size_t size,
                                size_t align, gfp_t gfp_flags)
{
    void *ptr;
    unsigned long phys;
    
    if (!pool || size == 0)
        return NULL;
    
    /* Align size and alignment */
    if (align < POOL_ALIGNMENT)
        align = POOL_ALIGNMENT;
    
    size = ALIGN(size, align);
    
    /* Allocate from generic pool with alignment */
    phys = gen_pool_alloc_aligned(pool->gen_pool, size, align);
    if (!phys)
        return NULL;
    
    ptr = phys_to_virt(phys);
    
    /* Zero if requested */
    if (pool->flags & POOL_FLAG_ZERO_INIT) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

/* ============================================================================
 * Pool Management Functions
 * ============================================================================ */

/**
 * pool_create - Create a memory pool
 */
static struct memory_pool *pool_create(struct pool_manager *manager,
                                       const char *name, unsigned int type,
                                       phys_addr_t start, size_t size,
                                       unsigned int flags)
{
    struct memory_pool *pool;
    int ret;
    
    if (!manager) {
        pr_err("Manager not initialized\n");
        return NULL;
    }
    
    if (manager->pool_count >= POOL_MAX_COUNT) {
        pr_err("Maximum pool count reached\n");
        return NULL;
    }
    
    if (size < POOL_MIN_SIZE || size > POOL_MAX_SIZE) {
        pr_err("Invalid pool size: %zu\n", size);
        return NULL;
    }
    
    /* Allocate pool structure */
    pool = kzalloc(sizeof(*pool), GFP_KERNEL);
    if (!pool) {
        pr_err("Failed to allocate pool structure\n");
        return NULL;
    }
    
    strncpy(pool->name, name, sizeof(pool->name) - 1);
    pool->id = manager->next_id++;
    pool->type = type;
    pool->flags = flags;
    pool->status = POOL_STATUS_OK;
    pool->phys_start = start;
    pool->size = size;
    pool->phys_end = start + size;
    
    spin_lock_init(&pool->lock);
    mutex_init(&pool->mutex);
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    
    /* Set operations */
    pool->alloc = pool_alloc_general;
    pool->free = pool_free_general;
    pool->alloc_aligned = pool_alloc_aligned;
    
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
    
    /* Initialize statistics */
    pool->stats.total_size = size;
    pool->stats.free_size = size;
    pool->stats.timestamp = jiffies;
    
    /* Initialize timer */
    timer_setup(&pool->monitor_timer, NULL, 0);
    INIT_WORK(&pool->shrink_work, NULL);
    
    /* Add to manager */
    mutex_lock(&manager->global_mutex);
    list_add_tail(&pool->list, &manager->pools);
    manager->pool_count++;
    manager->total_size += size;
    mutex_unlock(&manager->global_mutex);
    
    pr_info("Pool created: %s (ID: %u, Type: %u, Size: %lu MB)\n",
            pool->name, pool->id, pool->type, size / (1024 * 1024));
    
    return pool;
}

/**
 * pool_destroy - Destroy a memory pool
 */
static int pool_destroy(struct memory_pool *pool)
{
    if (!pool)
        return -EINVAL;
    
    mutex_lock(&pool->mutex);
    
    /* Check for active allocations */
    if (!list_empty(&pool->used_list)) {
        pr_warn("Pool %s has active allocations\n", pool->name);
        mutex_unlock(&pool->mutex);
        return -EBUSY;
    }
    
    /* Destroy generic pool */
    if (pool->gen_pool) {
        gen_pool_destroy(pool->gen_pool);
    }
    
    mutex_unlock(&pool->mutex);
    
    /* Remove from manager */
    if (global_manager) {
        mutex_lock(&global_manager->global_mutex);
        list_del(&pool->list);
        global_manager->pool_count--;
        global_manager->total_size -= pool->size;
        mutex_unlock(&global_manager->global_mutex);
    }
    
    pr_info("Pool destroyed: %s\n", pool->name);
    kfree(pool);
    return 0;
}

/**
 * pool_alloc - Allocate memory from a pool
 */
static void *pool_alloc(struct memory_pool *pool, size_t size, gfp_t flags)
{
    void *ptr;
    unsigned long old_used;
    
    if (!pool || size == 0)
        return NULL;
    
    mutex_lock(&pool->mutex);
    
    old_used = pool->stats.used_size;
    
    ptr = pool->alloc(pool, size, flags);
    if (ptr) {
        /* Update statistics */
        pool->stats.used_size += size;
        pool->stats.free_size -= size;
        pool->stats.allocations++;
        pool->stats.active_allocations++;
        pool->stats.total_allocated += size;
        
        if (pool->stats.used_size > pool->stats.peak_used) {
            pool->stats.peak_used = pool->stats.used_size;
        }
    } else {
        pool->stats.errors++;
    }
    
    mutex_unlock(&pool->mutex);
    
    return ptr;
}

/**
 * pool_free - Free memory from a pool
 */
static int pool_free(struct memory_pool *pool, void *ptr, size_t size)
{
    if (!pool || !ptr || size == 0)
        return -EINVAL;
    
    mutex_lock(&pool->mutex);
    
    pool->free(pool, ptr, size);
    
    /* Update statistics */
    pool->stats.used_size -= size;
    pool->stats.free_size += size;
    pool->stats.frees++;
    pool->stats.active_allocations--;
    pool->stats.total_freed += size;
    
    mutex_unlock(&pool->mutex);
    
    return 0;
}

/**
 * pool_alloc_aligned - Allocate aligned memory from a pool
 */
static void *pool_alloc_aligned_wrapper(struct memory_pool *pool,
                                        size_t size, size_t align,
                                        gfp_t flags)
{
    void *ptr;
    
    if (!pool || size == 0)
        return NULL;
    
    mutex_lock(&pool->mutex);
    
    ptr = pool->alloc_aligned(pool, size, align, flags);
    if (ptr) {
        pool->stats.used_size += size;
        pool->stats.free_size -= size;
        pool->stats.allocations++;
        pool->stats.active_allocations++;
        pool->stats.total_allocated += size;
        
        if (pool->stats.used_size > pool->stats.peak_used) {
            pool->stats.peak_used = pool->stats.used_size;
        }
    } else {
        pool->stats.errors++;
    }
    
    mutex_unlock(&pool->mutex);
    
    return ptr;
}

/**
 * pool_get_stats - Get pool statistics
 */
static void pool_get_stats(struct memory_pool *pool, struct pool_stats *stats)
{
    if (!pool || !stats)
        return;
    
    mutex_lock(&pool->mutex);
    *stats = pool->stats;
    mutex_unlock(&pool->mutex);
}

/**
 * pool_defrag - Defragment a pool
 */
static int pool_defrag(struct memory_pool *pool)
{
    int merged = 0;
    int ret = 0;
    
    if (!pool)
        return -EINVAL;
    
    mutex_lock(&pool->mutex);
    
    /* Merge free blocks */
    if (pool->gen_pool) {
        merged = gen_pool_merge(pool->gen_pool);
        if (merged > 0) {
            pool->stats.fragmentation = 0;
            ret = 0;
        } else {
            ret = -ENOSPC;
        }
    }
    
    mutex_unlock(&pool->mutex);
    
    return ret;
}

/**
 * pool_shrink - Shrink a pool
 */
static int pool_shrink(struct memory_pool *pool, size_t target_size)
{
    size_t shrink_size;
    int ret = 0;
    
    if (!pool)
        return -EINVAL;
    
    mutex_lock(&pool->mutex);
    
    if (pool->stats.used_size > target_size) {
        mutex_unlock(&pool->mutex);
        return -EBUSY;
    }
    
    shrink_size = pool->size - target_size;
    if (shrink_size > 0) {
        /* Remove memory from pool */
        if (pool->gen_pool) {
            gen_pool_remove(pool->gen_pool, pool->phys_end - shrink_size,
                           shrink_size);
        }
        pool->size = target_size;
        pool->stats.total_size = target_size;
        pool->stats.free_size = target_size - pool->stats.used_size;
        ret = 0;
    }
    
    mutex_unlock(&pool->mutex);
    
    return ret;
}

/* ============================================================================
 * Pool Manager Functions
 * ============================================================================ */

/**
 * pool_manager_get_pool - Get pool by ID
 */
static struct memory_pool *pool_manager_get_pool(unsigned int id)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    
    if (!manager)
        return NULL;
    
    mutex_lock(&manager->global_mutex);
    list_for_each_entry(pool, &manager->pools, list) {
        if (pool->id == id) {
            mutex_unlock(&manager->global_mutex);
            return pool;
        }
    }
    mutex_unlock(&manager->global_mutex);
    
    return NULL;
}

/**
 * pool_manager_get_pool_by_name - Get pool by name
 */
static struct memory_pool *pool_manager_get_pool_by_name(const char *name)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    
    if (!manager || !name)
        return NULL;
    
    mutex_lock(&manager->global_mutex);
    list_for_each_entry(pool, &manager->pools, list) {
        if (strcmp(pool->name, name) == 0) {
            mutex_unlock(&manager->global_mutex);
            return pool;
        }
    }
    mutex_unlock(&manager->global_mutex);
    
    return NULL;
}

/**
 * pool_manager_alloc - Allocate from any pool
 */
static void *pool_manager_alloc(size_t size, unsigned int type, gfp_t flags)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    void *ptr = NULL;
    
    if (!manager || size == 0)
        return NULL;
    
    mutex_lock(&manager->global_mutex);
    
    list_for_each_entry(pool, &manager->pools, list) {
        if (pool->type == type) {
            ptr = pool_alloc(pool, size, flags);
            if (ptr) {
                manager->used_size += size;
                if (manager->used_size > manager->peak_size) {
                    manager->peak_size = manager->used_size;
                }
                manager->total_allocations++;
                manager->active_allocations++;
                break;
            }
        }
    }
    
    mutex_unlock(&manager->global_mutex);
    
    return ptr;
}

/**
 * pool_manager_free - Free to any pool
 */
static int pool_manager_free(void *ptr, size_t size)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    unsigned long phys;
    int ret = -ENOENT;
    
    if (!manager || !ptr || size == 0)
        return -EINVAL;
    
    phys = virt_to_phys(ptr);
    
    mutex_lock(&manager->global_mutex);
    
    list_for_each_entry(pool, &manager->pools, list) {
        if (phys >= pool->phys_start && phys < pool->phys_end) {
            ret = pool_free(pool, ptr, size);
            if (ret == 0) {
                manager->used_size -= size;
                manager->total_frees++;
                manager->active_allocations--;
            }
            break;
        }
    }
    
    mutex_unlock(&manager->global_mutex);
    
    return ret;
}

/**
 * pool_manager_get_stats - Get manager statistics
 */
static void pool_manager_get_stats(struct pool_manager_stats *stats)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    struct pool_stats pstats;
    
    if (!manager || !stats)
        return;
    
    mutex_lock(&manager->global_mutex);
    
    stats->total_size = manager->total_size;
    stats->used_size = manager->used_size;
    stats->pool_count = manager->pool_count;
    stats->total_allocations = manager->total_allocations;
    stats->total_frees = manager->total_frees;
    stats->active_allocations = manager->active_allocations;
    stats->errors = manager->errors;
    
    stats->pool_stats = 0;
    list_for_each_entry(pool, &manager->pools, list) {
        pool_get_stats(pool, &pstats);
        stats->pool_stats++;
    }
    
    mutex_unlock(&manager->global_mutex);
}

/* ============================================================================
 * Manager Thread
 * ============================================================================ */

/**
 * pool_manager_thread - Manager monitoring thread
 */
static int pool_manager_thread(void *data)
{
    struct pool_manager *manager = data;
    struct memory_pool *pool;
    struct pool_stats stats;
    unsigned long last_used = 0;
    
    pr_info("Pool manager thread started\n");
    
    while (!manager->stop_manager) {
        /* Check usage */
        if (manager->used_size != last_used) {
            last_used = manager->used_size;
            pr_debug("Pool usage: %lu MB\n",
                     manager->used_size / (1024 * 1024));
        }
        
        /* Check each pool */
        mutex_lock(&manager->global_mutex);
        list_for_each_entry(pool, &manager->pools, list) {
            pool_get_stats(pool, &stats);
            
            /* Check for fragmentation */
            if (stats.fragmentation > 50) {
                pool_defrag(pool);
            }
            
            /* Check for low memory */
            if (stats.free_size < stats.total_size * 0.1) {
                pr_warn("Pool %s low memory: %lu MB free\n",
                        pool->name, stats.free_size / (1024 * 1024));
                manager->errors++;
            }
        }
        mutex_unlock(&manager->global_mutex);
        
        /* Sleep */
        msleep_interruptible(10000);
        
        if (kthread_should_stop())
            break;
    }
    
    pr_info("Pool manager thread stopped\n");
    return 0;
}

/**
 * pool_manager_stats_timer - Statistics timer callback
 */
static void pool_manager_stats_timer(struct timer_list *t)
{
    struct pool_manager *manager = from_timer(manager, t, stats_timer);
    
    if (!manager)
        return;
    
    /* Update statistics */
    if (manager->used_size > 0) {
        pr_debug("Pool Manager: %lu/%lu MB used (%.1f%%)\n",
                 manager->used_size / (1024 * 1024),
                 manager->total_size / (1024 * 1024),
                 (manager->used_size * 100.0) / manager->total_size);
    }
    
    /* Reschedule */
    mod_timer(&manager->stats_timer, jiffies + msecs_to_jiffies(30000));
}

/* ============================================================================
 * DebugFS Interface
 * ============================================================================ */

/**
 * pool_debugfs_show - DebugFS show callback
 */
static int pool_debugfs_show(struct seq_file *m, void *v)
{
    struct pool_manager *manager = m->private;
    struct memory_pool *pool;
    struct pool_stats stats;
    
    if (!manager)
        return 0;
    
    seq_printf(m, "=== Pool Manager ===\n");
    seq_printf(m, "Version: %s\n", POOL_MANAGER_VERSION);
    seq_printf(m, "Pools: %u\n", manager->pool_count);
    seq_printf(m, "Total: %lu MB\n", manager->total_size / (1024 * 1024));
    seq_printf(m, "Used: %lu MB\n", manager->used_size / (1024 * 1024));
    seq_printf(m, "Peak: %lu MB\n", manager->peak_size / (1024 * 1024));
    seq_printf(m, "Allocations: %u\n", manager->total_allocations);
    seq_printf(m, "Frees: %u\n", manager->total_frees);
    seq_printf(m, "Active: %u\n", manager->active_allocations);
    seq_printf(m, "Errors: %u\n", manager->errors);
    seq_printf(m, "\n");
    
    seq_printf(m, "Pools:\n");
    seq_printf(m, "------\n");
    list_for_each_entry(pool, &manager->pools, list) {
        pool_get_stats(pool, &stats);
        seq_printf(m, "  %s (ID: %u):\n", pool->name, pool->id);
        seq_printf(m, "    Type: %u\n", pool->type);
        seq_printf(m, "    Status: 0x%04x\n", pool->status);
        seq_printf(m, "    Size: %lu MB\n",
                   stats.total_size / (1024 * 1024));
        seq_printf(m, "    Used: %lu MB\n",
                   stats.used_size / (1024 * 1024));
        seq_printf(m, "    Free: %lu MB\n",
                   stats.free_size / (1024 * 1024));
        seq_printf(m, "    Peak: %lu MB\n",
                   stats.peak_used / (1024 * 1024));
        seq_printf(m, "    Allocations: %u\n", stats.allocations);
        seq_printf(m, "    Active: %u\n", stats.active_allocations);
        seq_printf(m, "    Fragmentation: %u%%\n",
                   stats.fragmentation);
        seq_printf(m, "    Errors: %u\n", stats.errors);
        seq_printf(m, "\n");
    }
    
    return 0;
}

static int pool_debugfs_open(struct inode *inode, struct file *file)
{
    return single_open(file, pool_debugfs_show, inode->i_private);
}

static const struct file_operations pool_debugfs_fops = {
    .owner = THIS_MODULE,
    .open = pool_debugfs_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * pool_debugfs_init - Initialize DebugFS
 */
static void pool_debugfs_init(struct pool_manager *manager)
{
    struct dentry *dir;
    
    dir = debugfs_create_dir(POOL_MANAGER_NAME, NULL);
    if (IS_ERR_OR_NULL(dir)) {
        pr_warn("Failed to create debugfs directory\n");
        return;
    }
    
    manager->debugfs_root = dir;
    
    debugfs_create_file("info", 0444, dir, manager, &pool_debugfs_fops);
    debugfs_create_u32("pool_count", 0444, dir, &manager->pool_count);
    debugfs_create_u32("errors", 0444, dir, &manager->errors);
    debugfs_create_u64("total_size", 0444, dir, &manager->total_size);
    debugfs_create_u64("used_size", 0444, dir, &manager->used_size);
}

/**
 * pool_debugfs_exit - Clean up DebugFS
 */
static void pool_debugfs_exit(struct pool_manager *manager)
{
    debugfs_remove_recursive(manager->debugfs_root);
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * pool_manager_create_pool - Create a memory pool
 */
int pool_manager_create_pool(const char *name, unsigned int type,
                             phys_addr_t start, size_t size,
                             unsigned int flags)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    
    if (!manager) {
        pr_err("Pool manager not initialized\n");
        return -ENODEV;
    }
    
    if (!name || size == 0) {
        pr_err("Invalid pool parameters\n");
        return -EINVAL;
    }
    
    pool = pool_create(manager, name, type, start, size, flags);
    if (!pool) {
        pr_err("Failed to create pool\n");
        return -ENOMEM;
    }
    
    return pool->id;
}
EXPORT_SYMBOL_GPL(pool_manager_create_pool);

/**
 * pool_manager_destroy_pool - Destroy a memory pool
 */
int pool_manager_destroy_pool(unsigned int id)
{
    struct memory_pool *pool;
    int ret;
    
    pool = pool_manager_get_pool(id);
    if (!pool) {
        pr_err("Pool not found: %u\n", id);
        return -ENOENT;
    }
    
    ret = pool_destroy(pool);
    if (ret) {
        pr_err("Failed to destroy pool: %u\n", id);
        return ret;
    }
    
    return 0;
}
EXPORT_SYMBOL_GPL(pool_manager_destroy_pool);

/**
 * pool_manager_alloc - Allocate from a pool
 */
void *pool_manager_alloc(size_t size, unsigned int type, gfp_t flags)
{
    return pool_manager_alloc(size, type, flags);
}
EXPORT_SYMBOL_GPL(pool_manager_alloc);

/**
 * pool_manager_free - Free memory
 */
int pool_manager_free(void *ptr, size_t size)
{
    return pool_manager_free(ptr, size);
}
EXPORT_SYMBOL_GPL(pool_manager_free);

/**
 * pool_manager_alloc_aligned - Allocate aligned memory
 */
void *pool_manager_alloc_aligned(size_t size, size_t align,
                                 unsigned int type, gfp_t flags)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    void *ptr = NULL;
    
    if (!manager || size == 0)
        return NULL;
    
    mutex_lock(&manager->global_mutex);
    
    list_for_each_entry(pool, &manager->pools, list) {
        if (pool->type == type) {
            ptr = pool_alloc_aligned_wrapper(pool, size, align, flags);
            if (ptr) {
                manager->used_size += size;
                if (manager->used_size > manager->peak_size) {
                    manager->peak_size = manager->used_size;
                }
                manager->total_allocations++;
                manager->active_allocations++;
                break;
            }
        }
    }
    
    mutex_unlock(&manager->global_mutex);
    
    return ptr;
}
EXPORT_SYMBOL_GPL(pool_manager_alloc_aligned);

/**
 * pool_manager_defrag - Defragment pools
 */
int pool_manager_defrag(void)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool;
    int ret = 0;
    
    if (!manager)
        return -ENODEV;
    
    mutex_lock(&manager->global_mutex);
    
    list_for_each_entry(pool, &manager->pools, list) {
        int r = pool_defrag(pool);
        if (r < 0 && ret == 0) {
            ret = r;
        }
    }
    
    mutex_unlock(&manager->global_mutex);
    
    return ret;
}
EXPORT_SYMBOL_GPL(pool_manager_defrag);

/**
 * pool_manager_get_stats - Get pool manager statistics
 */
void pool_manager_get_stats(struct pool_manager_stats *stats)
{
    pool_manager_get_stats(stats);
}
EXPORT_SYMBOL_GPL(pool_manager_get_stats);

/* ============================================================================
 * Module Initialization and Exit
 * ============================================================================ */

/**
 * pool_manager_init - Module initialization
 */
static int __init pool_manager_init(void)
{
    struct pool_manager *manager;
    struct memory_pool *default_pool;
    
    pr_info("Pool Manager loading...\n");
    pr_info("Version: %s\n", POOL_MANAGER_VERSION);
    
    /* Allocate manager */
    manager = kzalloc(sizeof(*manager), GFP_KERNEL);
    if (!manager) {
        pr_err("Failed to allocate manager\n");
        return -ENOMEM;
    }
    
    mutex_init(&manager->global_mutex);
    spin_lock_init(&manager->global_lock);
    INIT_LIST_HEAD(&manager->pools);
    manager->next_id = 1;
    
    /* Initialize timer */
    timer_setup(&manager->stats_timer, pool_manager_stats_timer, 0);
    mod_timer(&manager->stats_timer, jiffies + msecs_to_jiffies(1000));
    
    /* Start manager thread */
    manager->stop_manager = false;
    manager->manager_thread = kthread_run(pool_manager_thread, manager,
                                         "pool_manager");
    if (IS_ERR(manager->manager_thread)) {
        pr_err("Failed to create manager thread\n");
        kfree(manager);
        return PTR_ERR(manager->manager_thread);
    }
    
    /* Initialize debugfs */
    pool_debugfs_init(manager);
    
    /* Set global pointer */
    global_manager = manager;
    
    /* Create default pool */
    default_pool = pool_create(manager, "default", POOL_TYPE_GENERAL,
                             0x50000000, POOL_DEFAULT_SIZE, POOL_FLAG_NONE);
    if (!default_pool) {
        pr_warn("Failed to create default pool\n");
    }
    
    pr_info("Pool Manager loaded successfully\n");
    return 0;
}

/**
 * pool_manager_exit - Module exit
 */
static void __exit pool_manager_exit(void)
{
    struct pool_manager *manager = global_manager;
    struct memory_pool *pool, *tmp;
    
    if (!manager)
        return;
    
    pr_info("Pool Manager unloading...\n");
    
    /* Stop manager thread */
    manager->stop_manager = true;
    if (manager->manager_thread) {
        kthread_stop(manager->manager_thread);
    }
    
    /* Delete timer */
    del_timer_sync(&manager->stats_timer);
    
    /* Clean up debugfs */
    pool_debugfs_exit(manager);
    
    /* Destroy pools */
    list_for_each_entry_safe(pool, tmp, &manager->pools, list) {
        pool_destroy(pool);
    }
    
    global_manager = NULL;
    kfree(manager);
    
    pr_info("Pool Manager unloaded\n");
}

module_init(pool_manager_init);
module_exit(pool_manager_exit);
