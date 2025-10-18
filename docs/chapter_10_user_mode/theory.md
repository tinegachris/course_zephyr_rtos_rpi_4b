# Chapter 10: User Mode - Theory and Advanced Implementation

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Advanced User Mode Architecture

### Hardware Support Requirements

User mode functionality requires specific hardware capabilities that vary across processor architectures:

#### ARM Cortex-M with MPU

```c
/*
 * ARM Cortex-M MPU Configuration for User Mode
 *
 * Memory Protection Unit (MPU) provides:
 * - 8-16 programmable memory regions (depends on implementation)
 * - Fine-grained access control (Read/Write/Execute permissions)
 * - Privilege level enforcement (Privileged vs Unprivileged)
 * - Subregion disable capability for efficient memory mapping
 */

// MPU region configuration structure
struct mpu_region_config {
    uint32_t base_address;    // Region base (must be aligned to size)
    uint32_t size_encoding;   // Size field encoding (2^(n+1) bytes)
    uint32_t access_control;  // Access permissions and attributes
    bool     enabled;         // Region enable/disable
};

// Configure MPU region for user space
void configure_user_mpu_region(uint8_t region_num,
                              uintptr_t base_addr,
                              size_t size,
                              uint32_t permissions)
{
    // Calculate size encoding for MPU
    uint32_t size_bits = 31 - __builtin_clz(size) - 1;
    
    // Configure region base address
    MPU->RBAR = (base_addr & MPU_RBAR_ADDR_Msk) | 
                (region_num & MPU_RBAR_REGION_Msk) |
                MPU_RBAR_VALID_Msk;
    
    // Configure region attributes and size
    MPU->RASR = (permissions & MPU_RASR_AP_Msk) |
                ((size_bits << MPU_RASR_SIZE_Pos) & MPU_RASR_SIZE_Msk) |
                MPU_RASR_ENABLE_Msk;
}
```

#### x86 with Paging

```c
/*
 * x86 Paging Support for User Mode
 * 
 * Page tables provide:
 * - Virtual to physical address translation
 * - Page-level access control (User/Supervisor, Read/Write)
 * - Execute disable (NX) bit support
 * - Large page support for efficient memory mapping
 */

// Page table entry structure (simplified)
struct page_table_entry {
    uint64_t present : 1;        // Page present in memory
    uint64_t writable : 1;       // Write permission
    uint64_t user_accessible : 1; // User mode access allowed
    uint64_t write_through : 1;   // Cache write-through
    uint64_t cache_disable : 1;   // Cache disable
    uint64_t accessed : 1;        // Page accessed flag
    uint64_t dirty : 1;          // Page dirty flag
    uint64_t page_size : 1;      // Page size (0=4KB, 1=large page)
    uint64_t global : 1;         // Global page
    uint64_t available : 3;      // Available for software use
    uint64_t physical_addr : 40; // Physical address (4KB aligned)
    uint64_t reserved : 11;      // Reserved bits
    uint64_t execute_disable : 1; // Execute disable (NX bit)
} __packed;

// Set up page table entry for user space
void setup_user_page(struct page_table_entry *pte,
                    uint64_t physical_addr,
                    bool writable,
                    bool executable)
{
    pte->present = 1;
    pte->user_accessible = 1;
    pte->writable = writable ? 1 : 0;
    pte->execute_disable = executable ? 0 : 1;
    pte->physical_addr = physical_addr >> 12; // 4KB alignment
}
```

### Memory Domain Implementation Details

#### Domain Data Structures

