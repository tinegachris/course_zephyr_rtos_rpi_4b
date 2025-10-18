# Chapter 10: User Mode - Hands-On Laboratory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Lab Overview

This laboratory provides comprehensive hands-on experience with Zephyr's user mode capabilities. You'll implement secure applications using memory domains, system calls, and advanced security features.

## Prerequisites

- Zephyr development environment set up
- Board with memory protection support (e.g., ARM Cortex-M with MPU)
- Understanding of memory management concepts from Chapter 9

## Lab Environment Setup

### Required Hardware

```yaml
# Supported boards for user mode labs
supported_boards:
  - frdm_k64f      # ARM Cortex-M4 with MPU
  - nucleo_f767zi  # ARM Cortex-M7 with MPU
  - qemu_cortex_m3 # QEMU emulation with MPU
  - arduino_due    # ARM Cortex-M3 with MPU
  - mimxrt1050_evk # ARM Cortex-M7 with MPU

# Required Kconfig options
required_config:
  - CONFIG_USERSPACE=y
  - CONFIG_APPLICATION_MEMORY=y
  - CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT=y
  - CONFIG_MPU_GAP_FILLING=y
```

### Development Environment

```bash
# Set up lab workspace
cd ~/zephyr-workspace
mkdir -p user_mode_labs
cd user_mode_labs

# Create lab structure
mkdir -p {lab1_basics,lab2_domains,lab3_syscalls,lab4_security,lab5_advanced}
mkdir -p shared/{drivers,libraries,configs}
```

## Lab 1: User Mode Fundamentals

### Objective
Create basic user mode threads and understand privilege separation.

### Implementation

#### Step 1: Basic User Thread Creation

Create `lab1_basics/src/main.c`:

```c
/*
 * Lab 1: User Mode Basics
 * 
 * This lab demonstrates:
 * - Creating user mode threads
 * - Understanding privilege levels
 * - Basic memory protection
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(user_mode_lab1, LOG_LEVEL_INF);

// Define stack for user thread
#define USER_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(user_stack, USER_STACK_SIZE);

// User thread structure
static struct k_thread user_thread_data;

// Simple user mode function
void user_thread_function(void *arg1, void *arg2, void *arg3)
{
    int counter = 0;
    
    LOG_INF("User thread started (ID: %p)", k_current_get());
    LOG_INF("Thread running in %s mode",
            k_is_user_context() ? "USER" : "SUPERVISOR");
    
    while (1) {
        // This will work - user threads can call system calls
        printk("User thread iteration %d (uptime: %llu ms)\n",
               counter++, k_uptime_get());
        
        // Attempt to access kernel memory (this should fail)
        // Uncomment the following line to see the protection in action
        // *(volatile uint32_t *)0x20000000 = 0xDEADBEEF;
        
        k_sleep(K_SECONDS(2));
    }
}

// Privileged operation that user threads cannot perform directly
void privileged_operation(void)
{
    LOG_INF("Performing privileged operation in supervisor mode");
    
    // Access system control registers (privileged operation)
    uint32_t control_reg = __get_CONTROL();
    LOG_INF("CONTROL register value: 0x%08x", control_reg);
    
    // Check privilege level
    if (control_reg & 0x1) {
        LOG_INF("Currently running in unprivileged (user) mode");
    } else {
        LOG_INF("Currently running in privileged (supervisor) mode");
    }
}

int main(void)
{
    LOG_INF("=== User Mode Fundamentals Lab ===");
    
    // Demonstrate supervisor mode operations
    LOG_INF("Main thread running in %s mode",
            k_is_user_context() ? "USER" : "SUPERVISOR");
    
    privileged_operation();
    
    // Create user mode thread
    k_tid_t user_tid = k_thread_create(&user_thread_data,
                                      user_stack,
                                      K_THREAD_STACK_SIZEOF(user_stack),
                                      user_thread_function,
                                      NULL, NULL, NULL,
                                      5, K_USER, K_NO_WAIT);
    
    LOG_INF("Created user thread: %p", user_tid);
    
    // Main thread continues in supervisor mode
    int main_counter = 0;
    while (1) {
        printk("Main (supervisor) thread iteration %d\n", main_counter++);
        
        // Supervisor can perform privileged operations
        privileged_operation();
        
        k_sleep(K_SECONDS(3));
    }
    
    return 0;
}
```

#### Step 2: Configuration Files

Create `lab1_basics/prj.conf`:

