# Chapter 7: Thread Management - Theory

This theory section provides comprehensive understanding of Zephyr's thread management system, covering thread creation, lifecycle management, synchronization mechanisms, and performance optimization techniques for building professional concurrent embedded applications.

---

## Thread Fundamentals and Architecture

### Understanding Zephyr's Threading Model

Zephyr implements a sophisticated threading system optimized for embedded applications, providing deterministic behavior while maintaining low memory overhead and fast context switching.

**Core Threading Concepts:**

* **Lightweight Threads:** Zephyr threads share address space and system resources, minimizing memory overhead compared to process-based systems
* **Deterministic Scheduling:** Priority-based scheduling with configurable preemptive and cooperative policies ensures predictable timing behavior
* **Fast Context Switching:** Assembly-optimized context switching enables responsive thread transitions even on resource-constrained microcontrollers
* **Stack Management:** Each thread maintains its own stack space, preventing interference while enabling efficient memory utilization

### Thread States and Lifecycle

Zephyr threads progress through well-defined states during their lifecycle:

```c
// Thread states (internal representation)
enum {
    K_READY,         // Ready to run, waiting for CPU time
    K_PENDING,       // Blocked waiting for resource or event
    K_PRESTART,      // Created but not yet started
    K_SUSPENDED,     // Suspended by explicit request
    K_TERMINATED,    // Thread has terminated
    K_ABORTING,      // Thread is being terminated
    K_QUEUED,        // Waiting in scheduler queue
};
```

**Thread Lifecycle Management:**

```c
#include <zephyr/kernel.h>

// Thread lifecycle demonstration
void thread_lifecycle_example(void)
{
    // Thread creation moves from PRESTART to READY state
    k_tid_t new_thread = k_thread_create(&thread_data, thread_stack,
                                        STACK_SIZE, thread_entry,
                                        NULL, NULL, NULL,
                                        PRIORITY, 0, K_NO_WAIT);
    
    // Thread runs and may enter PENDING state during blocking operations
    // like k_sleep(), k_sem_take(), k_mutex_lock()
    
    // Threads can be suspended and resumed
    k_thread_suspend(new_thread);  // SUSPENDED state
    k_thread_resume(new_thread);   // Back to READY state
    
    // Thread termination
    k_thread_abort(new_thread);    // TERMINATED state
}
```

### Scheduling Policies and Priorities

Zephyr provides flexible scheduling policies to meet diverse application requirements:

**Priority Levels:**

* **Cooperative priorities:** 0 to CONFIG_NUM_COOP_PRIORITIES-1 (higher numbers = lower priority)
* **Preemptive priorities:** CONFIG_NUM_COOP_PRIORITIES to CONFIG_NUM_COOP_PRIORITIES + CONFIG_NUM_PREEMPT_PRIORITIES-1

```c
// Priority configuration examples
#define HIGH_PRIORITY_THREAD    1   // High priority cooperative
#define NORMAL_PRIORITY_THREAD  5   // Normal priority preemptive  
#define LOW_PRIORITY_THREAD     10  // Low priority preemptive

// Thread with time-critical requirements
K_THREAD_DEFINE(critical_thread, 1024,
               critical_task, NULL, NULL, NULL,
               HIGH_PRIORITY_THREAD, 0, 0);

// Background processing thread
K_THREAD_DEFINE(background_thread, 512,
               background_task, NULL, NULL, NULL,
               LOW_PRIORITY_THREAD, 0, 0);
```

**Scheduling Policies:**

```c
// Cooperative scheduling - thread must yield voluntarily
void cooperative_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        // Perform work
        process_sensor_data();
        
        // Yield to allow other threads to run
        k_yield();
        
        // Or sleep to yield with timing
        k_sleep(K_MSEC(10));
    }
}

// Preemptive scheduling - scheduler can interrupt thread
void preemptive_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        // Perform work - can be preempted by higher priority threads
        update_display();
        
        // Blocking operations automatically yield
        k_sem_take(&data_ready_sem, K_FOREVER);
    }
}
```

## Thread Creation Patterns

### Static Thread Creation

Static threads are defined at compile time and started automatically during system initialization:

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SENSOR_STACK_SIZE   1024
#define SENSOR_PRIORITY     5

