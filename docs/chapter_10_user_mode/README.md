# Chapter 10: User Mode in Zephyr RTOS

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

## Building on Memory Management Foundations

Having mastered Zephyr's memory management capabilities in Chapter 9—including heap allocation, thread stacks, and memory protection mechanisms—you now possess the foundational knowledge necessary to implement advanced security architectures. User mode represents the evolution of basic memory management into comprehensive system security, transforming the memory domains and protection concepts you've learned into robust isolation boundaries for production embedded systems.

## Overview

User mode represents a fundamental security and stability feature in modern embedded systems, providing essential process isolation and privilege separation. This chapter explores Zephyr's comprehensive user mode implementation, building directly upon the memory management concepts from Chapter 9 to create secure, isolated application environments with proper privilege boundaries.

## Learning Objectives

Upon completion of this chapter, you will be able to:

- **Implement User Mode Applications**: Create secure, isolated user applications with proper privilege separation
- **Configure Memory Domains**: Design and implement memory partitions for application isolation
- **Manage Thread Permissions**: Control thread access rights and privilege levels effectively
- **Utilize System Calls**: Implement controlled kernel service access from user space
- **Apply Security Principles**: Design robust, security-conscious embedded applications
- **Debug User Mode Issues**: Identify and resolve user mode configuration and runtime problems
- **Architect Secure Systems**: Build production-ready applications with proper isolation boundaries

## Introduction to User Mode Security

### From Memory Management to System Security

In Chapter 9, you learned to manage memory through heap allocation, thread stacks, and basic memory protection. However, these techniques operate within a single trust domain where all code runs with identical privileges. As embedded systems become more complex and connected, this "all-or-nothing" approach creates critical vulnerabilities.

User mode evolution builds upon your memory management skills by adding **privilege separation**—transforming the memory domains you understand into security boundaries that isolate trusted kernel operations from potentially untrusted application code.

### The Security Imperative in Embedded Systems

Modern embedded systems face unprecedented security challenges that basic memory management alone cannot address. The memory allocation strategies you mastered in Chapter 9 work excellently for resource management, but they assume all code can be trusted equally. User mode provides the next security layer by establishing clear boundaries between trusted kernel operations and potentially untrusted application code.

#### Real-World Security Scenarios

**Industrial Control Systems**
- Safety-critical control algorithms must be isolated from diagnostic software
- User mode prevents diagnostic code from corrupting control loops
- Memory protection ensures process control integrity

**IoT Device Security**
- Connected devices process untrusted network data
- User mode isolates protocol handlers from system services
- Failed network parsing cannot compromise device operation

**Automotive Applications**
- Infotainment systems must not interfere with vehicle control
- User mode ensures entertainment failures don't affect safety systems
- Secure boot verification runs in protected kernel space

### Zephyr's User Mode Architecture

Zephyr implements user mode through a comprehensive security model:

#### Hardware Memory Protection

```text
┌─────────────────────────────────────────────────────┐
│                 Kernel Space                        │
│               (Privileged Mode)                     │
├─────────────────────────────────────────────────────┤
│  • System Services     • Device Drivers            │
│  • Memory Management   • Interrupt Handlers        │
│  • Scheduler          • Hardware Access            │
├─────────────────────────────────────────────────────┤
│                   MPU/MMU                          │
│              Hardware Protection                    │
├─────────────────────────────────────────────────────┤
│                 User Space                          │
│             (Unprivileged Mode)                     │
├─────────────────────────────────────────────────────┤
│  • Application Code    • User Libraries            │
│  • User Data          • Application Buffers        │
│  • Stack Space        • Heap Allocations          │
└─────────────────────────────────────────────────────┘
```

#### Privilege Separation Model

**Kernel Mode Capabilities:**
- Direct hardware access
- Memory management control
- Interrupt handling
- System resource allocation
- Security policy enforcement

**User Mode Restrictions:**
- Limited memory access through domains
- No direct hardware access
- System calls for kernel services
- Controlled resource allocation
- Enforced security boundaries

## Memory Domains and Protection

### Memory Domain Architecture

Memory domains provide the foundation for user mode security by creating isolated memory spaces for different applications or security contexts.

#### Domain Structure and Organization

```c
// Memory domain represents an isolated memory space
struct k_mem_domain {
    // Architecture-specific data (page tables, etc.)
    struct arch_mem_domain arch;
    
    // Array of memory partitions within this domain
    struct k_mem_partition partitions[CONFIG_MAX_DOMAIN_PARTITIONS];
    
    // List of threads belonging to this domain
    sys_dlist_t mem_domain_q;
    
    // Number of active partitions
    uint8_t num_partitions;
};
```