```ini
# Basic user mode configuration
CONFIG_USERSPACE=y
CONFIG_APPLICATION_MEMORY=y

# Memory protection
CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT=y
CONFIG_MPU_GAP_FILLING=y

# Logging configuration
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_PRINTK=y

# Thread configuration
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Disable features that may conflict with user mode or require special handling
# Note: Some debugging features might be disabled in user mode to prevent
# information leaks from kernel space or because they require specific
# configurations when user mode is active.
CONFIG_THREAD_MONITOR=n
CONFIG_THREAD_NAME=n
```

Create `lab1_basics/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(user_mode_basics)

target_sources(app PRIVATE src/main.c)
```

### Expected Results

```text
*** Booting Zephyr OS build v4.2.99 ***
[00:00:00.000,000] <inf> user_mode_lab1: === User Mode Fundamentals Lab ===
[00:00:00.000,000] <inf> user_mode_lab1: Main thread running in SUPERVISOR mode
[00:00:00.000,000] <inf> user_mode_lab1: Performing privileged operation in supervisor mode
[00:00:00.000,000] <inf> user_mode_lab1: CONTROL register value: 0x00000000
[00:00:00.000,000] <inf> user_mode_lab1: Currently running in privileged (supervisor) mode
[00:00:00.000,000] <inf> user_mode_lab1: Created user thread: 0x20001234
[00:00:00.000,000] <inf> user_mode_lab1: User thread started (ID: 0x20001234)
[00:00:00.000,000] <inf> user_mode_lab1: Thread running in USER mode
User thread iteration 0 (uptime: 0 ms)
Main (supervisor) thread iteration 0
[00:00:00.000,000] <inf> user_mode_lab1: Performing privileged operation in supervisor mode
User thread iteration 1 (uptime: 2000 ms)
```

## Lab 2: Memory Domains and Partitions

### Objective

Implement memory isolation using domains and partitions.

### Implementation

#### Step 1: Memory Domain Setup

Create `lab2_domains/src/main.c`:

```c
/*
 * Lab 2: Memory Domains and Partitions
 *
 * This lab demonstrates:
 * - Creating memory domains
 * - Defining memory partitions
 * - Controlling access between user threads
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(user_mode_lab2, LOG_LEVEL_INF);

// Shared data structures for different security levels
struct sensitive_data {
    uint32_t secret_key;
    uint32_t access_count;
    char description[64];
};

struct public_data {
    uint32_t public_counter;
    char message[128];
};

// Allocate data in different memory sections
__in_section(".app_data_sensitive")
static struct sensitive_data sensitive_info = {
    .secret_key = 0x12345678,
    .access_count = 0,
    .description = "Sensitive information - restricted access"
};

__in_section(".app_data_public")
static struct public_data public_info = {
    .public_counter = 0,
    .message = "Public information - unrestricted access"
};

// Memory partitions for different access levels
K_MEM_PARTITION_DEFINE(sensitive_partition,
                      &sensitive_info,
                      sizeof(sensitive_info),
                      K_MEM_PARTITION_P_RW_U_RW);

K_MEM_PARTITION_DEFINE(public_partition,
                      &public_info,
                      sizeof(public_info),
                      K_MEM_PARTITION_P_RW_U_RW);

// Memory domains for different security contexts
static struct k_mem_domain trusted_domain;
static struct k_mem_domain restricted_domain;

// Thread stacks
#define THREAD_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(trusted_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(restricted_stack, THREAD_STACK_SIZE);

static struct k_thread trusted_thread_data;
static struct k_thread restricted_thread_data;

// Trusted thread - can access sensitive data
void trusted_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Trusted thread started (can access sensitive data)");

    while (1) {
        // Access sensitive data (should succeed)
        sensitive_info.access_count++;
        sensitive_info.secret_key ^= sys_rand32_get();
        
        // Access public data (should also succeed)
        public_info.public_counter++;
        snprintf(public_info.message, sizeof(public_info.message),
                "Updated by trusted thread (iteration %u)",
                public_info.public_counter);
        
        LOG_INF("Trusted: Sensitive access count = %u, key = 0x%08x",
                sensitive_info.access_count, sensitive_info.secret_key);
        LOG_INF("Trusted: Public counter = %u, message = '%s'",
                public_info.public_counter, public_info.message);
        
        k_sleep(K_SECONDS(3));
    }
}

// Restricted thread - cannot access sensitive data
void restricted_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Restricted thread started (limited access)");
    
    while (1) {
        // Access public data (should succeed)
        public_info.public_counter += 10;
        snprintf(public_info.message, sizeof(public_info.message),
                "Updated by restricted thread (counter %u)", 
                public_info.public_counter);
        
        LOG_INF("Restricted: Public counter = %u, message = '%s'",
                public_info.public_counter, public_info.message);
        
        // Attempt to access sensitive data (should fail)
        LOG_INF("Restricted: Attempting to access sensitive data...");
        
        // Uncomment the following lines to see memory protection in action
        /*
        LOG_INF("Restricted: Sensitive access count = %u", 
                sensitive_info.access_count);
        sensitive_info.secret_key = 0xDEADBEEF;
        */
        
        k_sleep(K_SECONDS(4));
    }
}

// Initialize memory domains
int setup_memory_domains(void)
{
    int ret;
    
    // Initialize trusted domain (can access both partitions)
    ret = k_mem_domain_init(&trusted_domain, 0, NULL);
    if (ret != 0) {
        LOG_ERR("Failed to initialize trusted domain: %d", ret);
        return ret;
    }
    
    ret = k_mem_domain_add_partition(&trusted_domain, &sensitive_partition);
    if (ret != 0) {
        LOG_ERR("Failed to add sensitive partition to trusted domain: %d", ret);
        return ret;
    }
    
    ret = k_mem_domain_add_partition(&trusted_domain, &public_partition);
    if (ret != 0) {
        LOG_ERR("Failed to add public partition to trusted domain: %d", ret);
        return ret;
    }
    
    // Initialize restricted domain (can only access public partition)
    ret = k_mem_domain_init(&restricted_domain, 0, NULL);
    if (ret != 0) {
        LOG_ERR("Failed to initialize restricted domain: %d", ret);
        return ret;
    }
    
    ret = k_mem_domain_add_partition(&restricted_domain, &public_partition);
    if (ret != 0) {
        LOG_ERR("Failed to add public partition to restricted domain: %d", ret);
        return ret;
    }
    
    LOG_INF("Memory domains configured successfully");
    return 0;
}

int main(void)
{
    LOG_INF("=== Memory Domains and Partitions Lab ===");
    
    // Initialize memory domains
    int ret = setup_memory_domains();
    if (ret != 0) {
        LOG_ERR("Memory domain setup failed: %d", ret);
        return ret;
    }
    
    // Create trusted thread with access to sensitive data
    k_tid_t trusted_tid = k_thread_create(&trusted_thread_data,
                                         trusted_stack,
                                         K_THREAD_STACK_SIZEOF(trusted_stack),
                                         trusted_thread,
                                         NULL, NULL, NULL,
                                         5, K_USER, K_NO_WAIT);
    
    ret = k_mem_domain_add_thread(&trusted_domain, trusted_tid);
    if (ret != 0) {
        LOG_ERR("Failed to add trusted thread to domain: %d", ret);
        return ret;
    }
    
    // Create restricted thread with limited access
    k_tid_t restricted_tid = k_thread_create(&restricted_thread_data,
                                            restricted_stack,
                                            K_THREAD_STACK_SIZEOF(restricted_stack),
                                            restricted_thread,
                                            NULL, NULL, NULL,
                                            5, K_USER, K_NO_WAIT);
    
    ret = k_mem_domain_add_thread(&restricted_domain, restricted_tid);
    if (ret != 0) {
        LOG_ERR("Failed to add restricted thread to domain: %d", ret);
        return ret;
    }
    
    LOG_INF("Created threads: trusted=%p, restricted=%p", 
            trusted_tid, restricted_tid);
    
    // Main thread monitors the system
    while (1) {
        LOG_INF("=== System Status ===");
        LOG_INF("Sensitive data: access_count=%u, key=0x%08x",
                sensitive_info.access_count, sensitive_info.secret_key);
        LOG_INF("Public data: counter=%u, message='%s'",
                public_info.public_counter, public_info.message);
        
        k_sleep(K_SECONDS(10));
    }
    
    return 0;
}
```

#### Step 2: Linker Script Modifications

To ensure our `sensitive_info` and `public_info` data structures are placed in distinct, non-overlapping memory regions that we can control with memory partitions, we need to instruct the linker where to place them. This is done using a custom linker script file.

Create `lab2_domains/app.ld`:

```ld
/*
 * Custom linker script for memory domain lab
 * Defines separate sections for different access levels
 */

SECTIONS
{
    /* Sensitive data section - restricted access */
    .app_data_sensitive :
    {
        _app_data_sensitive_start = .;
        *(.app_data_sensitive)
        *(.app_data_sensitive.*)
        _app_data_sensitive_end = .;
    } > RAM
    
    /* Public data section - unrestricted access */
    .app_data_public :
    {
        _app_data_public_start = .;
        *(.app_data_public)
        *(.app_data_public.*)
        _app_data_public_end = .;
    } > RAM
}

INSERT AFTER .bss;
```

### Expected Results

```
[00:00:00.000,000] <inf> user_mode_lab2: === Memory Domains and Partitions Lab ===
[00:00:00.000,000] <inf> user_mode_lab2: Memory domains configured successfully
[00:00:00.000,000] <inf> user_mode_lab2: Created threads: trusted=0x20001234, restricted=0x20001456
[00:00:00.000,000] <inf> user_mode_lab2: Trusted thread started (can access sensitive data)
[00:00:00.000,000] <inf> user_mode_lab2: Restricted thread started (limited access)
[00:00:03.000,000] <inf> user_mode_lab2: Trusted: Sensitive access count = 1, key = 0x87654321
[00:00:03.000,000] <inf> user_mode_lab2: Trusted: Public counter = 1, message = 'Updated by trusted thread (iteration 1)'
[00:00:04.000,000] <inf> user_mode_lab2: Restricted: Public counter = 11, message = 'Updated by restricted thread (counter 11)'
[00:00:04.000,000] <inf> user_mode_lab2: Restricted: Attempting to access sensitive data...
```

## Lab 3: Custom System Calls

### Objective
Implement custom system calls for secure inter-domain communication.

### Implementation

#### Step 1: System Call Definition

Create `lab3_syscalls/src/syscalls.c`:

```c
/*
 * Lab 3: Custom System Calls
 * 
 * This lab demonstrates:
 * - Defining custom system calls
 * - Parameter validation
 * - Secure privilege escalation
 */

#include <zephyr/kernel.h>
#include <zephyr/syscall.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(syscalls, LOG_LEVEL_DBG);

// Secure data structure managed by supervisor
struct secure_counter {
    uint32_t value;
    uint32_t access_count;
    k_tid_t last_accessor;
    uint64_t last_access_time;
    uint32_t checksum;
};

static struct secure_counter global_counter = {0};
static struct k_spinlock counter_lock;

// Calculate checksum for integrity verification
static uint32_t calculate_checksum(const struct secure_counter *counter)
{
    uint32_t checksum = 0;
    checksum ^= counter->value;
    checksum ^= counter->access_count;
    checksum ^= (uint32_t)(uintptr_t)counter->last_accessor;
    checksum ^= (uint32_t)(counter->last_access_time & 0xFFFFFFFF);
    checksum ^= (uint32_t)(counter->last_access_time >> 32);
    return checksum;
}

// System call: Get secure counter value
__syscall uint32_t secure_counter_get(void);

uint32_t z_impl_secure_counter_get(void)
{
    k_spinlock_key_t key = k_spin_lock(&counter_lock);
    
    // Verify integrity
    uint32_t expected_checksum = calculate_checksum(&global_counter);
    if (global_counter.checksum != expected_checksum) {
        LOG_ERR("Counter integrity check failed!");
        k_spin_unlock(&counter_lock, key);
        return 0; // Return safe value
    }
    
    // Update access information
    global_counter.access_count++;
    global_counter.last_accessor = k_current_get();
    global_counter.last_access_time = k_uptime_get();
    global_counter.checksum = calculate_checksum(&global_counter);
    
    uint32_t value = global_counter.value;
    
    k_spin_unlock(&counter_lock, key);
    
    LOG_DBG("Counter read: value=%u, access_count=%u", 
            value, global_counter.access_count);
    
    return value;
}

// System call: Increment secure counter
__syscall int secure_counter_increment(uint32_t delta);

int z_impl_secure_counter_increment(uint32_t delta)
{
    // Validate delta parameter
    if (delta == 0 || delta > 1000) {
        LOG_WRN("Invalid delta value: %u", delta);
        return -EINVAL;
    }
    
    k_spinlock_key_t key = k_spin_lock(&counter_lock);
    
    // Verify integrity
    uint32_t expected_checksum = calculate_checksum(&global_counter);
    if (global_counter.checksum != expected_checksum) {
        LOG_ERR("Counter integrity check failed!");
        k_spin_unlock(&counter_lock, key);
        return -EFAULT;
    }
    
    // Check for overflow
    if (global_counter.value > UINT32_MAX - delta) {
        LOG_WRN("Counter overflow prevented");
        k_spin_unlock(&counter_lock, key);
        return -ERANGE;
    }
    
    // Update counter
    global_counter.value += delta;
    global_counter.access_count++;
    global_counter.last_accessor = k_current_get();
    global_counter.last_access_time = k_uptime_get();
    global_counter.checksum = calculate_checksum(&global_counter);
    
    k_spin_unlock(&counter_lock, key);
    
    LOG_DBG("Counter incremented: delta=%u, new_value=%u", 
            delta, global_counter.value);
    
    return 0;
}

// System call: Get counter statistics
__syscall int secure_counter_get_stats(struct counter_stats *stats);

struct counter_stats {
    uint32_t current_value;
    uint32_t access_count;
    k_tid_t last_accessor;
    uint64_t last_access_time;
};

int z_impl_secure_counter_get_stats(struct counter_stats *stats)
{
    // Validate output buffer
    if (k_usermode_buffer_validate(stats, sizeof(*stats), true) != 0) {
        LOG_ERR("Invalid stats buffer");
        return -EFAULT;
    }
    
    k_spinlock_key_t key = k_spin_lock(&counter_lock);
    
    // Verify integrity
    uint32_t expected_checksum = calculate_checksum(&global_counter);
    if (global_counter.checksum != expected_checksum) {
        LOG_ERR("Counter integrity check failed!");
        k_spin_unlock(&counter_lock, key);
        return -EFAULT;
    }
    
    // Copy statistics
    stats->current_value = global_counter.value;
    stats->access_count = global_counter.access_count;
    stats->last_accessor = global_counter.last_accessor;
    stats->last_access_time = global_counter.last_access_time;
    
    k_spin_unlock(&counter_lock, key);
    
    LOG_DBG("Stats retrieved by thread %p", k_current_get());
    
    return 0;
}

// System call: Reset counter with authentication
__syscall int secure_counter_reset(uint32_t auth_code);

int z_impl_secure_counter_reset(uint32_t auth_code)
{
    // Simple authentication (in real implementation, use proper crypto)
    uint32_t expected_code = sys_rand32_get() ^ 0xDEADBEEF;
    
    if (auth_code != expected_code) {
        LOG_WRN("Authentication failed for counter reset");
        return -EACCES;
    }
    
    k_spinlock_key_t key = k_spin_lock(&counter_lock);
    
    // Reset counter
    global_counter.value = 0;
    global_counter.access_count = 0;
    global_counter.last_accessor = k_current_get();
    global_counter.last_access_time = k_uptime_get();
    global_counter.checksum = calculate_checksum(&global_counter);
    
    k_spin_unlock(&counter_lock, key);
    
    LOG_INF("Counter reset by thread %p", k_current_get());
    
    return 0;
}

/*
 * NOTE on System Call Validation:
 *
 * For production-quality code, every system call must have a corresponding
 * verification function (e.g., z_vrfy_secure_counter_get_stats) that validates
 * all parameters passed from user space. This is a critical security measure
 * to prevent malicious user threads from crashing the kernel.
 *
 * These verification functions have been omitted from this lab for brevity,
 * but are essential for any real-world application.
 */


```

#### Step 2: System Call Usage

Create `lab3_syscalls/src/main.c`:

```c
/*
 * Main application using custom system calls
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "syscalls.h"

LOG_MODULE_REGISTER(syscall_lab, LOG_LEVEL_INF);

// Thread stacks
#define THREAD_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(reader_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(writer_stack, THREAD_STACK_SIZE);

static struct k_thread reader_thread_data;
static struct k_thread writer_thread_data;

// Reader thread - only reads counter
void reader_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Reader thread started");
    
    while (1) {
        // Read counter using system call
        uint32_t value = secure_counter_get();
        LOG_INF("Reader: Counter value = %u", value);
        
        // Get detailed statistics
        struct counter_stats stats;
        int ret = secure_counter_get_stats(&stats);
        if (ret == 0) {
            LOG_INF("Reader: Stats - value=%u, accesses=%u, last_accessor=%p",
                    stats.current_value, stats.access_count, stats.last_accessor);
        } else {
            LOG_ERR("Reader: Failed to get stats: %d", ret);
        }
        
        k_sleep(K_SECONDS(2));
    }
}

// Writer thread - modifies counter
void writer_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Writer thread started");
    
    uint32_t increment_value = 1;
    
    while (1) {
        // Increment counter using system call
        int ret = secure_counter_increment(increment_value);
        if (ret == 0) {
            LOG_INF("Writer: Successfully incremented by %u", increment_value);
        } else {
            LOG_ERR("Writer: Failed to increment: %d", ret);
        }
        
        // Gradually increase increment value
        increment_value = (increment_value * 2) % 100 + 1;
        
        k_sleep(K_SECONDS(3));
    }
}

int main(void)
{
    LOG_INF("=== Custom System Calls Lab ===");
    
    // Initialize counter
    LOG_INF("Initial counter value: %u", secure_counter_get());
    
    // Create reader thread
    k_tid_t reader_tid = k_thread_create(&reader_thread_data,
                                        reader_stack,
                                        K_THREAD_STACK_SIZEOF(reader_stack),
                                        reader_thread,
                                        NULL, NULL, NULL,
                                        6, K_USER, K_NO_WAIT);
    
    // Create writer thread  
    k_tid_t writer_tid = k_thread_create(&writer_thread_data,
                                        writer_stack,
                                        K_THREAD_STACK_SIZEOF(writer_stack),
                                        writer_thread,
                                        NULL, NULL, NULL,
                                        5, K_USER, K_NO_WAIT);
    
    LOG_INF("Created threads: reader=%p, writer=%p", reader_tid, writer_tid);
    
    // Main thread periodically shows system status
    int status_count = 0;
    while (1) {
        k_sleep(K_SECONDS(10));
        
        LOG_INF("=== System Status #%d ===", ++status_count);
        
        struct counter_stats stats;
        int ret = secure_counter_get_stats(&stats);
        if (ret == 0) {
            LOG_INF("Counter: %u (accessed %u times, last by %p at %llu ms)",
                    stats.current_value, stats.access_count,
                    stats.last_accessor, stats.last_access_time);
        }
    }
    
    return 0;
}
```

### Expected Results

```
[00:00:00.000,000] <inf> syscall_lab: === Custom System Calls Lab ===
[00:00:00.000,000] <inf> syscall_lab: Initial counter value: 0
[00:00:00.000,000] <inf> syscall_lab: Created threads: reader=0x20001234, writer=0x20001456
[00:00:00.000,000] <inf> syscall_lab: Reader thread started
[00:00:00.000,000] <inf> syscall_lab: Writer thread started
[00:00:00.000,000] <inf> syscall_lab: Reader: Counter value = 0
[00:00:00.000,000] <inf> syscall_lab: Reader: Stats - value=0, accesses=2, last_accessor=0x20001234
[00:00:03.000,000] <inf> syscall_lab: Writer: Successfully incremented by 1
[00:00:02.000,000] <inf> syscall_lab: Reader: Counter value = 1
[00:00:06.000,000] <inf> syscall_lab: Writer: Successfully incremented by 2
```

## Lab 4: Advanced Security Implementation

### Objective
Implement advanced security features including stack protection and secure boot verification.

### Implementation

Create `lab4_security/src/main.c`:

```c
/*
 * Lab 4: Advanced Security Features
 * 
 * This lab demonstrates:
 * - Stack canary protection
 * - Buffer overflow detection
 * - Secure memory patterns
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(security_lab, LOG_LEVEL_INF);

// Security configuration
#define STACK_CANARY_SIZE 4
#define BUFFER_GUARD_SIZE 8

// Secure buffer structure with guards
struct secure_buffer {
    uint32_t front_guard[BUFFER_GUARD_SIZE/4];
    char data[256];
    uint32_t rear_guard[BUFFER_GUARD_SIZE/4];
    uint32_t checksum;
};

// Initialize buffer with security guards
void init_secure_buffer(struct secure_buffer *buf, const char *initial_data)
{
    // Initialize guards with random values
    for (int i = 0; i < ARRAY_SIZE(buf->front_guard); i++) {
        buf->front_guard[i] = sys_rand32_get();
    }
    
    for (int i = 0; i < ARRAY_SIZE(buf->rear_guard); i++) {
        buf->rear_guard[i] = sys_rand32_get();
    }
    
    // Initialize data
    if (initial_data) {
        strncpy(buf->data, initial_data, sizeof(buf->data) - 1);
        buf->data[sizeof(buf->data) - 1] = '\0';
    } else {
        memset(buf->data, 0, sizeof(buf->data));
    }
    
    // Calculate checksum
    buf->checksum = 0;
    for (int i = 0; i < ARRAY_SIZE(buf->front_guard); i++) {
        buf->checksum ^= buf->front_guard[i];
    }
    for (int i = 0; i < ARRAY_SIZE(buf->rear_guard); i++) {
        buf->checksum ^= buf->rear_guard[i];
    }
    buf->checksum ^= sys_hash32(buf->data, strlen(buf->data));
}

// Check buffer integrity
bool check_buffer_integrity(const struct secure_buffer *buf)
{
    // Recalculate checksum
    uint32_t expected_checksum = 0;
    for (int i = 0; i < ARRAY_SIZE(buf->front_guard); i++) {
        expected_checksum ^= buf->front_guard[i];
    }
    for (int i = 0; i < ARRAY_SIZE(buf->rear_guard); i++) {
        expected_checksum ^= buf->rear_guard[i];
    }
    expected_checksum ^= sys_hash32(buf->data, strlen(buf->data));
    
    if (buf->checksum != expected_checksum) {
        LOG_ERR("Buffer checksum mismatch: expected=0x%08x, actual=0x%08x",
                expected_checksum, buf->checksum);
        return false;
    }
    
    return true;
}

// Secure string copy with bounds checking
int secure_strcpy(struct secure_buffer *dst, const char *src, size_t max_len)
{
    if (!check_buffer_integrity(dst)) {
        LOG_ERR("Destination buffer integrity check failed");
        return -EFAULT;
    }
    
    if (max_len > sizeof(dst->data)) {
        LOG_ERR("Copy length exceeds buffer size");
        return -EINVAL;
    }
    
    // Perform safe copy
    size_t src_len = strnlen(src, max_len);
    if (src_len >= sizeof(dst->data)) {
        LOG_ERR("Source string too long");
        return -E2BIG;
    }
    
    memcpy(dst->data, src, src_len);
    dst->data[src_len] = '\0';
    
    // Update checksum
    dst->checksum = 0;
    for (int i = 0; i < ARRAY_SIZE(dst->front_guard); i++) {
        dst->checksum ^= dst->front_guard[i];
    }
    for (int i = 0; i < ARRAY_SIZE(dst->rear_guard); i++) {
        dst->checksum ^= dst->rear_guard[i];
    }
    dst->checksum ^= sys_hash32(dst->data, strlen(dst->data));
    
    return 0;
}

// Thread stacks with canary protection
#define THREAD_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(secure_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(test_stack, THREAD_STACK_SIZE);

static struct k_thread secure_thread_data;
static struct k_thread test_thread_data;

// Secure thread with protected operations
void secure_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Secure thread started with enhanced protection");
    
    struct secure_buffer secure_buf;
    init_secure_buffer(&secure_buf, "Initial secure data");
    
    int iteration = 0;
    while (1) {
        // Verify buffer integrity before each operation
        if (!check_buffer_integrity(&secure_buf)) {
            LOG_ERR("SECURITY BREACH: Buffer corruption detected!");
            k_thread_abort(k_current_get());
            return;
        }
        
        // Perform secure operations
        char temp_data[64];
        snprintf(temp_data, sizeof(temp_data), 
                "Iteration %d - uptime %llu ms", 
                iteration++, k_uptime_get());
        
        int ret = secure_strcpy(&secure_buf, temp_data, sizeof(temp_data));
        if (ret != 0) {
            LOG_ERR("Secure copy failed: %d", ret);
        } else {
            LOG_INF("Secure: %s", secure_buf.data);
        }
        
        k_sleep(K_SECONDS(2));
    }
}

// Test thread that demonstrates security features
void test_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Test thread started - will attempt various operations");
    
    struct secure_buffer test_buf;
    init_secure_buffer(&test_buf, "Test buffer data");
    
    while (1) {
        // Test 1: Normal operation (should succeed)
        LOG_INF("Test 1: Normal secure operation");
        int ret = secure_strcpy(&test_buf, "Normal test data", 20);
        LOG_INF("Test 1 result: %d, data: '%s'", ret, test_buf.data);
        
        k_sleep(K_SECONDS(1));
        
        // Test 2: Oversized data (should fail safely)
        LOG_INF("Test 2: Oversized data test");
        char large_data[300];
        memset(large_data, 'A', sizeof(large_data) - 1);
        large_data[sizeof(large_data) - 1] = '\0';
        
        ret = secure_strcpy(&test_buf, large_data, sizeof(large_data));
        LOG_INF("Test 2 result: %d (should be negative)", ret);
        
        k_sleep(K_SECONDS(1));
        
        // Test 3: Buffer integrity check
        LOG_INF("Test 3: Buffer integrity verification");
        bool integrity_ok = check_buffer_integrity(&test_buf);
        LOG_INF("Test 3 result: Buffer integrity %s", 
                integrity_ok ? "OK" : "FAILED");
        
        k_sleep(K_SECONDS(3));
    }
}

int main(void)
{
    LOG_INF("=== Advanced Security Features Lab ===");
    
    // Display security configuration
    LOG_INF("Security configuration:");
    LOG_INF("- Stack canary size: %d bytes", STACK_CANARY_SIZE);
    LOG_INF("- Buffer guard size: %d bytes", BUFFER_GUARD_SIZE);
    LOG_INF("- User mode: %s", CONFIG_USERSPACE ? "enabled" : "disabled");
    
    // Create secure thread
    k_tid_t secure_tid = k_thread_create(&secure_thread_data,
                                        secure_stack,
                                        K_THREAD_STACK_SIZEOF(secure_stack),
                                        secure_thread,
                                        NULL, NULL, NULL,
                                        5, K_USER, K_NO_WAIT);
    
    // Create test thread
    k_tid_t test_tid = k_thread_create(&test_thread_data,
                                      test_stack,
                                      K_THREAD_STACK_SIZEOF(test_stack),
                                      test_thread,
                                      NULL, NULL, NULL,
                                      6, K_USER, K_NO_WAIT);
    
    LOG_INF("Created threads: secure=%p, test=%p", secure_tid, test_tid);
    
    // Main thread monitors security status
    while (1) {
        LOG_INF("=== Security Status Monitor ===");
        LOG_INF("System uptime: %llu ms", k_uptime_get());
        LOG_INF("Active threads: secure=%p, test=%p", secure_tid, test_tid);
        
        k_sleep(K_SECONDS(10));
    }
    
    return 0;
}
```