// Sensor monitoring thread
void sensor_thread_entry(void *p1, void *p2, void *p3)
{
    uint32_t sensor_count = 0;
    
    printk("Sensor monitoring thread started\n");
    
    while (1) {
        // Simulate sensor reading
        float temperature = read_temperature_sensor();
        float humidity = read_humidity_sensor();
        
        printk("Reading %u: Temp=%.1f°C, RH=%.1f%%\n", 
               sensor_count++, temperature, humidity);
        
        // Check alarm conditions
        if (temperature > 30.0f) {
            printk("⚠️  High temperature alarm: %.1f°C\n", temperature);
        }
        
        k_sleep(K_SECONDS(5));
    }
}

// Static thread definition - starts automatically
K_THREAD_DEFINE(sensor_thread_id, SENSOR_STACK_SIZE,
               sensor_thread_entry, NULL, NULL, NULL,
               SENSOR_PRIORITY, 0, 0);
```

### Dynamic Thread Creation

Dynamic threads offer runtime flexibility for applications with changing requirements:

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Thread data structures for dynamic creation
static struct k_thread communication_thread_data;
static k_thread_stack_t communication_stack[1024];

// Communication thread entry point
void communication_thread_entry(void *param1, void *param2, void *param3)
{
    const char *protocol = (const char *)param1;
    int *port = (int *)param2;
    
    printk("Communication thread started: %s on port %d\n", protocol, *port);
    
    while (1) {
        // Simulate network communication
        if (strcmp(protocol, "TCP") == 0) {
            handle_tcp_communication(*port);
        } else if (strcmp(protocol, "UDP") == 0) {
            handle_udp_communication(*port);
        }
        
        k_sleep(K_MSEC(100));
    }
}

// Dynamic thread creation function
k_tid_t create_communication_thread(const char *protocol, int port)
{
    static int port_param;  // Static storage for parameter
    port_param = port;
    
    k_tid_t thread_id = k_thread_create(&communication_thread_data,
                                       communication_stack,
                                       K_THREAD_STACK_SIZEOF(communication_stack),
                                       communication_thread_entry,
                                       (void *)protocol, &port_param, NULL,
                                       7,      // Priority
                                       0,      // Options
                                       K_NO_WAIT);  // Start immediately
    
    if (thread_id) {
        printk("Created %s communication thread (ID: %p)\n", protocol, thread_id);
    } else {
        printk("Failed to create communication thread\n");
    }
    
    return thread_id;
}

// Usage example
void initialize_communications(void)
{
    k_tid_t tcp_thread = create_communication_thread("TCP", 8080);
    k_tid_t udp_thread = create_communication_thread("UDP", 1234);
    
    // Threads are now running concurrently
}
```

### Thread Termination and Cleanup

Proper thread lifecycle management includes graceful termination:

```c
#include <zephyr/kernel.h>

// Thread with cleanup capabilities
static volatile bool worker_thread_running = true;
static struct k_sem worker_cleanup_sem;

void worker_thread_entry(void *p1, void *p2, void *p3)
{
    int *work_count = (int *)p1;
    
    printk("Worker thread starting\n");
    
    // Initialize resources
    initialize_worker_resources();
    
    while (worker_thread_running) {
        // Perform work
        (*work_count)++;
        process_work_item();
        
        k_sleep(K_MSEC(100));
    }
    
    // Cleanup resources
    cleanup_worker_resources();
    printk("Worker thread completed %d work items\n", *work_count);
    
    // Signal completion
    k_sem_give(&worker_cleanup_sem);
}

// Graceful thread termination
void terminate_worker_thread(void)
{
    printk("Requesting worker thread termination\n");
    
    // Request termination
    worker_thread_running = false;
    
    // Wait for cleanup completion
    k_sem_take(&worker_cleanup_sem, K_SECONDS(5));
    
    printk("Worker thread terminated gracefully\n");
}

// Thread initialization
int init_worker_system(void)
{
    k_sem_init(&worker_cleanup_sem, 0, 1);
    
    static int work_count = 0;
    K_THREAD_DEFINE(worker_thread, 1024,
                   worker_thread_entry, &work_count, NULL, NULL,
                   6, 0, 0);
    
    return 0;
}
```

## Thread Synchronization Mechanisms

### Mutexes for Resource Protection

Mutexes provide exclusive access to shared resources, preventing race conditions:

```c
#include <zephyr/kernel.h>

// Shared resource protection example
static struct k_mutex sensor_data_mutex;
static float shared_temperature = 0.0f;
static float shared_humidity = 0.0f;
static uint32_t data_update_count = 0;

// Producer thread - updates sensor data
void sensor_producer_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        // Read sensors (outside critical section)
        float temp = read_temperature_sensor();
        float humidity = read_humidity_sensor();
        
        // Critical section - update shared data
        k_mutex_lock(&sensor_data_mutex, K_FOREVER);
        
        shared_temperature = temp;
        shared_humidity = humidity;
        data_update_count++;
        
        k_mutex_unlock(&sensor_data_mutex);
        
        k_sleep(K_SECONDS(1));
    }
}

// Consumer thread - processes sensor data
void data_processor_thread(void *p1, void *p2, void *p3)
{
    uint32_t last_count = 0;
    
    while (1) {
        float temp, humidity;
        uint32_t count;
        
        // Critical section - read shared data
        k_mutex_lock(&sensor_data_mutex, K_FOREVER);
        
        temp = shared_temperature;
        humidity = shared_humidity;
        count = data_update_count;
        
        k_mutex_unlock(&sensor_data_mutex);
        
        // Process data (outside critical section)
        if (count > last_count) {
            process_sensor_reading(temp, humidity);
            last_count = count;
        }
        
        k_sleep(K_MSEC(500));
    }
}

// Initialize mutex and threads
int init_sensor_system(void)
{
    k_mutex_init(&sensor_data_mutex);
    
    K_THREAD_DEFINE(producer_thread, 1024,
                   sensor_producer_thread, NULL, NULL, NULL,
                   5, 0, 0);
    
    K_THREAD_DEFINE(processor_thread, 1024,
                   data_processor_thread, NULL, NULL, NULL,
                   6, 0, 0);
    
    return 0;
}
```

### Semaphores for Resource Counting and Signaling

Semaphores manage resource availability and coordinate thread execution:

```c
#include <zephyr/kernel.h>

// Buffer pool management with semaphores
#define BUFFER_POOL_SIZE 5
#define BUFFER_SIZE 256

static uint8_t buffer_pool[BUFFER_POOL_SIZE][BUFFER_SIZE];
static struct k_sem available_buffers;
static struct k_sem used_buffers;
static struct k_mutex buffer_pool_mutex;
static int next_available_buffer = 0;
static int next_used_buffer = 0;

// Get buffer from pool
uint8_t *get_buffer(k_timeout_t timeout)
{
    uint8_t *buffer = NULL;
    
    // Wait for available buffer
    if (k_sem_take(&available_buffers, timeout) == 0) {
        k_mutex_lock(&buffer_pool_mutex, K_FOREVER);
        
        // Get buffer from pool
        buffer = buffer_pool[next_available_buffer];
        next_available_buffer = (next_available_buffer + 1) % BUFFER_POOL_SIZE;
        
        k_mutex_unlock(&buffer_pool_mutex);
    }
    
    return buffer;
}

// Return buffer to pool
void return_buffer(uint8_t *buffer)
{
    if (!buffer) return;
    
    k_mutex_lock(&buffer_pool_mutex, K_FOREVER);
    
    // Clear buffer and return to pool
    memset(buffer, 0, BUFFER_SIZE);
    
    k_mutex_unlock(&buffer_pool_mutex);
    
    // Signal buffer availability
    k_sem_give(&available_buffers);
}

// Producer thread - fills buffers with data
void data_producer_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        uint8_t *buffer = get_buffer(K_SECONDS(1));
        if (buffer) {
            // Fill buffer with data
            fill_buffer_with_sensor_data(buffer, BUFFER_SIZE);
            
            // Signal data ready
            k_sem_give(&used_buffers);
            
            printk("Producer: Buffer filled and queued\n");
        } else {
            printk("Producer: No buffer available, dropping data\n");
        }
        
        k_sleep(K_MSEC(200));
    }
}

// Consumer thread - processes filled buffers
void data_consumer_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        // Wait for data to process
        if (k_sem_take(&used_buffers, K_SECONDS(5)) == 0) {
            k_mutex_lock(&buffer_pool_mutex, K_FOREVER);
            
            // Get filled buffer
            uint8_t *buffer = buffer_pool[next_used_buffer];
            next_used_buffer = (next_used_buffer + 1) % BUFFER_POOL_SIZE;
            
            k_mutex_unlock(&buffer_pool_mutex);
            
            // Process data
            process_buffer_data(buffer, BUFFER_SIZE);
            
            // Return buffer to pool
            return_buffer(buffer);
            
            printk("Consumer: Buffer processed and returned\n");
        } else {
            printk("Consumer: No data to process\n");
        }
    }
}

// Initialize buffer pool system
int init_buffer_pool_system(void)
{
    // Initialize semaphores
    k_sem_init(&available_buffers, BUFFER_POOL_SIZE, BUFFER_POOL_SIZE);
    k_sem_init(&used_buffers, 0, BUFFER_POOL_SIZE);
    k_mutex_init(&buffer_pool_mutex);
    
    // Start threads
    K_THREAD_DEFINE(producer_tid, 1024,
                   data_producer_thread, NULL, NULL, NULL,
                   5, 0, 0);
    
    K_THREAD_DEFINE(consumer_tid, 1024,
                   data_consumer_thread, NULL, NULL, NULL,
                   6, 0, 0);
    
    return 0;
}
```