#### Memory Partition Definition

Memory partitions define specific memory regions with associated access permissions:

```c
// Define a user data partition
K_MEM_PARTITION_DEFINE(user_data_partition,
                      user_data_start,
                      user_data_size,
                      K_MEM_PARTITION_P_RW_U_RW);

// Define a read-only configuration partition
K_MEM_PARTITION_DEFINE(config_partition,
                      config_data_start,
                      config_data_size,
                      K_MEM_PARTITION_P_RO_U_RO);

// Define an executable code partition
K_MEM_PARTITION_DEFINE(user_code_partition,
                      user_code_start,
                      user_code_size,
                      K_MEM_PARTITION_P_RX_U_RX);
```

#### Permission Attributes

Zephyr supports fine-grained permission control:

```c
// Permission attribute definitions
#define K_MEM_PARTITION_P_RW_U_RW    // Kernel R/W, User R/W
#define K_MEM_PARTITION_P_RW_U_RO    // Kernel R/W, User R/O
#define K_MEM_PARTITION_P_RO_U_RO    // Kernel R/O, User R/O
#define K_MEM_PARTITION_P_RX_U_RX    // Kernel R/X, User R/X
#define K_MEM_PARTITION_P_RW_U_NA    // Kernel R/W, User No Access
```

### Domain Creation and Management

#### Basic Domain Setup

```c
// Application-specific memory partitions
static struct k_mem_partition *app_partitions[] = {
    &user_data_partition,
    &config_partition,
    &user_code_partition
};

// Create memory domain
static struct k_mem_domain app_domain;

void setup_application_domain(void)
{
    int ret;
    
    // Initialize memory domain with partitions
    ret = k_mem_domain_init(&app_domain, 
                           ARRAY_SIZE(app_partitions), 
                           app_partitions);
    
    if (ret != 0) {
        LOG_ERR("Failed to initialize memory domain: %d", ret);
        return;
    }
    
    LOG_INF("Application domain initialized with %zu partitions", 
            ARRAY_SIZE(app_partitions));
}
```

#### Dynamic Partition Management

```c
// Dynamically add partition to existing domain
void add_runtime_partition(void)
{
    // Define runtime partition
    static struct k_mem_partition runtime_partition = {
        .start = runtime_memory_start,
        .size = runtime_memory_size,
        .attr = K_MEM_PARTITION_P_RW_U_RW
    };
    
    int ret = k_mem_domain_add_partition(&app_domain, &runtime_partition);
    if (ret != 0) {
        LOG_ERR("Failed to add runtime partition: %d", ret);
        return;
    }
    
    LOG_INF("Runtime partition added successfully");
}

// Remove partition from domain
void remove_partition_example(void)
{
    int ret = k_mem_domain_remove_partition(&app_domain, &runtime_partition);
    if (ret != 0) {
        LOG_ERR("Failed to remove partition: %d", ret);
    }
}
```

## User Mode Thread Management

### Creating User Mode Threads

User mode threads operate with restricted privileges and memory access:

#### Basic User Thread Creation

```c
// User thread stack - must be properly sized
K_THREAD_STACK_DEFINE(user_thread_stack, 2048);
static struct k_thread user_thread_data;

// User thread entry point
void user_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("User thread started with restricted privileges");
    
    // User thread operations are restricted by memory domain
    while (1) {
        // Perform user application logic
        user_application_task();
        
        // Use system calls for kernel services
        k_sleep(K_MSEC(1000));
    }
}

void create_user_thread(void)
{
    // Create thread with K_USER flag for user mode
    k_tid_t user_tid = k_thread_create(&user_thread_data,
                                      user_thread_stack,
                                      K_THREAD_STACK_SIZEOF(user_thread_stack),
                                      user_thread_entry,
                                      NULL, NULL, NULL,
                                      K_PRIO_PREEMPT(10),
                                      K_USER,  // User mode flag
                                      K_NO_WAIT);
    
    // Add thread to memory domain
    int ret = k_mem_domain_add_thread(&app_domain, user_tid);
    if (ret != 0) {
        LOG_ERR("Failed to add thread to domain: %d", ret);
        k_thread_abort(user_tid);
        return;
    }
    
    k_thread_name_set(user_tid, "user_app");
    LOG_INF("User thread created and added to domain");
}
```

### Thread Permission Management

#### Thread Domain Assignment