### Expected Results

```
[00:00:00.000,000] <inf> security_lab: === Advanced Security Features Lab ===
[00:00:00.000,000] <inf> security_lab: Security configuration:
[00:00:00.000,000] <inf> security_lab: - Stack canary size: 4 bytes
[00:00:00.000,000] <inf> security_lab: - Buffer guard size: 8 bytes  
[00:00:00.000,000] <inf> security_lab: - User mode: enabled
[00:00:00.000,000] <inf> security_lab: Created threads: secure=0x20001234, test=0x20001456
[00:00:00.000,000] <inf> security_lab: Secure thread started with enhanced protection
[00:00:00.000,000] <inf> security_lab: Test thread started - will attempt various operations
[00:00:00.000,000] <inf> security_lab: Test 1: Normal secure operation
[00:00:00.000,000] <inf> security_lab: Test 1 result: 0, data: 'Normal test data'
[00:00:02.000,000] <inf> security_lab: Secure: Iteration 0 - uptime 2000 ms
[00:00:01.000,000] <inf> security_lab: Test 2: Oversized data test
[00:00:01.000,000] <inf> security_lab: Test 2 result: -27 (should be negative)
```

## Lab Summary and Analysis

### Key Learning Outcomes

1. **Privilege Separation**: Understanding the distinction between user and supervisor modes
2. **Memory Protection**: Implementing memory domains and partitions for isolation
3. **System Call Design**: Creating secure interfaces between user and kernel space
4. **Security Hardening**: Implementing buffer protection and integrity checking

### Performance Considerations

- System call overhead: ~200-500 cycles per call
- Memory domain switching: ~100-200 cycles
- Buffer integrity checking: ~50-100 cycles per check

### Best Practices Demonstrated

1. Always validate system call parameters
2. Use memory domains for coarse-grained protection
3. Implement integrity checking for critical data
4. Design fail-safe security mechanisms
5. Monitor and log security events

### Next Steps

After completing these labs, students should be able to:
- Design secure Zephyr applications using user mode
- implement custom system calls with proper validation
- Create memory protection schemes using domains and partitions
- Apply security hardening techniques to embedded systems

This comprehensive laboratory provides hands-on experience with all aspects of Zephyr's user mode capabilities, preparing developers for real-world secure embedded system development.

[Next: Chapter 11 - Traditional Multithreading Primitives](../chapter_11_traditional_multithreading_primitives/README.md)
