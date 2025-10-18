# Chapter 11: Traditional Multithreading Primitives - Theory and Implementation

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Fundamental Synchronization Concepts

### Understanding Concurrency in Embedded Systems

Concurrency in embedded systems presents unique challenges compared to general-purpose computing environments. The constraints of limited memory, deterministic timing requirements, and direct hardware interaction necessitate careful consideration of synchronization mechanisms.

#### The Challenge of Shared Resources

```c
/*
 * Example: Shared resource without synchronization (PROBLEMATIC)
 *
 * This example demonstrates a classic race condition where multiple
 * threads access shared data without proper synchronization.
 */

static volatile uint32_t shared_counter = 0;
static volatile bool system_ready = false;

// Thread 1: Producer
void producer_thread(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        // RACE CONDITION: Non-atomic read-modify-write
        shared_counter++;  // Multiple instructions: load, increment, store

        if (shared_counter >= 100) {
            system_ready = true;  // Another potential race condition
        }

        k_msleep(10);
    }
}

// Thread 2: Consumer
void consumer_thread(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        if (system_ready) {
            // RACE CONDITION: Value may change between check and use
            printk("Counter value: %u\n", shared_counter);
            shared_counter = 0;  // Reset counter
            system_ready = false;
        }
        
        k_msleep(50);
    }
}
```

#### Critical Sections and Atomic Operations

A critical section is a code segment that accesses shared resources and must not be executed concurrently by multiple threads. Zephyr provides several mechanisms to protect critical sections:

```c
/*
 * Critical Section Protection Methods in Zephyr
 */

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/irq.h>

// Method 1: Interrupt disable/enable (single-core systems)
void critical_section_irq_disable(void)
{
    unsigned int key = irq_lock();
    
    // Critical section - interrupts disabled
    shared_counter++;
    if (shared_counter >= 100) {
        system_ready = true;
    }
    
    irq_unlock(key);
}

// Method 2: Spinlocks (SMP-safe)
static struct k_spinlock data_lock;

void critical_section_spinlock(void)
{
    k_spinlock_key_t key = k_spin_lock(&data_lock);
    
    // Critical section - SMP-safe protection
    shared_counter++;
    if (shared_counter >= 100) {
        system_ready = true;
    }
    
    k_spin_unlock(&data_lock, key);
}

// Method 3: Atomic operations (when applicable)
void atomic_operations_example(void)
{
    // Atomic increment
    uint32_t old_value = atomic_inc(&shared_counter);
    
    // Atomic compare-and-swap
    bool success = atomic_cas(&system_ready, false, true);
    
    // Atomic test-and-set
    atomic_set_bit(&status_flags, READY_FLAG_BIT);
}
```

## Mutex Implementation and Theory

### Mutex Architecture in Zephyr

Mutexes (mutual exclusion objects) provide the primary mechanism for protecting shared resources in Zephyr applications. Zephyr's mutex implementation includes advanced features like priority inheritance and recursive locking support.

#### Basic Mutex Operations

```c
/*
 * Comprehensive Mutex Usage Example
 * 
 * This example demonstrates proper mutex usage patterns,
 * error handling, and performance considerations.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mutex_example, LOG_LEVEL_DBG);

// Shared data structure requiring protection
struct shared_resource {
    uint32_t value;
    uint64_t timestamp;
    char description[64];
    uint32_t access_count;
};

static struct shared_resource global_resource = {0};
static K_MUTEX_DEFINE(resource_mutex);

// Safe resource access with comprehensive error handling
int safe_resource_update(uint32_t new_value, const char *desc)
{
    int ret;
    
    // Attempt to acquire mutex with timeout
    ret = k_mutex_lock(&resource_mutex, K_MSEC(1000));
    if (ret != 0) {
        LOG_ERR("Failed to acquire mutex: %d", ret);
        return ret;
    }
    
    // Critical section - exclusive access guaranteed
    LOG_DBG("Updating resource: old_value=%u, new_value=%u", 
            global_resource.value, new_value);
    
    global_resource.value = new_value;
    global_resource.timestamp = k_uptime_get();
    global_resource.access_count++;
    
    if (desc != NULL) {
        strncpy(global_resource.description, desc, 
                sizeof(global_resource.description) - 1);
        global_resource.description[sizeof(global_resource.description) - 1] = '\0';
    }
    
    // Release mutex
    ret = k_mutex_unlock(&resource_mutex);
    if (ret != 0) {
        LOG_ERR("Failed to release mutex: %d", ret);
    }
    
    LOG_DBG("Resource updated successfully, access_count=%u", 
            global_resource.access_count);
    
    return ret;
}

// Safe resource read with minimal locking time
int safe_resource_read(struct shared_resource *output)
{
    int ret;
    
    if (output == NULL) {
        return -EINVAL;
    }
    
    ret = k_mutex_lock(&resource_mutex, K_MSEC(500));
    if (ret != 0) {
        LOG_WRN("Read operation timed out");
        return ret;
    }
    
    // Fast copy to minimize lock time
    memcpy(output, &global_resource, sizeof(*output));
    
    k_mutex_unlock(&resource_mutex);
    
    LOG_DBG("Resource read: value=%u, timestamp=%llu", 
            output->value, output->timestamp);
    
    return 0;
}
```