```c
// Architecture-specific domain data (ARM example)
struct arch_mem_domain {
    // MPU region allocation bitmap
    uint32_t mpu_regions_used;
    
    // Page table pointer (for MMU architectures)
    uint32_t *page_table_base;
    
    // ASID (Address Space Identifier) for TLB management
    uint8_t asid;
    
    // Domain-specific configuration flags
    uint32_t config_flags;
};

// Memory partition with extended attributes
struct k_mem_partition_extended {
    struct k_mem_partition base;
    
    // Cache policies
    enum {
        CACHE_POLICY_WRITEBACK,
        CACHE_POLICY_WRITETHROUGH,
        CACHE_POLICY_NOCACHE
    } cache_policy;
    
    // Memory type (device, normal, strongly ordered)
    enum memory_type mem_type;
    
    // Shareability attributes
    bool shareable;
    
    // Execute-never attribute
    bool execute_never;
};
```

#### Dynamic Memory Management

```c
// Dynamic partition allocation system
struct dynamic_partition_pool {
    struct k_mem_partition partitions[MAX_DYNAMIC_PARTITIONS];
    uint32_t allocation_bitmap;
    struct k_spinlock lock;
    size_t total_size;
    size_t available_size;
};

static struct dynamic_partition_pool dynamic_pool;

// Allocate dynamic partition from pool
struct k_mem_partition *alloc_dynamic_partition(size_t size, uint32_t attrs)
{
    k_spinlock_key_t key = k_spin_lock(&dynamic_pool.lock);
    
    // Find free partition slot
    int partition_idx = find_first_zero_bit(&dynamic_pool.allocation_bitmap,
                                          MAX_DYNAMIC_PARTITIONS);
    
    if (partition_idx >= MAX_DYNAMIC_PARTITIONS) {
        k_spin_unlock(&dynamic_pool.lock, key);
        return NULL; // No free partitions
    }
    
    // Check available memory
    if (size > dynamic_pool.available_size) {
        k_spin_unlock(&dynamic_pool.lock, key);
        return NULL; // Insufficient memory
    }
    
    // Allocate partition
    struct k_mem_partition *partition = &dynamic_pool.partitions[partition_idx];
    partition->start = allocate_memory_region(size);
    partition->size = size;
    partition->attr = attrs;
    
    // Mark partition as allocated
    dynamic_pool.allocation_bitmap |= BIT(partition_idx);
    dynamic_pool.available_size -= size;
    
    k_spin_unlock(&dynamic_pool.lock, key);
    return partition;
}
```

## System Call Implementation

### System Call Architecture

#### Call Path Analysis

```c
/*
 * System Call Execution Flow:
 * 
 * 1. User space calls function marked with __syscall
 * 2. Generated stub switches to supervisor mode (SVC instruction)
 * 3. SVC handler determines system call number
 * 4. Kernel validates parameters and permissions
 * 5. Kernel executes privileged operation
 * 6. Return value passed back to user space
 * 7. CPU switches back to user mode
 */

// System call handler entry point
void svc_handler(struct arch_esf *esf)
{
    uint32_t syscall_num = extract_syscall_number(esf);
    
    // Validate system call number
    if (syscall_num >= NUM_SYSCALLS) {
        set_syscall_return_value(esf, -ENOSYS);
        return;
    }
    
    // Get current thread for permission checking
    k_tid_t current_thread = k_current_get();
    
    // Check if thread has permission for this system call
    if (!check_syscall_permission(current_thread, syscall_num)) {
        set_syscall_return_value(esf, -EPERM);
        return;
    }
    
    // Extract parameters from stack/registers
    struct syscall_params params;
    extract_syscall_params(esf, &params);
    
    // Dispatch to appropriate handler
    syscall_return_t result = syscall_dispatch_table[syscall_num](&params);
    
    // Set return value
    set_syscall_return_value(esf, result);
}
```

#### Parameter Validation Framework

