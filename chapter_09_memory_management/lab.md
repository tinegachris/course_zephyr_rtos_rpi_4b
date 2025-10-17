# Chapter 9: Memory Management - Laboratory Exercise

## Lab Overview

This comprehensive laboratory exercise demonstrates Zephyr's memory management capabilities through practical implementation of core memory management techniques. Students will implement heap management, memory slabs, stack monitoring, and debug memory issues.

## Learning Objectives

Upon completion of this lab, you will have:

- Implemented dynamic memory allocation using Zephyr APIs
- Created memory slabs for fixed-size buffer management  
- Monitored and analyzed thread stack usage
- Developed memory debugging and profiling capabilities
- Applied memory management best practices

## Prerequisites

- Completion of Chapters 1-8
- Understanding of C programming and pointers
- Familiarity with Zephyr build system
- Development board (Raspberry Pi 4B recommended)

## Lab Setup

### Project Structure

Create the following project structure:

```
memory_lab/
├── CMakeLists.txt
├── prj.conf
└── src/
    └── main.c
```

### Step 1: Project Configuration

Create `CMakeLists.txt`:

```cmake
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(memory_lab)

target_sources(app PRIVATE src/main.c)
```

Create `prj.conf`:

```ini
# Memory Management Configuration
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_LOG_PRINTK=y

# System Configuration
CONFIG_MAIN_STACK_SIZE=2048
```

## Exercise 1: Basic Memory Allocation

Create `src/main.c` with basic allocation examples:

```c
/*
 * Memory Management Lab - Basic Allocation
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(memory_lab, LOG_LEVEL_INF);

void demonstrate_basic_allocation(void)
{
    LOG_INF("=== Basic Memory Allocation Demo ===");
    
    /* Basic malloc/free */
    void *buffer1 = k_malloc(256);
    if (buffer1 != NULL) {
        LOG_INF("Allocated 256 bytes at %p", buffer1);
        memset(buffer1, 0x55, 256);
        k_free(buffer1);
        LOG_INF("Freed buffer1");
    } else {
        LOG_ERR("Failed to allocate buffer1");
    }
    
    /* Aligned allocation */
    void *aligned_buffer = k_aligned_alloc(64, 1024);
    if (aligned_buffer != NULL) {
        LOG_INF("Allocated 1024 bytes aligned to 64 at %p", aligned_buffer);
        LOG_INF("Address alignment check: %s", 
                ((uintptr_t)aligned_buffer % 64 == 0) ? "PASSED" : "FAILED");
        k_free(aligned_buffer);
        LOG_INF("Freed aligned_buffer");
    }
    
    /* Calloc (zero-initialized) */
    int *array = k_calloc(100, sizeof(int));
    if (array != NULL) {
        LOG_INF("Allocated zero-initialized array of 100 ints");
        
        /* Verify it's zeroed */
        bool all_zero = true;
        for (int i = 0; i < 100; i++) {
            if (array[i] != 0) {
                all_zero = false;
                break;
            }
        }
        LOG_INF("Zero initialization check: %s", all_zero ? "PASSED" : "FAILED");
        
        k_free(array);
        LOG_INF("Freed array");
    }
}

int main(void)
{
    LOG_INF("=== Zephyr Memory Management Lab ===");
    
    k_sleep(K_SECONDS(1));
    demonstrate_basic_allocation();
    
    LOG_INF("Lab Exercise 1 completed!");
    return 0;
}
```

### Build and Test Exercise 1

```bash
west build -b rpi_4b memory_lab
west flash
```