#### Priority Inheritance Protocol

Zephyr implements priority inheritance to prevent priority inversion scenarios:

```c
/*
 * Priority Inheritance Demonstration
 * 
 * This example shows how Zephyr's priority inheritance prevents
 * priority inversion in complex scheduling scenarios.
 */

static K_MUTEX_DEFINE(shared_mutex);
static struct shared_resource critical_data;

// High priority thread
void high_priority_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("High priority thread starting (priority: %d)", 
            k_thread_priority_get(k_current_get()));
    
    k_msleep(100); // Allow other threads to start
    
    LOG_INF("High priority thread requesting mutex");
    int ret = k_mutex_lock(&shared_mutex, K_FOREVER);
    
    if (ret == 0) {
        LOG_INF("High priority thread acquired mutex");
        
        // Simulate work in critical section
        k_msleep(50);
        
        LOG_INF("High priority thread completed work");
        k_mutex_unlock(&shared_mutex);
    }
}

// Medium priority thread (potential priority inversion candidate)
void medium_priority_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Medium priority thread starting (priority: %d)", 
            k_thread_priority_get(k_current_get()));
    
    k_msleep(50); // Start after low priority acquires mutex
    
    // CPU-intensive work that could cause priority inversion
    // without priority inheritance
    LOG_INF("Medium priority thread doing intensive work");
    
    for (volatile int i = 0; i < 1000000; i++) {
        // Busy work to consume CPU
    }
    
    LOG_INF("Medium priority thread completed work");
}

// Low priority thread
void low_priority_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Low priority thread starting (priority: %d)", 
            k_thread_priority_get(k_current_get()));
    
    int ret = k_mutex_lock(&shared_mutex, K_FOREVER);
    
    if (ret == 0) {
        LOG_INF("Low priority thread acquired mutex");
        
        // Long operation in critical section
        // During this time, if high priority thread requests the mutex,
        // priority inheritance will boost this thread's priority
        LOG_INF("Low priority thread doing extended work");
        k_msleep(200);
        
        LOG_INF("Low priority thread releasing mutex");
        k_mutex_unlock(&shared_mutex);
    }
}

// Thread creation with priority inheritance test
void create_priority_test_threads(void)
{
    static struct k_thread high_thread, medium_thread, low_thread;
    static K_THREAD_STACK_DEFINE(high_stack, 1024);
    static K_THREAD_STACK_DEFINE(medium_stack, 1024);
    static K_THREAD_STACK_DEFINE(low_stack, 1024);
    
    // Create threads with different priorities
    k_thread_create(&low_thread, low_stack, K_THREAD_STACK_SIZEOF(low_stack),
                   low_priority_thread, NULL, NULL, NULL,
                   10, 0, K_NO_WAIT);  // Lowest priority
    
    k_thread_create(&medium_thread, medium_stack, K_THREAD_STACK_SIZEOF(medium_stack),
                   medium_priority_thread, NULL, NULL, NULL,
                   5, 0, K_NO_WAIT);   // Medium priority
    
    k_thread_create(&high_thread, high_stack, K_THREAD_STACK_SIZEOF(high_stack),
                   high_priority_thread, NULL, NULL, NULL,
                   1, 0, K_NO_WAIT);   // Highest priority
}
```

### Advanced Mutex Patterns

#### Scoped Locking and RAII Pattern