```c
// Parameter validation macros and functions
#define K_SYSCALL_VERIFY(expr) \
    do { \
        if (!(expr)) { \
            return -EINVAL; \
        } \
    } while (0)

#define K_SYSCALL_VERIFY_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            LOG_ERR("Syscall validation failed: " msg); \
            return -EINVAL; \
        } \
    } while (0)

// String validation using Zephyr's built-in function
// k_usermode_string_nlen() is provided by Zephyr and returns string length
// with error indication through err parameter
size_t k_usermode_string_nlen(const char *src, size_t maxlen, int *err);

// Example usage in verification functions
static inline int verify_user_string(const char *user_str, size_t max_len)
{
    int err;
    size_t actual_len;
    
    // Check string accessibility and get length
    actual_len = k_usermode_string_nlen(user_str, max_len, &err);
    if (err != 0) {
        return -EFAULT;  // String not accessible or fault occurred
    }
    
    // Check if string is properly null-terminated within bounds
    if (actual_len == max_len) {
        return -EINVAL;  // String too long (not null-terminated)
    }
    
    // Verify we can read the entire string including null terminator
    if (K_SYSCALL_MEMORY_READ(user_str, actual_len + 1) != 0) {
        return -EFAULT;
    }
    
    return 0;  // String is valid
}

// Buffer validation for system calls
int k_usermode_buffer_validate(const void *addr, size_t size, bool write)
{
    if (!is_user_context()) {
        return 0; // Kernel context - assume valid
    }
    
    // Check buffer alignment
    if ((uintptr_t)addr & (sizeof(void *) - 1)) {
        return -EINVAL; // Misaligned buffer
    }
    
    // Validate entire buffer is accessible
    if (!arch_buffer_validate(addr, size, write)) {
        return -EFAULT;
    }
    
    return 0;
}
```

### Advanced System Call Patterns

#### Asynchronous System Calls

```c
// Asynchronous system call support
struct async_syscall_context {
    k_tid_t calling_thread;
    uint32_t syscall_id;
    void (*completion_callback)(int result, void *user_data);
    void *user_data;
    atomic_t ref_count;
};

// Queue for pending async operations
K_MSGQ_DEFINE(async_syscall_queue, 
              sizeof(struct async_syscall_context *), 
              16, 4);

// Initiate asynchronous system call
__syscall int k_async_syscall_start(uint32_t syscall_id,
                                   void *params,
                                   void (*callback)(int, void *),
                                   void *user_data);

int z_impl_k_async_syscall_start(uint32_t syscall_id,
                               void *params,
                               void (*callback)(int, void *),
                               void *user_data)
{
    // Allocate async context
    struct async_syscall_context *ctx = k_malloc(sizeof(*ctx));
    if (!ctx) {
        return -ENOMEM;
    }
    
    // Initialize context
    ctx->calling_thread = k_current_get();
    ctx->syscall_id = syscall_id;
    ctx->completion_callback = callback;
    ctx->user_data = user_data;
    atomic_set(&ctx->ref_count, 1);
    
    // Queue for background processing
    int ret = k_msgq_put(&async_syscall_queue, &ctx, K_NO_WAIT);
    if (ret != 0) {
        k_free(ctx);
        return ret;
    }
    
    return 0; // Success - operation will complete asynchronously
}
```

#### System Call Tracing and Profiling

```c
// System call performance monitoring
struct syscall_stats {
    uint64_t call_count;
    uint64_t total_time_ns;
    uint64_t max_time_ns;
    uint64_t min_time_ns;
    uint32_t error_count;
};

static struct syscall_stats syscall_statistics[NUM_SYSCALLS];

// Instrumented system call wrapper
static inline syscall_return_t trace_syscall_execution(uint32_t syscall_num,
                                                      syscall_func_t handler,
                                                      struct syscall_params *params)
{
    uint64_t start_time = k_cycle_get_64();
    
    // Execute system call
    syscall_return_t result = handler(params);
    
    uint64_t end_time = k_cycle_get_64();
    uint64_t duration = k_cyc_to_ns_floor64(end_time - start_time);
    
    // Update statistics
    struct syscall_stats *stats = &syscall_statistics[syscall_num];
    stats->call_count++;
    stats->total_time_ns += duration;
    
    if (duration > stats->max_time_ns) {
        stats->max_time_ns = duration;
    }
    
    if (stats->min_time_ns == 0 || duration < stats->min_time_ns) {
        stats->min_time_ns = duration;
    }
    
    if (result < 0) {
        stats->error_count++;
    }
    
    // Log expensive system calls
    if (duration > SYSCALL_SLOW_THRESHOLD_NS) {
        LOG_WRN("Slow syscall %u: %llu ns", syscall_num, duration);
    }
    
    return result;
}
```