### Condition Variables for Complex Synchronization

Condition variables enable threads to wait for complex conditions involving multiple shared variables:

```c
#include <zephyr/kernel.h>

// Complex condition synchronization example
static struct k_mutex data_mutex;
static struct k_condvar data_condition;
static int sensor_readings[10];
static int reading_count = 0;
static bool alarm_condition = false;

// Sensor thread - adds readings and checks conditions
void sensor_monitoring_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        int new_reading = read_sensor_value();
        
        k_mutex_lock(&data_mutex, K_FOREVER);
        
        // Add new reading
        sensor_readings[reading_count % 10] = new_reading;
        reading_count++;
        
        // Check for alarm condition (average > threshold)
        if (reading_count >= 5) {
            int sum = 0;
            int count = MIN(reading_count, 10);
            for (int i = 0; i < count; i++) {
                sum += sensor_readings[i];
            }
            
            bool new_alarm_state = (sum / count) > 75;
            if (new_alarm_state != alarm_condition) {
                alarm_condition = new_alarm_state;
                // Signal condition change
                k_condvar_broadcast(&data_condition);
            }
        }
        
        k_mutex_unlock(&data_mutex);
        
        k_sleep(K_SECONDS(1));
    }
}

// Alarm processing thread - waits for alarm conditions
void alarm_processing_thread(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&data_mutex, K_FOREVER);
        
        // Wait for alarm condition
        while (!alarm_condition) {
            k_condvar_wait(&data_condition, &data_mutex, K_FOREVER);
        }
        
        // Process alarm (mutex still held)
        printk("🚨 ALARM: Sensor average exceeded threshold!\n");
        activate_alarm_outputs();
        
        // Wait for alarm to clear
        while (alarm_condition) {
            k_condvar_wait(&data_condition, &data_mutex, K_FOREVER);
        }
        
        printk("✅ ALARM CLEARED: Sensor values normal\n");
        deactivate_alarm_outputs();
        
        k_mutex_unlock(&data_mutex);
    }
}

// Initialize condition variable system
int init_alarm_system(void)
{
    k_mutex_init(&data_mutex);
    k_condvar_init(&data_condition);
    
    K_THREAD_DEFINE(sensor_thread, 1024,
                   sensor_monitoring_thread, NULL, NULL, NULL,
                   5, 0, 0);
    
    K_THREAD_DEFINE(alarm_thread, 1024,
                   alarm_processing_thread, NULL, NULL, NULL,
                   4, 0, K_MSEC(100));  // Slight delay to allow sensor thread to start
    
    return 0;
}
```

## Timing and Delays

### Precise Timing Control

Zephyr provides multiple timing mechanisms for different precision and performance requirements:

```c
#include <zephyr/kernel.h>
#include <zephyr/timing/timing.h>

// Timing precision examples
void timing_examples(void)
{
    // Millisecond precision - most common
    printk("Starting millisecond delay\n");
    k_sleep(K_MSEC(1000));
    printk("1 second elapsed\n");
    
    // Microsecond precision - requires careful configuration
    printk("Starting microsecond delay\n");
    k_sleep(K_USEC(500000));  // 500ms in microseconds
    printk("500ms elapsed (microsecond precision)\n");
    
    // Busy waiting - precise but CPU intensive
    printk("Busy wait example\n");
    uint32_t start_time = k_cycle_get_32();
    k_busy_wait(1000);  // 1ms busy wait
    uint32_t end_time = k_cycle_get_32();
    
    uint32_t cycles_elapsed = end_time - start_time;
    printk("Busy wait: %u cycles elapsed\n", cycles_elapsed);
}

// Periodic timing example
void periodic_task_thread(void *p1, void *p2, void *p3)
{
    uint32_t period_ms = *(uint32_t *)p1;
    uint32_t task_count = 0;
    
    k_timeout_t period = K_MSEC(period_ms);
    
    while (1) {
        uint32_t start_time = k_uptime_get_32();
        
        // Perform periodic task
        perform_periodic_work();
        task_count++;
        
        uint32_t work_time = k_uptime_get_32() - start_time;
        
        if (work_time > period_ms) {
            printk("⚠️  Task %u overran by %u ms\n", 
                   task_count, work_time - period_ms);
        }
        
        // Sleep for remainder of period
        k_sleep(period);
        
        if ((task_count % 100) == 0) {
            printk("Completed %u periodic tasks\n", task_count);
        }
    }
}

// High-precision timing with timing API
void high_precision_timing_example(void)
{
    timing_t start_time, end_time;
    uint64_t cycles, nanoseconds;
    
    // Initialize timing subsystem
    timing_init();
    timing_start();
    
    start_time = timing_counter_get();
    
    // Perform time-critical operation
    critical_operation();
    
    end_time = timing_counter_get();
    
    // Calculate elapsed time
    cycles = timing_cycles_get(&start_time, &end_time);
    nanoseconds = timing_cycles_to_ns(cycles);
    
    printk("Critical operation took %llu cycles (%llu ns)\n", 
           cycles, nanoseconds);
    
    timing_stop();
}
```

### Timeout Handling Patterns

Professional applications require robust timeout handling for reliable operation:

```c
#include <zephyr/kernel.h>

// Timeout handling examples
enum operation_result {
    OP_SUCCESS = 0,
    OP_TIMEOUT = -1,
    OP_ERROR = -2
};

// Resource acquisition with timeout
enum operation_result acquire_resource_with_timeout(struct k_mutex *resource_mutex, 
                                                   uint32_t timeout_ms)
{
    int result = k_mutex_lock(resource_mutex, K_MSEC(timeout_ms));
    
    switch (result) {
    case 0:
        return OP_SUCCESS;
    case -EAGAIN:
        printk("Resource acquisition timed out after %u ms\n", timeout_ms);
        return OP_TIMEOUT;
    default:
        printk("Resource acquisition failed with error %d\n", result);
        return OP_ERROR;
    }
}

// Communication with retry and timeout
enum operation_result communicate_with_retry(const char *message, 
                                           int max_retries, 
                                           uint32_t timeout_ms)
{
    for (int retry = 0; retry < max_retries; retry++) {
        printk("Communication attempt %d/%d\n", retry + 1, max_retries);
        
        // Simulate communication operation
        int result = send_message_with_timeout(message, K_MSEC(timeout_ms));
        
        if (result == 0) {
            printk("Communication successful on attempt %d\n", retry + 1);
            return OP_SUCCESS;
        }
        
        if (result == -EAGAIN) {
            printk("Communication timed out, retrying...\n");
            k_sleep(K_MSEC(100));  // Brief delay before retry
            continue;
        }
        
        printk("Communication failed with error %d\n", result);
        return OP_ERROR;
    }
    
    printk("Communication failed after %d attempts\n", max_retries);
    return OP_TIMEOUT;
}

// Watchdog-style monitoring thread
void monitoring_thread_with_timeout(void *p1, void *p2, void *p3)
{
    uint32_t watchdog_timeout_ms = *(uint32_t *)p1;
    k_timeout_t watchdog_period = K_MSEC(watchdog_timeout_ms);
    
    while (1) {
        bool system_healthy = true;
        
        // Check system components with timeouts
        if (acquire_resource_with_timeout(&system_mutex, 100) != OP_SUCCESS) {
            printk("⚠️  System mutex acquisition timeout\n");
            system_healthy = false;
        } else {
            // Perform health checks
            system_healthy = check_system_health();
            k_mutex_unlock(&system_mutex);
        }
        
        if (!system_healthy) {
            printk("🚨 System health check failed - initiating recovery\n");
            initiate_system_recovery();
        }
        
        k_sleep(watchdog_period);
    }
}
```