```c
/*
 * RAII-style Mutex Management in C
 * 
 * While C doesn't have destructors, we can implement
 * scope-based locking using function wrappers.
 */

// Scoped mutex lock structure
struct scoped_mutex_lock {
    struct k_mutex *mutex;
    bool acquired;
    k_timeout_t timeout;
};

// Initialize scoped lock
static inline struct scoped_mutex_lock 
scoped_mutex_init(struct k_mutex *mutex, k_timeout_t timeout)
{
    struct scoped_mutex_lock lock = {
        .mutex = mutex,
        .acquired = false,
        .timeout = timeout
    };
    
    if (k_mutex_lock(mutex, timeout) == 0) {
        lock.acquired = true;
    }
    
    return lock;
}

// Release scoped lock
static inline void scoped_mutex_release(struct scoped_mutex_lock *lock)
{
    if (lock->acquired) {
        k_mutex_unlock(lock->mutex);
        lock->acquired = false;
    }
}

// Macro for automatic scope-based locking
#define WITH_MUTEX_LOCK(mutex, timeout) \
    for (struct scoped_mutex_lock __lock = scoped_mutex_init(mutex, timeout); \
         __lock.acquired; \
         scoped_mutex_release(&__lock), __lock.acquired = false)

// Usage example
void scoped_mutex_example(void)
{
    WITH_MUTEX_LOCK(&resource_mutex, K_MSEC(1000)) {
        // Critical section - mutex automatically released on scope exit
        global_resource.value++;
        global_resource.timestamp = k_uptime_get();
        
        // Even early returns or breaks will release the mutex
        if (global_resource.value > 1000) {
            LOG_WRN("Resource value too high, resetting");
            global_resource.value = 0;
            return; // Mutex automatically released
        }
    }
    // Mutex automatically released here
}
```

#### Reader-Writer Lock Implementation

```c
/*
 * Reader-Writer Lock Implementation using Zephyr Primitives
 * 
 * Allows multiple readers or one writer, but not both simultaneously.
 */

struct k_rwlock {
    struct k_mutex writer_mutex;    // Protects writer access
    struct k_sem reader_sem;        // Limits concurrent readers
    atomic_t reader_count;          // Current number of readers
    atomic_t writer_waiting;        // Writer waiting flag
};

#define K_RWLOCK_DEFINE(name) \
    struct k_rwlock name = { \
        .writer_mutex = Z_MUTEX_INITIALIZER(name.writer_mutex), \
        .reader_sem = Z_SEM_INITIALIZER(name.reader_sem, 1, 1), \
        .reader_count = ATOMIC_INIT(0), \
        .writer_waiting = ATOMIC_INIT(0) \
    }

// Acquire read lock
int k_rwlock_read_lock(struct k_rwlock *rwlock, k_timeout_t timeout)
{
    int ret;
    
    // Wait if writer is waiting or active
    ret = k_sem_take(&rwlock->reader_sem, timeout);
    if (ret != 0) {
        return ret;
    }
    
    // Increment reader count
    int readers = atomic_inc(&rwlock->reader_count);
    
    // First reader blocks writers
    if (readers == 0) {
        ret = k_mutex_lock(&rwlock->writer_mutex, timeout);
        if (ret != 0) {
            atomic_dec(&rwlock->reader_count);
            k_sem_give(&rwlock->reader_sem);
            return ret;
        }
    }
    
    k_sem_give(&rwlock->reader_sem);
    return 0;
}

// Release read lock
void k_rwlock_read_unlock(struct k_rwlock *rwlock)
{
    int readers = atomic_dec(&rwlock->reader_count);
    
    // Last reader releases writer mutex
    if (readers == 1) {
        k_mutex_unlock(&rwlock->writer_mutex);
    }
}

// Acquire write lock
int k_rwlock_write_lock(struct k_rwlock *rwlock, k_timeout_t timeout)
{
    atomic_set(&rwlock->writer_waiting, 1);
    
    int ret = k_mutex_lock(&rwlock->writer_mutex, timeout);
    
    atomic_set(&rwlock->writer_waiting, 0);
    return ret;
}

// Release write lock
void k_rwlock_write_unlock(struct k_rwlock *rwlock)
{
    k_mutex_unlock(&rwlock->writer_mutex);
}
```

## Semaphore Theory and Implementation

### Understanding Semaphore Semantics

Semaphores provide a counting mechanism for resource management and thread signaling. Zephyr supports both binary semaphores (similar to mutexes) and counting semaphores for managing pools of identical resources.

#### Counting Semaphore for Resource Pool Management

