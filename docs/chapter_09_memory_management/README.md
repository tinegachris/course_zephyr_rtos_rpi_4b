# Chapter 9: Memory Management in Zephyr RTOS

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Building on System Observability Foundations

Having mastered tracing and logging in Chapter 8—gaining essential visibility into concurrent thread behavior, system performance, and debugging complex interactions—you now understand how to observe and analyze system behavior. However, many of the performance issues and system failures you've learned to detect through tracing stem from improper memory management practices.

This chapter addresses the root causes of many problems you've learned to diagnose, providing comprehensive coverage of Zephyr's sophisticated memory management capabilities that directly impact the system performance, reliability, and real-time behavior you've been measuring and optimizing.

## Overview

Memory management represents one of the most critical aspects of embedded systems development, serving as the foundation for the reliable concurrent systems you've been building and debugging. Poor memory management creates the performance bottlenecks, resource exhaustion, and timing issues that your tracing and logging skills help you identify—this chapter teaches you to prevent these problems at their source.

## Learning Objectives

Upon completion of this chapter, you will be able to:

- **Architect Memory Systems**: Design efficient memory layouts for embedded applications
- **Implement Dynamic Allocation**: Utilize Zephyr's heap management APIs effectively
- **Manage Thread Resources**: Configure and optimize thread stack allocation
- **Apply Memory Protection**: Implement memory domains and access control
- **Optimize Performance**: Select appropriate allocators for specific use cases
- **Debug Memory Issues**: Identify and resolve memory-related problems
- **Ensure Real-Time Behavior**: Maintain deterministic memory operations

## Introduction to Memory Management in Embedded Systems

### Memory Architecture in Modern Embedded Systems

Embedded systems employ a hierarchical memory architecture optimized for performance, power consumption, and cost constraints:

#### Primary Memory Types

**Static RAM (SRAM)**
- Primary working memory for runtime operations
- Fastest access speeds with minimal latency
- Limited capacity (typically KB to low MB range)
- Contains system heap, thread stacks, and global variables
- Directly addressable by the processor

**Flash Memory**
- Non-volatile storage for program code and constants
- Execute-in-place (XIP) capabilities reduce SRAM requirements
- Larger capacity but significantly slower write operations
- Organized in sectors/pages for efficient management

**Cache Memory**
- High-speed buffer between processor and main memory
- Instruction and data caches improve execution performance
- Cache-aware programming enhances system efficiency
- Critical for high-performance embedded applications

#### Extended Memory Options

**External SDRAM**
- Higher capacity volatile memory
- Requires memory controller configuration
- Higher latency due to external bus overhead
- Suitable for buffer-intensive applications

**External Flash**
- Extended non-volatile storage
- File system support for complex data management
- Various interfaces (SPI, QSPI, parallel)
- Used for data logging and content storage

### Zephyr Memory Layout Architecture

Zephyr organizes system memory into well-defined regions, each serving specific purposes:

#### Core Memory Regions

**Text Segment (.text)**
```
┌─────────────────────────────────┐
│         Program Code            │  ← Application and kernel code
│      (Read-Only, Execute)       │    Function implementations
└─────────────────────────────────┘    Interrupt handlers
```

**Data Segment (.data)**
```
┌─────────────────────────────────┐
│    Initialized Global Data      │  ← Pre-initialized variables
│      (Read-Write Access)        │    Configuration constants
└─────────────────────────────────┘    Static structures
```

**BSS Segment (.bss)**
```
┌─────────────────────────────────┐
│   Uninitialized Global Data     │  ← Zero-initialized variables
│      (Read-Write Access)        │    Large arrays
└─────────────────────────────────┘    Static buffers
```

**Heap Region**
```
┌─────────────────────────────────┐
│      Dynamic Allocation         │  ← Runtime memory allocation
│         (Managed)               │    k_malloc(), k_heap_alloc()
│    ┌─────────────────────────┐   │    Object pools
│    │   Free Space (grows)    │   │    Buffer management
│    └─────────────────────────┘   │
└─────────────────────────────────┘
```

**Stack Region**
```
┌─────────────────────────────────┐
│        Thread Stacks            │  ← Per-thread execution stacks
│                                 │    Local variables
│    Thread 1: [████████████]     │    Function call frames
│    Thread 2: [██████████  ]     │    Context preservation
│    Thread 3: [████████    ]     │
└─────────────────────────────────┘
```