## Advanced Security Features

### Memory Protection Hardening

#### Stack Canaries for User Threads

```c
// Stack canary support for user mode threads
#define STACK_CANARY_MAGIC 0xDEADBEEF

struct user_thread_stack_info {
    uint32_t *canary_location;
    uint32_t original_canary;
    size_t stack_size;
    void *stack_base;
};

// Initialize stack canary for user thread
void init_user_stack_canary(k_tid_t thread)
{
    if (!(thread->base.user_options & K_USER)) {
        return; // Not a user thread
    }
    
    // Calculate canary location (near stack base)
    uint32_t *canary_addr = (uint32_t *)((char *)thread->stack_info.start + 
                                        sizeof(uint32_t) * 4);
    
    // Generate random canary value
    uint32_t canary_value = sys_rand32_get() ^ STACK_CANARY_MAGIC;
    
    // Store canary
    *canary_addr = canary_value;
    
    // Save canary info for checking
    thread->arch.stack_canary_addr = canary_addr;
    thread->arch.stack_canary_value = canary_value;
}

// Check stack canary integrity
bool check_user_stack_canary(k_tid_t thread)
{
    if (!thread->arch.stack_canary_addr) {
        return true; // No canary set
    }
    
    uint32_t current_value = *thread->arch.stack_canary_addr;
    return current_value == thread->arch.stack_canary_value;
}
```

#### Control Flow Integrity

```c
// Control Flow Integrity (CFI) support
struct cfi_context {
    uint32_t *return_address_stack;
    size_t ras_top;
    size_t ras_size;
    uint32_t call_count;
    uint32_t return_count;
};

// CFI instrumentation for function calls
void __attribute__((no_instrument_function)) 
cfi_function_entry(void *func_addr, void *call_site)
{
    k_tid_t current = k_current_get();
    if (!(current->base.user_options & K_USER)) {
        return; // Skip CFI for kernel threads
    }
    
    struct cfi_context *cfi = &current->arch.cfi_context;
    
    // Push return address onto shadow stack
    if (cfi->ras_top < cfi->ras_size - 1) {
        cfi->return_address_stack[cfi->ras_top++] = (uint32_t)call_site;
        cfi->call_count++;
    } else {
        // Shadow stack overflow - security violation
        LOG_ERR("CFI: Shadow stack overflow in thread %p", current);
        k_thread_abort(current);
    }
}

void __attribute__((no_instrument_function))
cfi_function_exit(void *func_addr, void *call_site)
{
    k_tid_t current = k_current_get();
    if (!(current->base.user_options & K_USER)) {
        return;
    }
    
    struct cfi_context *cfi = &current->arch.cfi_context;
    
    // Verify return address matches shadow stack
    if (cfi->ras_top > 0) {
        uint32_t expected_return = cfi->return_address_stack[--cfi->ras_top];
        if (expected_return != (uint32_t)call_site) {
            // Return address mismatch - ROP attack detected
            LOG_ERR("CFI: Return address mismatch in thread %p", current);
            LOG_ERR("Expected: 0x%08x, Actual: 0x%08x", 
                    expected_return, (uint32_t)call_site);
            k_thread_abort(current);
        }
        cfi->return_count++;
    }
}
```

### Secure Boot Integration