```c
/*
 * Resource Pool Management with Counting Semaphores
 * 
 * This example demonstrates managing a pool of identical
 * resources using counting semaphores.
 */

#define MAX_CONNECTIONS 5
#define CONNECTION_BUFFER_SIZE 512

struct network_connection {
    bool in_use;
    int socket_fd;
    char buffer[CONNECTION_BUFFER_SIZE];
    uint32_t bytes_received;
    uint64_t last_activity;
};

static struct network_connection connection_pool[MAX_CONNECTIONS];
static K_SEM_DEFINE(connection_semaphore, MAX_CONNECTIONS, MAX_CONNECTIONS);
static K_MUTEX_DEFINE(pool_mutex);

// Acquire connection from pool
struct network_connection *acquire_connection(k_timeout_t timeout)
{
    int ret;
    struct network_connection *conn = NULL;
    
    // Wait for available connection
    ret = k_sem_take(&connection_semaphore, timeout);
    if (ret != 0) {
        LOG_WRN("No connections available within timeout");
        return NULL;
    }
    
    // Find and allocate connection from pool
    k_mutex_lock(&pool_mutex, K_FOREVER);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!connection_pool[i].in_use) {
            conn = &connection_pool[i];
            conn->in_use = true;
            conn->socket_fd = -1;
            conn->bytes_received = 0;
            conn->last_activity = k_uptime_get();
            memset(conn->buffer, 0, sizeof(conn->buffer));
            break;
        }
    }
    
    k_mutex_unlock(&pool_mutex);
    
    if (conn == NULL) {
        // This shouldn't happen if semaphore works correctly
        LOG_ERR("Semaphore/pool inconsistency detected");
        k_sem_give(&connection_semaphore);
    } else {
        LOG_DBG("Acquired connection %p", conn);
    }
    
    return conn;
}

// Release connection back to pool
void release_connection(struct network_connection *conn)
{
    if (conn == NULL) {
        return;
    }
    
    k_mutex_lock(&pool_mutex, K_FOREVER);
    
    // Verify connection belongs to pool
    if (conn >= connection_pool && 
        conn < connection_pool + MAX_CONNECTIONS) {
        
        if (conn->in_use) {
            // Clean up connection
            if (conn->socket_fd >= 0) {
                close(conn->socket_fd);
                conn->socket_fd = -1;
            }
            
            conn->in_use = false;
            conn->bytes_received = 0;
            memset(conn->buffer, 0, sizeof(conn->buffer));
            
            LOG_DBG("Released connection %p", conn);
            
            // Signal semaphore that connection is available
            k_sem_give(&connection_semaphore);
        } else {
            LOG_WRN("Attempting to release already free connection");
        }
    } else {
        LOG_ERR("Invalid connection pointer: %p", conn);
    }
    
    k_mutex_unlock(&pool_mutex);
}

// Get pool statistics
void get_connection_pool_stats(int *total, int *available, int *in_use)
{
    k_mutex_lock(&pool_mutex, K_FOREVER);
    
    *total = MAX_CONNECTIONS;
    *in_use = 0;
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connection_pool[i].in_use) {
            (*in_use)++;
        }
    }
    
    *available = *total - *in_use;
    
    k_mutex_unlock(&pool_mutex);
    
    LOG_DBG("Pool stats: total=%d, available=%d, in_use=%d", 
            *total, *available, *in_use);
}
```

#### Producer-Consumer Pattern with Semaphores