```c
// Move thread between domains
void reassign_thread_domain(k_tid_t thread, struct k_mem_domain *new_domain)
{
    int ret = k_mem_domain_add_thread(new_domain, thread);
    if (ret != 0) {
        LOG_ERR("Failed to reassign thread to new domain: %d", ret);
        return;
    }
    
    LOG_INF("Thread reassigned to new memory domain");
}

// Remove thread from domain (returns to default domain)
void remove_from_domain(k_tid_t thread)
{
    // Adding to default domain removes from current domain
    int ret = k_mem_domain_add_thread(&k_mem_domain_default, thread);
    if (ret != 0) {
        LOG_ERR("Failed to return thread to default domain: %d", ret);
    }
}
```

## System Call Interface

### Understanding System Calls

System calls provide controlled access to kernel services from user mode threads:

#### System Call Mechanism

```c
// System calls are automatically generated for functions marked with __syscall
__syscall int custom_kernel_service(int param1, const char *param2);

// Implementation in kernel space
int z_impl_custom_kernel_service(int param1, const char *param2)
{
    // Kernel implementation with full privileges
    LOG_INF("Kernel service called with param1=%d, param2=%s", param1, param2);
    
    // Perform privileged operations
    return perform_kernel_operation(param1, param2);
}

// Verification function for parameter validation
static inline int z_vrfy_custom_kernel_service(int param1, const char *param2)
{
    // Validate user parameters
    if (param1 < 0) {
        return -EINVAL;
    }
    
    // Validate string parameter from user space
    if (param2 != NULL) {
        int err;
        size_t len = k_usermode_string_nlen(param2, 256, &err);
        if (err != 0) {
            return -EFAULT;
        }
        // Additional validation: ensure we can read the actual string length
        if (K_SYSCALL_MEMORY_READ(param2, len + 1) != 0) {
            return -EFAULT;
        }
    }
    
    // Call actual implementation
    return z_impl_custom_kernel_service(param1, param2);
}

#include <zephyr/syscalls/custom_kernel_service_mrsh.c>
```

#### Common System Call Patterns

```c
// Memory allocation system call usage
void user_memory_example(void)
{
    // Allocate memory through system call
    void *user_buffer = k_malloc(1024);
    if (user_buffer == NULL) {
        LOG_ERR("Memory allocation failed");
        return;
    }
    
    // Use allocated memory
    memset(user_buffer, 0, 1024);
    strcpy(user_buffer, "User data");
    
    // Free memory through system call
    k_free(user_buffer);
}

// Thread management system calls
void user_thread_control(void)
{
    // Sleep using system call
    k_sleep(K_MSEC(100));
    
    // Yield to other threads
    k_yield();
    
    // Get current thread priority
    int priority = k_thread_priority_get(k_current_get());
    LOG_INF("Current thread priority: %d", priority);
}
```

## Advanced User Mode Patterns

### Multi-Domain Applications

Create applications with multiple security domains:

```c
// Security domain for cryptographic operations
K_MEM_PARTITION_DEFINE(crypto_partition,
                      crypto_memory_start,
                      crypto_memory_size,
                      K_MEM_PARTITION_P_RW_U_NA);  // Kernel only

static struct k_mem_partition *crypto_partitions[] = {
    &crypto_partition
};

static struct k_mem_domain crypto_domain;

// Network processing domain
K_MEM_PARTITION_DEFINE(network_partition,
                      network_buffer_start,
                      network_buffer_size,
                      K_MEM_PARTITION_P_RW_U_RW);

static struct k_mem_partition *network_partitions[] = {
    &network_partition
};

static struct k_mem_domain network_domain;

void setup_multi_domain_system(void)
{
    // Initialize crypto domain (high security)
    k_mem_domain_init(&crypto_domain,
                     ARRAY_SIZE(crypto_partitions),
                     crypto_partitions);
    
    // Initialize network domain (controlled access)
    k_mem_domain_init(&network_domain,
                     ARRAY_SIZE(network_partitions),
                     network_partitions);
    
    LOG_INF("Multi-domain system initialized");
}
```

### Secure Communication Channels