```c
// User mode application signature verification
struct app_signature {
    uint32_t magic;
    uint32_t version;
    uint32_t code_size;
    uint32_t signature_algo;
    uint8_t signature[SIGNATURE_SIZE];
    uint8_t public_key_hash[HASH_SIZE];
};

// Verify user application before loading
int verify_user_application(const void *app_image, size_t image_size)
{
    const struct app_signature *sig = (const struct app_signature *)
        ((const char *)app_image + image_size - sizeof(struct app_signature));
    
    // Verify signature magic
    if (sig->magic != APP_SIGNATURE_MAGIC) {
        return -EINVAL;
    }
    
    // Verify application size
    if (sig->code_size > image_size - sizeof(struct app_signature)) {
        return -EINVAL;
    }
    
    // Calculate application hash
    uint8_t app_hash[HASH_SIZE];
    crypto_hash_sha256(app_image, sig->code_size, app_hash);
    
    // Verify signature
    int ret = crypto_verify_signature(app_hash, HASH_SIZE,
                                    sig->signature, SIGNATURE_SIZE,
                                    sig->public_key_hash);
    if (ret != 0) {
        LOG_ERR("Application signature verification failed");
        return ret;
    }
    
    LOG_INF("User application signature verified successfully");
    return 0;
}
```

## Performance Optimization

### System Call Optimization

#### Fast Path System Calls

```c
// Optimized system call dispatch for common operations
static inline syscall_return_t fast_syscall_dispatch(uint32_t syscall_num,
                                                    uint32_t arg0, uint32_t arg1,
                                                    uint32_t arg2, uint32_t arg3)
{
    switch (syscall_num) {
    case SYSCALL_K_SLEEP:
        // Fast path for sleep - no parameter validation needed
        return (syscall_return_t)k_sleep(K_TICKS(arg0));
        
    case SYSCALL_K_YIELD:
        // Fast path for yield - no parameters
        k_yield();
        return 0;
        
    case SYSCALL_K_CURRENT_GET:
        // Fast path for current thread - return thread ID
        return (syscall_return_t)k_current_get();
        
    case SYSCALL_K_UPTIME_GET:
        // Fast path for uptime - no parameters
        return (syscall_return_t)k_uptime_get_32();
        
    default:
        // Fall through to regular system call handling
        return regular_syscall_dispatch(syscall_num, arg0, arg1, arg2, arg3);
    }
}
```

#### Memory Domain Caching

```c
// Cache memory domain configuration for performance
struct domain_cache_entry {
    struct k_mem_domain *domain;
    uint32_t partition_hash;
    void *cached_hw_config;
    uint64_t last_access_time;
    bool dirty;
};

#define DOMAIN_CACHE_SIZE 8
static struct domain_cache_entry domain_cache[DOMAIN_CACHE_SIZE];
static struct k_spinlock domain_cache_lock;

// Find cached domain configuration
static struct domain_cache_entry *find_cached_domain(struct k_mem_domain *domain)
{
    k_spinlock_key_t key = k_spin_lock(&domain_cache_lock);
    
    for (int i = 0; i < DOMAIN_CACHE_SIZE; i++) {
        if (domain_cache[i].domain == domain) {
            domain_cache[i].last_access_time = k_uptime_get();
            k_spin_unlock(&domain_cache_lock, key);
            return &domain_cache[i];
        }
    }
    
    k_spin_unlock(&domain_cache_lock, key);
    return NULL;
}

// Fast domain switch using cached configuration
int fast_domain_switch(k_tid_t thread, struct k_mem_domain *new_domain)
{
    struct domain_cache_entry *cache_entry = find_cached_domain(new_domain);
    
    if (cache_entry && !cache_entry->dirty) {
        // Use cached hardware configuration
        return arch_apply_cached_domain_config(cache_entry->cached_hw_config);
    } else {
        // Rebuild and cache configuration
        return rebuild_and_cache_domain_config(new_domain);
    }
}
```

This comprehensive theory section provides deep insights into Zephyr's user mode implementation, enabling developers to understand the underlying mechanisms and optimize their secure applications effectively.

[Next: User Mode Lab](./lab.md)