```c
/*
 * Classic Producer-Consumer Implementation
 * 
 * Demonstrates coordinated data transfer between threads
 * using semaphores and circular buffers.
 */

#define BUFFER_SIZE 32

struct circular_buffer {
    uint32_t data[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
};

static struct circular_buffer shared_buffer = {0};
static K_SEM_DEFINE(buffer_full, 0, BUFFER_SIZE);     // Items available
static K_SEM_DEFINE(buffer_empty, BUFFER_SIZE, BUFFER_SIZE); // Space available
static K_MUTEX_DEFINE(buffer_mutex);

// Producer function
void producer_function(void *arg1, void *arg2, void *arg3)
{
    uint32_t data_counter = 0;
    
    while (1) {
        // Generate data
        uint32_t data = data_counter++;
        
        // Wait for space in buffer
        int ret = k_sem_take(&buffer_empty, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("Producer semaphore error: %d", ret);
            continue;
        }
        
        // Critical section: add data to buffer
        k_mutex_lock(&buffer_mutex, K_FOREVER);
        
        shared_buffer.data[shared_buffer.tail] = data;
        shared_buffer.tail = (shared_buffer.tail + 1) % BUFFER_SIZE;
        shared_buffer.count++;
        
        k_mutex_unlock(&buffer_mutex);
        
        // Signal that data is available
        k_sem_give(&buffer_full);
        
        LOG_DBG("Produced: %u (buffer count: %zu)", data, shared_buffer.count);
        
        // Simulate production time
        k_msleep(100 + (sys_rand32_get() % 100));
    }
}

// Consumer function
void consumer_function(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        // Wait for data in buffer
        int ret = k_sem_take(&buffer_full, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("Consumer semaphore error: %d", ret);
            continue;
        }
        
        // Critical section: remove data from buffer
        k_mutex_lock(&buffer_mutex, K_FOREVER);
        
        uint32_t data = shared_buffer.data[shared_buffer.head];
        shared_buffer.head = (shared_buffer.head + 1) % BUFFER_SIZE;
        shared_buffer.count--;
        
        k_mutex_unlock(&buffer_mutex);
        
        // Signal that space is available
        k_sem_give(&buffer_empty);
        
        LOG_DBG("Consumed: %u (buffer count: %zu)", data, shared_buffer.count);
        
        // Process consumed data
        process_data(data);
        
        // Simulate consumption time
        k_msleep(150 + (sys_rand32_get() % 100));
    }
}

// Data processing function
void process_data(uint32_t data)
{
    // Simulate data processing
    static uint32_t processed_count = 0;
    processed_count++;
    
    if (processed_count % 10 == 0) {
        LOG_INF("Processed %u items, latest: %u", processed_count, data);
    }
}
```

## Spinlock Implementation and SMP Considerations

### Understanding Spinlock Mechanics

Spinlocks provide the lowest-overhead synchronization primitive for protecting short critical sections, particularly in SMP (Symmetric Multiprocessing) environments. Unlike mutexes, spinlocks use active waiting, making them unsuitable for long critical sections.

#### Basic Spinlock Usage

```c
/*
 * Spinlock Usage Patterns for High-Performance Synchronization
 * 
 * Demonstrates proper spinlock usage for protecting shared
 * data structures with minimal overhead.
 */

#include <zephyr/spinlock.h>

// Shared data structure requiring high-performance protection
struct high_frequency_counter {
    atomic_t value;
    uint64_t last_update_cycle;
    uint32_t update_count;
} __aligned(64);  // Cache line alignment

static struct high_frequency_counter hf_counter = {0};
static struct k_spinlock counter_spinlock;

// High-frequency counter increment (ISR-safe)
void increment_hf_counter(void)
{
    k_spinlock_key_t key = k_spin_lock(&counter_spinlock);
    
    // Very short critical section
    atomic_inc(&hf_counter.value);
    hf_counter.last_update_cycle = k_cycle_get_64();
    hf_counter.update_count++;
    
    k_spin_unlock(&counter_spinlock, key);
}

// Read counter with consistent snapshot
void read_hf_counter(struct high_frequency_counter *snapshot)
{
    k_spinlock_key_t key = k_spin_lock(&counter_spinlock);
    
    // Fast copy for consistent read
    snapshot->value = atomic_get(&hf_counter.value);
    snapshot->last_update_cycle = hf_counter.last_update_cycle;
    snapshot->update_count = hf_counter.update_count;
    
    k_spin_unlock(&counter_spinlock, key);
}

// Try-lock pattern for non-blocking access
bool try_increment_hf_counter(void)
{
    k_spinlock_key_t key;
    
    if (k_spin_trylock(&counter_spinlock, &key) != 0) {
        return false; // Lock not available
    }
    
    // Critical section
    atomic_inc(&hf_counter.value);
    hf_counter.last_update_cycle = k_cycle_get_64();
    hf_counter.update_count++;
    
    k_spin_unlock(&counter_spinlock, key);
    return true;
}
```

#### Lock-Free Alternatives to Spinlocks