## Memory Allocation Strategies

### Static Memory Allocation

Static allocation provides predictable, compile-time memory assignment:

**Characteristics:**
- Memory allocated at compile time
- Zero runtime allocation overhead
- Deterministic memory usage patterns
- No risk of allocation failures
- Optimal for real-time systems

**Implementation Example:**
```c
// Global arrays with compile-time allocation
static uint8_t sensor_buffer[1024];
static struct sensor_data readings[MAX_SENSORS];

// Thread stacks allocated statically
K_THREAD_STACK_DEFINE(sensor_thread_stack, 2048);
K_THREAD_STACK_DEFINE(control_thread_stack, 1536);
```

### Dynamic Memory Allocation

Dynamic allocation provides runtime flexibility at the cost of complexity:

**Advantages:**
- Flexible memory usage based on runtime needs
- Efficient memory utilization
- Support for variable-size data structures
- Enables sophisticated memory management

**Challenges:**
- Potential allocation failures
- Memory fragmentation over time
- Runtime overhead for allocation operations
- Increased complexity in error handling

## Zephyr Memory Management APIs

### Kernel Heap Management

Zephyr's kernel heap provides synchronized, thread-safe dynamic memory allocation:

#### K_HEAP_DEFINE Macro

Creates statically defined heaps with automatic initialization:

```c
// Define a 4KB heap for application use
K_HEAP_DEFINE(app_heap, 4096);

// Define specialized heaps for different purposes
K_HEAP_DEFINE(buffer_heap, 2048);    // Network buffers
K_HEAP_DEFINE(config_heap, 1024);    // Configuration data
```

#### Runtime Heap Initialization

Create heaps over application-controlled memory regions:

```c
static uint8_t heap_memory[8192];
static struct k_heap custom_heap;

void initialize_custom_heap(void)
{
    k_heap_init(&custom_heap, heap_memory, sizeof(heap_memory));
}
```

#### Memory Allocation APIs

**k_heap_alloc() - Basic Allocation**
```c
void *k_heap_alloc(struct k_heap *h, size_t bytes, k_timeout_t timeout);

// Example usage
void *buffer = k_heap_alloc(&app_heap, 256, K_FOREVER);
if (buffer == NULL) {
    // Handle allocation failure
    return -ENOMEM;
}
```

**k_heap_calloc() - Zero-Initialized Allocation**
```c
void *k_heap_calloc(struct k_heap *h, size_t num, size_t size, k_timeout_t timeout);

// Allocate array of structures, initialized to zero
struct sensor_reading *readings = k_heap_calloc(&app_heap, 
                                               NUM_SENSORS,
                                               sizeof(struct sensor_reading),
                                               K_NO_WAIT);
```

**k_heap_free() - Memory Deallocation**
```c
void k_heap_free(struct k_heap *h, void *mem);

// Always free allocated memory
k_heap_free(&app_heap, buffer);
buffer = NULL;  // Prevent accidental reuse
```

### System Heap Integration

Zephyr provides standard C library compatibility through system heap integration:

#### Standard Library Functions

```c
// Standard malloc/free with system heap
void *ptr = malloc(512);
if (ptr != NULL) {
    // Use allocated memory
    free(ptr);
}

// Calloc for zero-initialized arrays
int *array = calloc(100, sizeof(int));
free(array);

// Realloc for dynamic resizing
char *buffer = malloc(256);
buffer = realloc(buffer, 512);  // Expand to 512 bytes
free(buffer);
```

#### Zephyr-Specific System Heap APIs

```c
// Aligned allocation
void *aligned_ptr = k_aligned_alloc(64, 1024);  // 64-byte aligned, 1KB
k_free(aligned_ptr);

// Thread-aware allocation
void *thread_ptr = k_malloc(256);  // Uses thread's resource pool
k_free(thread_ptr);
```

### Memory Slab Management

Memory slabs provide fixed-size block allocation with deterministic performance:

#### Slab Definition and Initialization