## Performance Optimization and Best Practices

### Stack Size Optimization

Proper stack sizing prevents overflow while minimizing memory usage:

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

// Stack size calculation guidelines
#define BASE_STACK_SIZE         512     // Basic thread overhead
#define FUNCTION_CALL_OVERHEAD  64      // Per function call level
#define LOCAL_VARIABLE_SPACE    128     // Local variables
#define INTERRUPT_STACK_SPACE   256     // ISR stack usage
#define SAFETY_MARGIN          128     // Safety buffer

// Conservative stack sizing
#define SENSOR_THREAD_STACK (BASE_STACK_SIZE + \
                           FUNCTION_CALL_OVERHEAD * 4 + \
                           LOCAL_VARIABLE_SPACE + \
                           SAFETY_MARGIN)

// Stack monitoring example
void monitor_stack_usage(void)
{
    size_t unused_stack = k_thread_stack_space_get(k_current_get());
    size_t total_stack = K_THREAD_STACK_SIZEOF(sensor_thread_stack);
    size_t used_stack = total_stack - unused_stack;
    
    printk("Thread stack usage: %zu/%zu bytes (%.1f%%)\n",
           used_stack, total_stack, 
           (float)used_stack / total_stack * 100.0f);
    
    if (unused_stack < 128) {
        printk("⚠️  Warning: Low stack space remaining\n");
    }
}

// Stack-efficient coding practices
void stack_efficient_function(void)
{
    // Use stack efficiently
    {
        // Large buffers in limited scope
        uint8_t temp_buffer[512];
        process_data_in_buffer(temp_buffer, sizeof(temp_buffer));
        // Buffer freed when leaving scope
    }
    
    // Continue with small stack footprint
    int result = calculate_result();
    update_system_state(result);
}
```

### Thread Priority Management

Strategic priority assignment ensures system responsiveness:

```c
#include <zephyr/kernel.h>

// Priority level definitions
#define PRIORITY_CRITICAL       1   // Safety-critical tasks
#define PRIORITY_HIGH          3   // Time-sensitive operations  
#define PRIORITY_NORMAL        5   // Regular application tasks
#define PRIORITY_LOW           8   // Background processing
#define PRIORITY_IDLE          10  // Cleanup and maintenance

// Priority inheritance example
static struct k_mutex shared_resource_mutex;

void high_priority_task(void *p1, void *p2, void *p3)
{
    while (1) {
        printk("High priority task requesting resource\n");
        
        // This will boost priority of thread holding mutex
        k_mutex_lock(&shared_resource_mutex, K_FOREVER);
        
        // Critical section
        perform_critical_operation();
        
        k_mutex_unlock(&shared_resource_mutex);
        
        k_sleep(K_MSEC(100));
    }
}

void low_priority_task(void *p1, void *p2, void *p3)
{
    while (1) {
        k_mutex_lock(&shared_resource_mutex, K_FOREVER);
        
        // Long operation - priority will be boosted if high priority task waits
        perform_lengthy_operation();
        
        k_mutex_unlock(&shared_resource_mutex);
        
        k_sleep(K_MSEC(500));
    }
}

// Dynamic priority adjustment
void adaptive_priority_thread(void *p1, void *p2, void *p3)
{
    int base_priority = 6;
    int current_priority = base_priority;
    
    while (1) {
        int system_load = get_system_load_percentage();
        
        // Adjust priority based on system load
        if (system_load > 80) {
            // Increase priority during high load
            current_priority = base_priority - 1;
        } else if (system_load < 20) {
            // Decrease priority during low load
            current_priority = base_priority + 1;
        } else {
            current_priority = base_priority;
        }
        
        // Apply priority change
        k_thread_priority_set(k_current_get(), current_priority);
        
        perform_adaptive_work();
        k_sleep(K_SECONDS(1));
    }
}
```

---

This theoretical foundation provides comprehensive understanding of Zephyr's thread management capabilities. The Lab section will demonstrate these concepts through hands-on implementation of multi-threaded applications using your Raspberry Pi 4B platform.