```c
/*
 * Lock-Free Data Structures using Atomic Operations
 * 
 * For ultimate performance, some scenarios benefit from
 * completely lock-free approaches using atomic operations.
 */

// Lock-free queue node
struct lockfree_node {
    atomic_ptr_t next;
    uint32_t data;
};

// Lock-free queue structure
struct lockfree_queue {
    atomic_ptr_t head;
    atomic_ptr_t tail;
    atomic_t size;
};

// Initialize lock-free queue
void lockfree_queue_init(struct lockfree_queue *queue)
{
    // Create dummy node
    struct lockfree_node *dummy = k_malloc(sizeof(struct lockfree_node));
    atomic_set_ptr(&dummy->next, NULL);
    
    atomic_set_ptr(&queue->head, dummy);
    atomic_set_ptr(&queue->tail, dummy);
    atomic_set(&queue->size, 0);
}

// Lock-free enqueue operation
bool lockfree_queue_enqueue(struct lockfree_queue *queue, uint32_t data)
{
    struct lockfree_node *new_node = k_malloc(sizeof(struct lockfree_node));
    if (new_node == NULL) {
        return false;
    }
    
    new_node->data = data;
    atomic_set_ptr(&new_node->next, NULL);
    
    struct lockfree_node *tail;
    struct lockfree_node *next;
    
    while (1) {
        tail = atomic_get_ptr(&queue->tail);
        next = atomic_get_ptr(&tail->next);
        
        // Check if tail is still the last node
        if (tail == atomic_get_ptr(&queue->tail)) {
            if (next == NULL) {
                // Try to link new node at end of list
                if (atomic_cas_ptr(&tail->next, NULL, new_node)) {
                    break; // Success
                }
            } else {
                // Try to advance tail pointer
                atomic_cas_ptr(&queue->tail, tail, next);
            }
        }
    }
    
    // Try to advance tail pointer
    atomic_cas_ptr(&queue->tail, tail, new_node);
    atomic_inc(&queue->size);
    
    return true;
}

// Lock-free dequeue operation
bool lockfree_queue_dequeue(struct lockfree_queue *queue, uint32_t *data)
{
    struct lockfree_node *head;
    struct lockfree_node *tail;
    struct lockfree_node *next;
    
    while (1) {
        head = atomic_get_ptr(&queue->head);
        tail = atomic_get_ptr(&queue->tail);
        next = atomic_get_ptr(&head->next);
        
        // Check consistency
        if (head == atomic_get_ptr(&queue->head)) {
            if (head == tail) {
                if (next == NULL) {
                    return false; // Queue is empty
                }
                // Try to advance tail
                atomic_cas_ptr(&queue->tail, tail, next);
            } else {
                if (next == NULL) {
                    continue; // Inconsistent state, retry
                }
                
                // Read data before dequeue
                *data = next->data;
                
                // Try to advance head
                if (atomic_cas_ptr(&queue->head, head, next)) {
                    break; // Success
                }
            }
        }
    }
    
    k_free(head);
    atomic_dec(&queue->size);
    return true;
}
```

## Condition Variables and Advanced Synchronization

### Condition Variable Theory

Condition variables enable threads to wait for specific conditions to become true while atomically releasing associated mutexes. This powerful primitive supports complex synchronization patterns beyond simple mutual exclusion.

#### Condition Variable Implementation Patterns

