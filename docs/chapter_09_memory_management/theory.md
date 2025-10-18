# Chapter 9: Memory Management - Theory and Implementation

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Advanced Memory Management Concepts

### Memory Allocation Algorithms

Understanding the algorithms behind memory allocation enables better design decisions and performance optimization.

#### First-Fit Algorithm

Zephyr's heap implementation uses sophisticated allocation algorithms:

```c
/*
 * First-fit allocation strategy:
 * - Searches for the first available block of sufficient size
 * - Fast allocation for small to medium-sized blocks
 * - May lead to fragmentation over time
 */

struct heap_block {
    size_t size;
    bool free;
    struct heap_block *next;
};
```

#### Best-Fit Considerations

While Zephyr primarily uses first-fit, understanding best-fit helps optimize usage:

```c
// Conceptual best-fit approach for comparison
void *best_fit_alloc(size_t requested_size)
{
    struct heap_block *best_block = NULL;
    size_t smallest_fit = SIZE_MAX;
    
    // Find the smallest block that satisfies the request
    for (struct heap_block *block = heap_start; block; block = block->next) {
        if (block->free && block->size >= requested_size) {
            if (block->size < smallest_fit) {
                smallest_fit = block->size;
                best_block = block;
            }
        }
    }
    
    return allocate_from_block(best_block, requested_size);
}
```

### Memory Fragmentation Analysis

Fragmentation significantly impacts long-running embedded systems:

#### Internal Fragmentation

Memory wasted within allocated blocks due to alignment requirements:

```c
// Example of internal fragmentation
struct aligned_data {
    uint8_t flag;        // 1 byte
    // 3 bytes padding for alignment
    uint32_t value;      // 4 bytes
    uint8_t status;      // 1 byte
    // 3 bytes padding
} __aligned(4);          // Total: 12 bytes for 6 bytes of data
```

#### External Fragmentation

Free memory scattered across non-contiguous blocks:

```c
// Visual representation of external fragmentation
/*
 * Memory Layout After Multiple Allocations/Deallocations:
 * 
 * [Used:128][Free:32][Used:256][Free:16][Used:64][Free:128]
 * 
 * Total Free: 176 bytes
 * Largest Contiguous: 128 bytes
 * Cannot satisfy allocation > 128 bytes despite having enough total free space
 */
```

### Anti-Fragmentation Strategies

#### Memory Pool Design

```c
// Pool-based allocation reduces fragmentation
#define SMALL_BUFFER_SIZE   64
#define MEDIUM_BUFFER_SIZE  256
#define LARGE_BUFFER_SIZE   1024

K_MEM_SLAB_DEFINE(small_pool, SMALL_BUFFER_SIZE, 32, 4);
K_MEM_SLAB_DEFINE(medium_pool, MEDIUM_BUFFER_SIZE, 16, 4);
K_MEM_SLAB_DEFINE(large_pool, LARGE_BUFFER_SIZE, 8, 4);

void *smart_alloc(size_t size)
{
    void *ptr = NULL;
    
    if (size <= SMALL_BUFFER_SIZE) {
        k_mem_slab_alloc(&small_pool, &ptr, K_NO_WAIT);
    } else if (size <= MEDIUM_BUFFER_SIZE) {
        k_mem_slab_alloc(&medium_pool, &ptr, K_NO_WAIT);
    } else if (size <= LARGE_BUFFER_SIZE) {
        k_mem_slab_alloc(&large_pool, &ptr, K_NO_WAIT);
    } else {
        // Fall back to heap for very large allocations
        ptr = k_heap_alloc(&app_heap, size, K_NO_WAIT);
    }
    
    return ptr;
}
```

## Thread Stack Management

### Stack Architecture and Growth

Understanding stack behavior is crucial for optimal sizing:

```c
/*
 * Stack Growth Pattern (ARM Cortex-M):
 * 
 * Higher Memory Addresses
 * ┌─────────────────────────┐  ← Stack Base (Initial SP)
 * │                         │
 * │    Available Space      │
 * │                         │
 * ├─────────────────────────┤  ← Current Stack Pointer
 * │  Local Variables        │
 * │  Function Parameters    │
 * │  Return Addresses       │
 * │  Saved Registers        │
 * └─────────────────────────┘  ← Stack Limit
 * Lower Memory Addresses
 */
```

### Stack Frame Analysis

Each function call creates a stack frame:

```c
void analyze_stack_frame(int param1, float param2)
{
    // Stack frame contains:
    // 1. Function parameters (param1, param2)
    // 2. Return address
    // 3. Saved registers (r4-r11, lr)
    // 4. Local variables
    
    char local_array[256];      // 256 bytes
    int local_counter;          // 4 bytes + alignment
    float result;               // 4 bytes
    
    // Additional space for nested function calls
    nested_function(local_array, &local_counter);
    
    // Total frame size: ~280-300 bytes depending on alignment
}

size_t calculate_stack_usage(void)
{
    size_t base_overhead = 64;      // Thread control block overhead
    size_t function_calls = 8;      // Maximum call depth
    size_t frame_size = 128;        // Average frame size
    size_t local_variables = 512;   // Largest local variable space
    size_t isr_overhead = 256;      // Interrupt service overhead
    size_t safety_margin = 256;     // 25% safety buffer
    
    return base_overhead + (function_calls * frame_size) + 
           local_variables + isr_overhead + safety_margin;
}
```

### Dynamic Stack Allocation Patterns

Advanced stack management for dynamic systems:

```c
// Stack pool for dynamic thread creation
#define MAX_DYNAMIC_THREADS 8
#define DYNAMIC_STACK_SIZE  2048

static k_thread_stack_t *stack_pool[MAX_DYNAMIC_THREADS];
static bool stack_allocated[MAX_DYNAMIC_THREADS];
static struct k_spinlock stack_pool_lock;

k_thread_stack_t *acquire_stack(void)
{
    k_spinlock_key_t key = k_spin_lock(&stack_pool_lock);
    k_thread_stack_t *stack = NULL;
    
    for (int i = 0; i < MAX_DYNAMIC_THREADS; i++) {
        if (!stack_allocated[i]) {
            if (stack_pool[i] == NULL) {
                stack_pool[i] = k_thread_stack_alloc(DYNAMIC_STACK_SIZE, 0);
            }
            
            if (stack_pool[i] != NULL) {
                stack_allocated[i] = true;
                stack = stack_pool[i];
                break;
            }
        }
    }
    
    k_spin_unlock(&stack_pool_lock, key);
    return stack;
}

void release_stack(k_thread_stack_t *stack)
{
    k_spinlock_key_t key = k_spin_lock(&stack_pool_lock);
    
    for (int i = 0; i < MAX_DYNAMIC_THREADS; i++) {
        if (stack_pool[i] == stack) {
            stack_allocated[i] = false;
            break;
        }
    }
    
    k_spin_unlock(&stack_pool_lock, key);
}
```

## Memory Domains and Protection

### Memory Protection Unit (MPU) Integration

Modern embedded processors provide hardware memory protection:

```c
// Memory region definitions for MPU
struct memory_region {
    uintptr_t base_addr;
    size_t size;
    uint32_t attributes;
};

static const struct memory_region app_regions[] = {
    {
        .base_addr = (uintptr_t)&__app_ram_start,
        .size = (size_t)&__app_ram_size,
        .attributes = K_MEM_PARTITION_P_RW_U_RW
    },
    {
        .base_addr = (uintptr_t)&__app_flash_start,
        .size = (size_t)&__app_flash_size,
        .attributes = K_MEM_PARTITION_P_RO_U_RO
    }
};
```

### User Mode Memory Management

Implementing user mode applications with restricted memory access:

```c
// User mode thread with restricted memory access
K_THREAD_STACK_DEFINE(user_thread_stack, 2048);

// Define user-accessible memory partition
K_MEM_PARTITION_DEFINE(user_data_partition,
                      user_data_start,
                      sizeof(user_data),
                      K_MEM_PARTITION_P_RW_U_RW);

// Create memory domain for user threads
static struct k_mem_partition *user_partitions[] = {
    &user_data_partition,
    &k_mbedtls_partition,  // If using mbedTLS
};

static struct k_mem_domain user_domain;

void setup_user_mode_memory(void)
{
    // Initialize user memory domain
    k_mem_domain_init(&user_domain, 
                     ARRAY_SIZE(user_partitions), 
                     user_partitions);
    
    // Create user mode thread
    k_tid_t user_thread = k_thread_create(&user_thread_data,
                                         user_thread_stack,
                                         K_THREAD_STACK_SIZEOF(user_thread_stack),
                                         user_thread_entry,
                                         NULL, NULL, NULL,
                                         USER_THREAD_PRIORITY,
                                         K_USER,  // User mode flag
                                         K_NO_WAIT);
    
    // Assign thread to memory domain
    k_mem_domain_add_thread(&user_domain, user_thread);
}
```

### Memory Access Violation Handling

Implementing robust error handling for memory violations:

```c
// Memory fault handler
void memory_fault_handler(const struct arch_esf *esf)
{
    k_tid_t current_thread = k_current_get();
    uint32_t fault_addr = arch_fault_addr_get();
    
    LOG_ERR("Memory fault in thread %p at address 0x%08x", 
            current_thread, fault_addr);
    
    // Log thread information
    LOG_ERR("Thread: %s, Priority: %d", 
            k_thread_name_get(current_thread),
            k_thread_priority_get(current_thread));
    
    // Attempt recovery or terminate thread
    if (is_recoverable_fault(fault_addr)) {
        handle_recoverable_fault(current_thread, fault_addr);
    } else {
        k_thread_abort(current_thread);
    }
}
```