```c
// Define memory slab for fixed-size objects
#define BUFFER_SIZE 128
#define NUM_BUFFERS 16

K_MEM_SLAB_DEFINE(packet_slab, BUFFER_SIZE, NUM_BUFFERS, 4);

// Alternative: Runtime initialization
static struct k_mem_slab custom_slab;
static uint8_t slab_memory[NUM_BUFFERS * BUFFER_SIZE];

void init_custom_slab(void)
{
    k_mem_slab_init(&custom_slab, slab_memory, BUFFER_SIZE, NUM_BUFFERS);
}
```

#### Slab Allocation and Deallocation

```c
void demonstrate_slab_usage(void)
{
    void *buffer;
    
    // Allocate from slab (deterministic time)
    if (k_mem_slab_alloc(&packet_slab, &buffer, K_NO_WAIT) == 0) {
        // Use buffer for packet processing
        process_packet(buffer);
        
        // Return to slab
        k_mem_slab_free(&packet_slab, buffer);
    } else {
        // All buffers in use
        LOG_WRN("Packet slab exhausted");
    }
}
```

### Thread Stack Management

Proper stack management ensures system stability and optimal memory usage:

#### Static Stack Definition

```c
// Define thread stacks with specific sizes
K_THREAD_STACK_DEFINE(worker_stack, 2048);
K_THREAD_STACK_DEFINE(network_stack, 4096);
K_THREAD_STACK_DEFINE(sensor_stack, 1536);
```

#### Dynamic Stack Allocation

```c
// Allocate thread stacks dynamically
k_thread_stack_t *dynamic_stack = k_thread_stack_alloc(2048, 0);
if (dynamic_stack == NULL) {
    return -ENOMEM;
}

// Create thread with dynamic stack
k_tid_t thread_id = k_thread_create(&thread_data,
                                   dynamic_stack,
                                   2048,
                                   thread_entry_point,
                                   NULL, NULL, NULL,
                                   THREAD_PRIORITY,
                                   0,
                                   K_NO_WAIT);

// Free stack when thread terminates
k_thread_stack_free(dynamic_stack);
```

#### Stack Size Calculation

Determining appropriate stack sizes requires careful analysis:

```c
// Stack size considerations
#define BASE_STACK_SIZE     512   // Minimum for thread overhead
#define FUNCTION_OVERHEAD   64    // Per function call
#define LOCAL_VAR_SIZE      128   // Local variables
#define ISR_OVERHEAD        256   // Interrupt handling
#define SAFETY_MARGIN       256   // Additional safety buffer

#define CALCULATED_STACK_SIZE (BASE_STACK_SIZE + \
                              FUNCTION_OVERHEAD + \
                              LOCAL_VAR_SIZE + \
                              ISR_OVERHEAD + \
                              SAFETY_MARGIN)
```

### Memory Domains and Protection

Memory domains provide isolation and access control for enhanced system security:

#### Memory Domain Creation

```c
// Define memory partitions for different access levels
K_MEM_PARTITION_DEFINE(user_partition,
                      user_memory_start,
                      user_memory_size,
                      K_MEM_PARTITION_P_RW_U_RW);

K_MEM_PARTITION_DEFINE(config_partition,
                      config_memory_start,
                      config_memory_size,
                      K_MEM_PARTITION_P_RW_U_RO);

// Create memory domain with partitions
static struct k_mem_partition *partitions[] = {
    &user_partition,
    &config_partition
};

static struct k_mem_domain user_domain;

void setup_user_domain(void)
{
    k_mem_domain_init(&user_domain, ARRAY_SIZE(partitions), partitions);
}
```

#### Thread Domain Assignment

```c
void assign_thread_to_domain(k_tid_t thread)
{
    // Add thread to memory domain
    k_mem_domain_add_thread(&user_domain, thread);
}
```

## Performance Optimization Techniques

### Allocator Selection Strategy

Choose appropriate allocators based on usage patterns:

```c
// Real-time critical path: Use memory slabs
K_MEM_SLAB_DEFINE(rt_buffer_slab, RT_BUFFER_SIZE, RT_BUFFER_COUNT, 4);

// Variable-size allocations: Use dedicated heap
K_HEAP_DEFINE(variable_heap, 8192);

// Standard operations: System heap
void *system_buffer = k_malloc(size);
```

### Memory Alignment Optimization

Proper alignment improves cache performance and prevents alignment faults:

```c
// Cache-line aligned allocation for DMA buffers
#define CACHE_LINE_SIZE 32
void *dma_buffer = k_aligned_alloc(CACHE_LINE_SIZE, buffer_size);

// Structure alignment for optimal packing
struct __packed sensor_data {
    uint32_t timestamp;
    uint16_t temperature;  
    uint16_t humidity;
} __aligned(4);
```

### Memory Pool Strategies

Implement object pools for frequently used data structures:

```c
// Message pool for inter-thread communication
struct message {
    uint8_t type;
    uint8_t priority;
    uint16_t length;
    uint8_t data[MAX_MESSAGE_SIZE];
};

K_MEM_SLAB_DEFINE(message_pool, sizeof(struct message), MAX_MESSAGES, 4);

struct message *alloc_message(void)
{
    struct message *msg;
    if (k_mem_slab_alloc(&message_pool, (void **)&msg, K_NO_WAIT) == 0) {
        memset(msg, 0, sizeof(struct message));
        return msg;
    }
    return NULL;
}

void free_message(struct message *msg)
{
    k_mem_slab_free(&message_pool, msg);
}
```

## Debugging and Monitoring

### Memory Statistics and Tracking

Monitor memory usage patterns for optimization:

```c
void print_heap_statistics(struct k_heap *heap)
{
    struct sys_memory_stats stats;
    
    if (k_heap_runtime_stats_get(heap, &stats) == 0) {
        printk("Heap Statistics:\n");
        printk("  Total Size: %zu bytes\n", stats.allocated_bytes + stats.free_bytes);
        printk("  Allocated:  %zu bytes\n", stats.allocated_bytes);
        printk("  Free:       %zu bytes\n", stats.free_bytes);
        printk("  Max Usage:  %zu bytes\n", stats.max_allocated_bytes);
    }
}
```

### Stack Usage Monitoring

Track stack usage to optimize stack sizes:

```c
void monitor_stack_usage(k_tid_t thread)
{
    size_t unused_space = k_thread_stack_space_get(thread);
    size_t total_size = k_thread_stack_size_get(thread);
    size_t used_space = total_size - unused_space;
    
    printk("Thread stack usage: %zu/%zu bytes (%.1f%%)\n",
           used_space, total_size, (float)used_space * 100.0f / total_size);
}
```

### Memory Error Detection

Implement checks for common memory errors:

```c
void safe_memory_operations(void)
{
    void *ptr = k_malloc(256);
    
    if (ptr == NULL) {
        LOG_ERR("Memory allocation failed");
        return;
    }
    
    // Use memory safely
    memset(ptr, 0, 256);
    
    // Always free allocated memory
    k_free(ptr);
    ptr = NULL;  // Prevent use-after-free
}
```

## Real-World Implementation Patterns

### Buffer Management System

```c
struct buffer_manager {
    struct k_heap *heap;
    struct k_mem_slab *small_buffers;
    struct k_mem_slab *large_buffers;
    struct k_spinlock lock;
};

void *buffer_alloc(struct buffer_manager *mgr, size_t size)
{
    k_spinlock_key_t key = k_spin_lock(&mgr->lock);
    void *buffer = NULL;
    
    if (size <= SMALL_BUFFER_SIZE) {
        k_mem_slab_alloc(mgr->small_buffers, &buffer, K_NO_WAIT);
    } else if (size <= LARGE_BUFFER_SIZE) {
        k_mem_slab_alloc(mgr->large_buffers, &buffer, K_NO_WAIT);
    } else {
        buffer = k_heap_alloc(mgr->heap, size, K_NO_WAIT);
    }
    
    k_spin_unlock(&mgr->lock, key);
    return buffer;
}
```

### Resource Pool Management

```c
void setup_thread_resource_pools(void)
{
    // High-priority threads get larger resource pools
    k_thread_resource_pool_assign(&high_priority_thread, &high_priority_heap);
    
    // Normal threads share a common pool
    k_thread_resource_pool_assign(&normal_thread1, &shared_heap);
    k_thread_resource_pool_assign(&normal_thread2, &shared_heap);
    
    // Background threads use minimal resources
    k_thread_resource_pool_assign(&background_thread, &minimal_heap);
}
```

This comprehensive introduction establishes the foundation for advanced memory management techniques in Zephyr RTOS. The following sections will dive deeper into practical implementation and optimization strategies.

[Next: Memory Management Theory](./theory.md)