```c
/*
 * Advanced Thread Coordination with Condition Variables
 * 
 * Demonstrates complex synchronization patterns using
 * condition variables for efficient thread coordination.
 */

// Shared state structure
struct work_queue_state {
    struct k_fifo work_queue;
    bool shutdown_requested;
    int active_workers;
    int pending_work_count;
    uint64_t total_work_processed;
};

static struct work_queue_state queue_state = {0};
static K_MUTEX_DEFINE(state_mutex);
static K_CONDVAR_DEFINE(work_available_cv);
static K_CONDVAR_DEFINE(shutdown_complete_cv);

// Work item structure
struct work_item {
    void *fifo_reserved; // Required for k_fifo
    uint32_t work_id;
    void (*work_function)(uint32_t id);
    uint64_t submit_time;
};

// Worker thread implementation
void worker_thread(void *arg1, void *arg2, void *arg3)
{
    int worker_id = POINTER_TO_INT(arg1);
    struct work_item *item;
    
    LOG_INF("Worker %d starting", worker_id);
    
    k_mutex_lock(&state_mutex, K_FOREVER);
    queue_state.active_workers++;
    k_mutex_unlock(&state_mutex);
    
    while (1) {
        k_mutex_lock(&state_mutex, K_FOREVER);
        
        // Wait for work or shutdown signal
        while (k_fifo_is_empty(&queue_state.work_queue) && 
               !queue_state.shutdown_requested) {
            
            LOG_DBG("Worker %d waiting for work", worker_id);
            k_condvar_wait(&work_available_cv, &state_mutex, K_FOREVER);
        }
        
        // Check for shutdown
        if (queue_state.shutdown_requested && 
            k_fifo_is_empty(&queue_state.work_queue)) {
            
            queue_state.active_workers--;
            LOG_INF("Worker %d shutting down", worker_id);
            
            // Signal shutdown completion if last worker
            if (queue_state.active_workers == 0) {
                k_condvar_broadcast(&shutdown_complete_cv);
            }
            
            k_mutex_unlock(&state_mutex);
            break;
        }
        
        // Get work item
        item = k_fifo_get(&queue_state.work_queue, K_NO_WAIT);
        if (item != NULL) {
            queue_state.pending_work_count--;
        }
        
        k_mutex_unlock(&state_mutex);
        
        // Process work item outside critical section
        if (item != NULL) {
            LOG_DBG("Worker %d processing work item %u", worker_id, item->work_id);
            
            uint64_t start_time = k_uptime_get();
            item->work_function(item->work_id);
            uint64_t processing_time = k_uptime_get() - start_time;
            
            LOG_DBG("Worker %d completed work item %u in %llu ms", 
                    worker_id, item->work_id, processing_time);
            
            // Update statistics
            k_mutex_lock(&state_mutex, K_FOREVER);
            queue_state.total_work_processed++;
            k_mutex_unlock(&state_mutex);
            
            k_free(item);
        }
    }
    
    LOG_INF("Worker %d terminated", worker_id);
}

// Submit work to queue
int submit_work(uint32_t work_id, void (*work_function)(uint32_t))
{
    if (work_function == NULL) {
        return -EINVAL;
    }
    
    struct work_item *item = k_malloc(sizeof(struct work_item));
    if (item == NULL) {
        return -ENOMEM;
    }
    
    item->work_id = work_id;
    item->work_function = work_function;
    item->submit_time = k_uptime_get();
    
    k_mutex_lock(&state_mutex, K_FOREVER);
    
    if (queue_state.shutdown_requested) {
        k_mutex_unlock(&state_mutex);
        k_free(item);
        return -ESHUTDOWN;
    }
    
    k_fifo_put(&queue_state.work_queue, item);
    queue_state.pending_work_count++;
    
    // Signal workers that work is available
    k_condvar_signal(&work_available_cv);
    
    k_mutex_unlock(&state_mutex);
    
    LOG_DBG("Submitted work item %u", work_id);
    return 0;
}

// Shutdown work queue and wait for completion
void shutdown_work_queue(k_timeout_t timeout)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    
    LOG_INF("Initiating work queue shutdown");
    queue_state.shutdown_requested = true;
    
    // Wake up all waiting workers
    k_condvar_broadcast(&work_available_cv);
    
    // Wait for all workers to complete
    while (queue_state.active_workers > 0) {
        int ret = k_condvar_wait(&shutdown_complete_cv, &state_mutex, timeout);
        if (ret == -EAGAIN) {
            LOG_WRN("Shutdown timeout - %d workers still active", 
                    queue_state.active_workers);
            break;
        }
    }
    
    LOG_INF("Work queue shutdown complete. Processed %llu total items", 
            queue_state.total_work_processed);
    
    k_mutex_unlock(&state_mutex);
}
```

## Performance Analysis and Optimization

### Synchronization Overhead Analysis

Understanding the performance characteristics of different synchronization primitives is crucial for embedded system optimization:

```c
/*
 * Performance Benchmarking for Synchronization Primitives
 * 
 * Measures and compares the overhead of different synchronization
 * mechanisms under various load conditions.
 */

#include <zephyr/sys/printk.h>

// Benchmark configuration
#define BENCHMARK_ITERATIONS 10000
#define BENCHMARK_THREADS 4

// Test structures
static K_MUTEX_DEFINE(bench_mutex);
static struct k_spinlock bench_spinlock;
static K_SEM_DEFINE(bench_semaphore, 1, 1);
static volatile uint32_t shared_counter;

// Benchmark results structure
struct benchmark_results {
    uint64_t mutex_time_ns;
    uint64_t spinlock_time_ns;
    uint64_t semaphore_time_ns;
    uint64_t atomic_time_ns;
    uint32_t iterations;
};

// Mutex benchmark
uint64_t benchmark_mutex(uint32_t iterations)
{
    uint64_t start_time = k_cycle_get_64();
    
    for (uint32_t i = 0; i < iterations; i++) {
        k_mutex_lock(&bench_mutex, K_FOREVER);
        shared_counter++;
        k_mutex_unlock(&bench_mutex);
    }
    
    uint64_t end_time = k_cycle_get_64();
    return k_cyc_to_ns_floor64(end_time - start_time);
}

// Spinlock benchmark
uint64_t benchmark_spinlock(uint32_t iterations)
{
    uint64_t start_time = k_cycle_get_64();
    
    for (uint32_t i = 0; i < iterations; i++) {
        k_spinlock_key_t key = k_spin_lock(&bench_spinlock);
        shared_counter++;
        k_spin_unlock(&bench_spinlock, key);
    }
    
    uint64_t end_time = k_cycle_get_64();
    return k_cyc_to_ns_floor64(end_time - start_time);
}

// Semaphore benchmark
uint64_t benchmark_semaphore(uint32_t iterations)
{
    uint64_t start_time = k_cycle_get_64();
    
    for (uint32_t i = 0; i < iterations; i++) {
        k_sem_take(&bench_semaphore, K_FOREVER);
        shared_counter++;
        k_sem_give(&bench_semaphore);
    }
    
    uint64_t end_time = k_cycle_get_64();
    return k_cyc_to_ns_floor64(end_time - start_time);
}

// Atomic operations benchmark
uint64_t benchmark_atomic(uint32_t iterations)
{
    uint64_t start_time = k_cycle_get_64();
    
    for (uint32_t i = 0; i < iterations; i++) {
        atomic_inc((atomic_t *)&shared_counter);
    }
    
    uint64_t end_time = k_cycle_get_64();
    return k_cyc_to_ns_floor64(end_time - start_time);
}

// Run comprehensive benchmarks
void run_synchronization_benchmarks(struct benchmark_results *results)
{
    LOG_INF("Starting synchronization primitive benchmarks");
    
    results->iterations = BENCHMARK_ITERATIONS;
    
    // Reset counter
    shared_counter = 0;
    results->mutex_time_ns = benchmark_mutex(BENCHMARK_ITERATIONS);
    LOG_INF("Mutex benchmark: %llu ns total, %.2f ns/op", 
            results->mutex_time_ns, 
            (double)results->mutex_time_ns / BENCHMARK_ITERATIONS);
    
    // Reset counter
    shared_counter = 0;
    results->spinlock_time_ns = benchmark_spinlock(BENCHMARK_ITERATIONS);
    LOG_INF("Spinlock benchmark: %llu ns total, %.2f ns/op", 
            results->spinlock_time_ns,
            (double)results->spinlock_time_ns / BENCHMARK_ITERATIONS);
    
    // Reset counter
    shared_counter = 0;
    results->semaphore_time_ns = benchmark_semaphore(BENCHMARK_ITERATIONS);
    LOG_INF("Semaphore benchmark: %llu ns total, %.2f ns/op", 
            results->semaphore_time_ns,
            (double)results->semaphore_time_ns / BENCHMARK_ITERATIONS);
    
    // Reset counter
    shared_counter = 0;
    results->atomic_time_ns = benchmark_atomic(BENCHMARK_ITERATIONS);
    LOG_INF("Atomic benchmark: %llu ns total, %.2f ns/op", 
            results->atomic_time_ns,
            (double)results->atomic_time_ns / BENCHMARK_ITERATIONS);
    
    // Performance comparison
    LOG_INF("Performance comparison (relative to atomic):");
    LOG_INF("  Atomic:    1.00x");
    LOG_INF("  Spinlock:  %.2fx", 
            (double)results->spinlock_time_ns / results->atomic_time_ns);
    LOG_INF("  Semaphore: %.2fx", 
            (double)results->semaphore_time_ns / results->atomic_time_ns);
    LOG_INF("  Mutex:     %.2fx", 
            (double)results->mutex_time_ns / results->atomic_time_ns);
}
```

This comprehensive theory section provides the foundation for understanding and implementing traditional multithreading primitives in Zephyr RTOS, combining theoretical concepts with practical implementation patterns and performance considerations essential for professional embedded system development.

[Next: Traditional Multithreading Primitives Lab](./lab.md)