## Advanced Heap Management

### Custom Heap Implementations

Implementing specialized heaps for specific use cases:

```c
// Ring buffer heap for continuous data streaming
struct ring_heap {
    uint8_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    struct k_spinlock lock;
};

void ring_heap_init(struct ring_heap *heap, void *memory, size_t size)
{
    heap->buffer = (uint8_t *)memory;
    heap->size = size;
    heap->head = 0;
    heap->tail = 0;
    heap->lock = (struct k_spinlock){};
}

void *ring_heap_alloc(struct ring_heap *heap, size_t bytes)
{
    k_spinlock_key_t key = k_spin_lock(&heap->lock);
    void *ptr = NULL;
    
    size_t available = (heap->tail >= heap->head) ? 
                      (heap->size - heap->tail + heap->head) :
                      (heap->head - heap->tail);
    
    if (bytes <= available) {
        ptr = &heap->buffer[heap->tail];
        heap->tail = (heap->tail + bytes) % heap->size;
    }
    
    k_spin_unlock(&heap->lock, key);
    return ptr;
}
```

### Memory Debugging and Profiling

Advanced debugging techniques for memory issues:

```c
// Memory allocation tracking
struct alloc_record {
    void *ptr;
    size_t size;
    k_tid_t thread;
    uint32_t timestamp;
    const char *file;
    int line;
};

#define MAX_ALLOC_RECORDS 256
static struct alloc_record alloc_records[MAX_ALLOC_RECORDS];
static int record_index = 0;
static struct k_spinlock record_lock;

#define TRACKED_MALLOC(size) \
    tracked_malloc(size, __FILE__, __LINE__)

void *tracked_malloc(size_t size, const char *file, int line)
{
    void *ptr = k_malloc(size);
    
    if (ptr != NULL) {
        k_spinlock_key_t key = k_spin_lock(&record_lock);
        
        alloc_records[record_index] = (struct alloc_record){
            .ptr = ptr,
            .size = size,
            .thread = k_current_get(),
            .timestamp = k_uptime_get_32(),
            .file = file,
            .line = line
        };
        
        record_index = (record_index + 1) % MAX_ALLOC_RECORDS;
        k_spin_unlock(&record_lock, key);
    }
    
    return ptr;
}

void print_allocation_report(void)
{
    k_spinlock_key_t key = k_spin_lock(&record_lock);
    
    printk("=== Memory Allocation Report ===\n");
    for (int i = 0; i < MAX_ALLOC_RECORDS; i++) {
        struct alloc_record *rec = &alloc_records[i];
        if (rec->ptr != NULL) {
            printk("Ptr: %p, Size: %zu, Thread: %p, Time: %u, %s:%d\n",
                   rec->ptr, rec->size, rec->thread, rec->timestamp,
                   rec->file, rec->line);
        }
    }
    
    k_spin_unlock(&record_lock, key);
}
```

## Real-Time Memory Considerations

### Deterministic Allocation Patterns

Ensuring predictable timing for real-time systems:

```c
// Pre-allocated message pools for real-time communication
#define RT_MESSAGE_SIZE 64
#define RT_MESSAGE_COUNT 32

K_MEM_SLAB_DEFINE(rt_message_slab, RT_MESSAGE_SIZE, RT_MESSAGE_COUNT, 4);

// Real-time safe message allocation
void *rt_alloc_message(void)
{
    void *msg;
    int ret = k_mem_slab_alloc(&rt_message_slab, &msg, K_NO_WAIT);
    return (ret == 0) ? msg : NULL;
}

void rt_free_message(void *msg)
{
    k_mem_slab_free(&rt_message_slab, msg);
}
```

### Priority-Based Memory Management

Implementing priority-aware allocation strategies:

```c
// Priority-based heap allocation
struct priority_heap {
    struct k_heap high_priority;
    struct k_heap normal_priority;
    struct k_heap low_priority;
};

void *priority_alloc(struct priority_heap *heap, size_t size, int priority)
{
    void *ptr = NULL;
    
    if (priority >= HIGH_PRIORITY_THRESHOLD) {
        ptr = k_heap_alloc(&heap->high_priority, size, K_NO_WAIT);
    }
    
    if (ptr == NULL && priority >= NORMAL_PRIORITY_THRESHOLD) {
        ptr = k_heap_alloc(&heap->normal_priority, size, K_NO_WAIT);
    }
    
    if (ptr == NULL) {
        ptr = k_heap_alloc(&heap->low_priority, size, K_NO_WAIT);
    }
    
    return ptr;
}
```

This comprehensive theory section provides deep insights into Zephyr's memory management implementation, enabling developers to make informed decisions about memory allocation strategies and optimization techniques.

[Next: Memory Management Lab](./lab.md)