**Expected Output:**
```
*** Booting Zephyr OS build zephyr-v4.2.0 ***
[00:00:01.000,000] <inf> memory_lab: === Zephyr Memory Management Lab ===
[00:00:02.000,000] <inf> memory_lab: === Basic Memory Allocation Demo ===
[00:00:02.001,000] <inf> memory_lab: Allocated 256 bytes at 0x20001234
[00:00:02.002,000] <inf> memory_lab: Freed buffer1
[00:00:02.003,000] <inf> memory_lab: Allocated 1024 bytes aligned to 64 at 0x20001400
[00:00:02.004,000] <inf> memory_lab: Address alignment check: PASSED
[00:00:02.005,000] <inf> memory_lab: Freed aligned_buffer
[00:00:02.006,000] <inf> memory_lab: Allocated zero-initialized array of 100 ints
[00:00:02.007,000] <inf> memory_lab: Zero initialization check: PASSED
[00:00:02.008,000] <inf> memory_lab: Freed array
[00:00:02.009,000] <inf> memory_lab: Lab Exercise 1 completed!
```

## Exercise 2: Custom Heap Management

Add heap allocation examples to your main.c:

```c
void demonstrate_heap_allocation(void)
{
    LOG_INF("=== Custom Heap Allocation Demo ===");
    
    /* Create a custom heap */
    static uint8_t heap_memory[2048];
    static struct k_heap custom_heap;
    
    k_heap_init(&custom_heap, heap_memory, sizeof(heap_memory));
    LOG_INF("Initialized custom heap with %zu bytes", sizeof(heap_memory));
    
    /* Allocate from custom heap */
    void *buffer1 = k_heap_alloc(&custom_heap, 512, K_NO_WAIT);
    void *buffer2 = k_heap_alloc(&custom_heap, 256, K_NO_WAIT);
    void *buffer3 = k_heap_alloc(&custom_heap, 128, K_NO_WAIT);
    
    LOG_INF("Allocated buffers: %p, %p, %p", buffer1, buffer2, buffer3);
    
    /* Check heap statistics */
    struct sys_memory_stats stats;
    if (k_heap_runtime_stats_get(&custom_heap, &stats) == 0) {
        LOG_INF("Heap stats: %zu bytes allocated, %zu bytes free",
                stats.allocated_bytes, stats.free_bytes);
    }
    
    /* Try to allocate more than available */
    void *buffer4 = k_heap_alloc(&custom_heap, 2000, K_NO_WAIT);
    if (buffer4 == NULL) {
        LOG_INF("Large allocation failed as expected");
    }
    
    /* Free some buffers */
    k_heap_free(&custom_heap, buffer2);
    LOG_INF("Freed middle buffer");
    
    /* Allocate in freed space */
    void *buffer5 = k_heap_alloc(&custom_heap, 200, K_NO_WAIT);
    LOG_INF("Allocated in freed space: %p", buffer5);
    
    /* Cleanup */
    k_heap_free(&custom_heap, buffer1);
    k_heap_free(&custom_heap, buffer3);
    k_heap_free(&custom_heap, buffer5);
    LOG_INF("Cleaned up all allocations");
}
```

Add the call to main():

```c
int main(void)
{
    LOG_INF("=== Zephyr Memory Management Lab ===");
    
    k_sleep(K_SECONDS(1));
    demonstrate_basic_allocation();
    
    k_sleep(K_SECONDS(1));
    demonstrate_heap_allocation();
    
    LOG_INF("Lab Exercise 2 completed!");
    return 0;
}
```

## Exercise 3: Memory Slabs

Add memory slab demonstration:

```c
void demonstrate_memory_slabs(void)
{
    LOG_INF("=== Memory Slab Demo ===");
    
    /* Define memory slab for fixed-size buffers */
    #define SLAB_BLOCK_SIZE 128
    #define SLAB_BLOCK_COUNT 8
    
    K_MEM_SLAB_DEFINE_STATIC(demo_slab, SLAB_BLOCK_SIZE, SLAB_BLOCK_COUNT, 4);
    
    void *buffers[SLAB_BLOCK_COUNT];
    int allocated_count = 0;
    
    LOG_INF("Created slab: %d blocks of %d bytes each", 
            SLAB_BLOCK_COUNT, SLAB_BLOCK_SIZE);
    
    /* Allocate all blocks */
    for (int i = 0; i < SLAB_BLOCK_COUNT; i++) {
        int ret = k_mem_slab_alloc(&demo_slab, &buffers[i], K_NO_WAIT);
        if (ret == 0) {
            allocated_count++;
            /* Write pattern to verify buffer integrity */
            memset(buffers[i], 0xAA + i, SLAB_BLOCK_SIZE);
            LOG_INF("Allocated slab block %d at %p", i, buffers[i]);
        } else {
            LOG_WRN("Failed to allocate slab block %d", i);
            break;
        }
    }
    
    /* Try to allocate one more (should fail) */
    void *extra_buffer;
    int ret = k_mem_slab_alloc(&demo_slab, &extra_buffer, K_NO_WAIT);
    if (ret != 0) {
        LOG_INF("Extra allocation failed as expected (slab full)");
    }
    
    /* Verify buffer patterns */
    for (int i = 0; i < allocated_count; i++) {
        uint8_t *buf = (uint8_t *)buffers[i];
        bool pattern_ok = true;
        for (int j = 0; j < SLAB_BLOCK_SIZE; j++) {
            if (buf[j] != (0xAA + i)) {
                pattern_ok = false;
                break;
            }
        }
        LOG_INF("Buffer %d pattern check: %s", i, pattern_ok ? "PASSED" : "FAILED");
    }
    
    /* Free half the blocks */
    for (int i = 0; i < allocated_count / 2; i++) {
        k_mem_slab_free(&demo_slab, buffers[i]);
        LOG_INF("Freed slab block %d", i);
    }
    
    /* Allocate again */
    ret = k_mem_slab_alloc(&demo_slab, &extra_buffer, K_NO_WAIT);
    if (ret == 0) {
        LOG_INF("Allocated after freeing: %p", extra_buffer);
        k_mem_slab_free(&demo_slab, extra_buffer);
    }
    
    /* Cleanup remaining blocks */
    for (int i = allocated_count / 2; i < allocated_count; i++) {
        k_mem_slab_free(&demo_slab, buffers[i]);
    }
    LOG_INF("Memory slab demo completed");
}
```

## Exercise 4: Stack Monitoring

Add stack usage analysis:

```c
void demonstrate_stack_monitoring(void)
{
    LOG_INF("=== Stack Monitoring Demo ===");
    
    k_tid_t current = k_current_get();
    
    /* Check initial stack usage */
    size_t unused = k_thread_stack_space_get(current);
    size_t total = k_thread_stack_size_get(current);
    size_t used = total - unused;
    
    LOG_INF("Initial stack usage: %zu/%zu bytes (%.1f%%)",
            used, total, (float)used * 100.0f / total);
    
    /* Function that uses more stack */
    char local_buffer[512];
    memset(local_buffer, 0xAA, sizeof(local_buffer));
    
    /* Check stack usage after local allocation */
    unused = k_thread_stack_space_get(current);
    used = total - unused;
    
    LOG_INF("After local buffer: %zu/%zu bytes (%.1f%%)",
            used, total, (float)used * 100.0f / total);
    
    /* Simulate nested function calls */
    for (int depth = 0; depth < 5; depth++) {
        char nested_buffer[64];
        snprintf(nested_buffer, sizeof(nested_buffer), "Depth %d", depth);
        
        unused = k_thread_stack_space_get(current);
        used = total - unused;
        
        LOG_INF("Depth %d stack: %zu/%zu bytes (%.1f%%)",
                depth, used, total, (float)used * 100.0f / total);
                
        /* Warning if usage is high */
        if ((float)used * 100.0f / total > 75.0f) {
            LOG_WRN("High stack usage detected at depth %d", depth);
        }
    }
}
```

## Exercise 5: Memory Stress Testing

Add comprehensive stress test:

```c
void memory_stress_test(void)
{
    LOG_INF("=== Memory Stress Test ===");
    
    #define STRESS_ITERATIONS 50
    #define STRESS_BUFFER_SIZE 256
    
    void *buffers[STRESS_ITERATIONS];
    int success_count = 0;
    uint32_t start_time = k_uptime_get_32();
    
    /* Allocate many buffers */
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        buffers[i] = k_malloc(STRESS_BUFFER_SIZE);
        if (buffers[i] != NULL) {
            success_count++;
            /* Write unique pattern to each buffer */
            uint32_t pattern = 0x12345678 + i;
            for (int j = 0; j < STRESS_BUFFER_SIZE / 4; j++) {
                ((uint32_t *)buffers[i])[j] = pattern;
            }
        } else {
            LOG_WRN("Allocation failed at iteration %d", i);
            break;
        }
    }
    
    uint32_t alloc_time = k_uptime_get_32() - start_time;
    LOG_INF("Successfully allocated %d/%d buffers in %u ms", 
            success_count, STRESS_ITERATIONS, alloc_time);
    
    /* Verify and free buffers */
    start_time = k_uptime_get_32();
    for (int i = 0; i < success_count; i++) {
        if (buffers[i] != NULL) {
            /* Verify pattern integrity */
            uint32_t expected_pattern = 0x12345678 + i;
            bool pattern_ok = true;
            
            for (int j = 0; j < STRESS_BUFFER_SIZE / 4; j++) {
                if (((uint32_t *)buffers[i])[j] != expected_pattern) {
                    pattern_ok = false;
                    break;
                }
            }
            
            if (!pattern_ok) {
                LOG_ERR("Buffer %d pattern corrupted!", i);
            }
            
            k_free(buffers[i]);
        }
    }
    
    uint32_t free_time = k_uptime_get_32() - start_time;
    LOG_INF("Verified and freed all buffers in %u ms", free_time);
    LOG_INF("Stress test completed successfully");
}
```

## Complete Main Function

Update your main function to run all exercises:

```c
int main(void)
{
    LOG_INF("=== Zephyr Memory Management Lab ===");
    LOG_INF("Build time: " __DATE__ " " __TIME__);
    
    k_sleep(K_SECONDS(1));
    demonstrate_basic_allocation();
    
    k_sleep(K_SECONDS(1));
    demonstrate_heap_allocation();
    
    k_sleep(K_SECONDS(1));
    demonstrate_memory_slabs();
    
    k_sleep(K_SECONDS(1));
    demonstrate_stack_monitoring();
    
    k_sleep(K_SECONDS(1));
    memory_stress_test();
    
    LOG_INF("=== All Lab Exercises Completed Successfully! ===");
    
    /* Keep system running for observation */
    while (1) {
        k_sleep(K_SECONDS(10));
        LOG_INF("System running... Press reset to restart");
    }
    
    return 0;
}
```

## Expected Learning Outcomes

Upon completion of this lab, students will have:

1. **Practical Memory Management Skills:**
   - Implemented basic memory allocation/deallocation
   - Created and managed custom heaps
   - Used memory slabs for deterministic allocation

2. **Performance Analysis Capabilities:**
   - Monitored memory usage patterns  
   - Analyzed stack utilization
   - Conducted stress testing

3. **Debugging and Monitoring Expertise:**
   - Tracked allocation patterns
   - Verified memory integrity
   - Identified optimization opportunities

## Troubleshooting Guide

### Common Issues and Solutions

1. **Build Errors:**
   - Verify CMakeLists.txt syntax
   - Check prj.conf settings
   - Ensure all includes are correct

2. **Allocation Failures:**
   - Check heap sizes in configuration
   - Monitor total memory usage
   - Consider using smaller test sizes

3. **Stack Issues:**
   - Increase MAIN_STACK_SIZE if needed
   - Monitor stack usage during development
   - Optimize local variable usage

This comprehensive lab provides practical experience with Zephyr's core memory management features.