```c
// Shared memory for inter-domain communication
K_MEM_PARTITION_DEFINE(shared_partition,
                      shared_memory_start,
                      shared_memory_size,
                      K_MEM_PARTITION_P_RW_U_RW);

// Message passing between domains
struct domain_message {
    uint32_t msg_type;
    uint32_t data_length;
    uint8_t data[MAX_MESSAGE_SIZE];
    k_tid_t sender_thread;
};

// Secure message queue between domains
K_MSGQ_DEFINE(inter_domain_msgq, sizeof(struct domain_message), 8, 4);

// Send message from user domain
int send_secure_message(uint32_t msg_type, const void *data, size_t length)
{
    if (length > MAX_MESSAGE_SIZE) {
        return -EMSGSIZE;
    }
    
    struct domain_message msg = {
        .msg_type = msg_type,
        .data_length = length,
        .sender_thread = k_current_get()
    };
    
    if (data != NULL && length > 0) {
        memcpy(msg.data, data, length);
    }
    
    return k_msgq_put(&inter_domain_msgq, &msg, K_NO_WAIT);
}
```

## Performance and Optimization

### Memory Domain Optimization

#### Efficient Partition Layout

```c
// Optimize partition alignment for hardware efficiency
#define PARTITION_ALIGN 1024  // Align to hardware requirements

// Calculate aligned partition sizes
#define ALIGNED_SIZE(size) ROUND_UP(size, PARTITION_ALIGN)
#define ALIGNED_START(addr) ROUND_UP(addr, PARTITION_ALIGN)

// Efficient partition definitions
K_MEM_PARTITION_DEFINE(optimized_partition,
                      ALIGNED_START(base_address),
                      ALIGNED_SIZE(required_size),
                      K_MEM_PARTITION_P_RW_U_RW);
```

#### System Call Optimization

```c
// Batch system calls to reduce overhead
void optimized_user_operations(void)
{
    // Group related operations
    void *buffers[4];
    
    // Allocate multiple buffers in sequence
    for (int i = 0; i < 4; i++) {
        buffers[i] = k_malloc(256);
        if (buffers[i] == NULL) {
            // Clean up partial allocations
            for (int j = 0; j < i; j++) {
                k_free(buffers[j]);
            }
            return;
        }
    }
    
    // Perform batch operations
    process_multiple_buffers(buffers, 4);
    
    // Clean up all buffers
    for (int i = 0; i < 4; i++) {
        k_free(buffers[i]);
    }
}
```

## Security Best Practices

### Secure Application Design

#### Input Validation

```c
// Always validate user inputs in system calls
__syscall int secure_string_process(const char *user_string, size_t max_len);

static inline int z_vrfy_secure_string_process(const char *user_string, 
                                              size_t max_len)
{
    // Validate string is accessible and null-terminated
    int err;
    size_t actual_len = k_usermode_string_nlen(user_string, max_len, &err);
    if (err != 0) {
        return -EFAULT;  // Invalid user space pointer
    }
    
    // Additional validation
    if (actual_len == 0) {
        return -EINVAL;  // Empty string not allowed
    }
    
    return z_impl_secure_string_process(user_string, max_len);
}
```

#### Resource Management

```c
// Track user space resource usage
struct user_resource_tracker {
    atomic_t allocated_memory;
    atomic_t open_files;
    atomic_t active_timers;
    k_tid_t owner_thread;
};

static struct user_resource_tracker user_resources[CONFIG_MAX_USER_THREADS];

// Enforce resource limits
bool check_resource_limit(k_tid_t thread, enum resource_type type)
{
    struct user_resource_tracker *tracker = get_thread_tracker(thread);
    
    switch (type) {
    case RESOURCE_MEMORY:
        return atomic_get(&tracker->allocated_memory) < MAX_USER_MEMORY;
    case RESOURCE_FILES:
        return atomic_get(&tracker->open_files) < MAX_USER_FILES;
    case RESOURCE_TIMERS:
        return atomic_get(&tracker->active_timers) < MAX_USER_TIMERS;
    default:
        return false;
    }
}
```

### Error Handling and Recovery

```c
// Graceful handling of user mode violations
void user_mode_fault_handler(const struct arch_esf *esf)
{
    k_tid_t faulting_thread = k_current_get();
    
    LOG_ERR("User mode violation in thread %p", faulting_thread);
    LOG_ERR("Fault address: 0x%08x", arch_esf_get_fault_address(esf));
    
    // Log thread information for debugging
    const char *thread_name = k_thread_name_get(faulting_thread);
    if (thread_name != NULL) {
        LOG_ERR("Thread name: %s", thread_name);
    }
    
    // Clean up thread resources
    cleanup_thread_resources(faulting_thread);
    
    // Terminate the faulting thread
    k_thread_abort(faulting_thread);
}
```

This comprehensive introduction establishes the foundation for secure user mode programming in Zephyr RTOS. The following sections will provide hands-on implementation guidance and practical examples.

[Next: User Mode Theory](./theory.md)
