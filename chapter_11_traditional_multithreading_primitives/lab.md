# Chapter 11: Traditional Multithreading Primitives - Hands-On Laboratory

## Lab Overview

This comprehensive laboratory provides hands-on experience with Zephyr's traditional multithreading primitives. You'll implement real-world synchronization patterns, analyze performance characteristics, and develop production-quality concurrent applications.

## Prerequisites

- Completion of Chapters 1-10
- Understanding of thread management and memory concepts
- Familiarity with C programming and embedded systems
- Zephyr development environment configured

## Lab Environment Setup

### Supported Hardware Platforms

```yaml
# Recommended boards for multithreading labs
primary_boards:
  - qemu_cortex_m3    # QEMU emulation - always available
  - nucleo_f767zi     # ARM Cortex-M7 with good performance
  - frdm_k64f         # ARM Cortex-M4 with debugging support
  - arduino_due       # ARM Cortex-M3 reference platform

smp_boards:
  - qemu_cortex_a53   # Multi-core ARM Cortex-A53
  - mimxrt1170_evk    # Dual-core Cortex-M7/M4
  - esp32             # Dual-core Xtensa (when supported)

# Required Kconfig options
required_config:
  - CONFIG_MULTITHREADING=y
  - CONFIG_SMP=y  # For SMP-specific labs
  - CONFIG_PRINTK=y
  - CONFIG_LOG=y
  - CONFIG_KERNEL_COHERENCE=y  # For SMP systems
```

### Development Environment

```bash
# Set up comprehensive lab workspace
mkdir -p ~/zephyr-workspace/multithreading_labs
cd ~/zephyr-workspace/multithreading_labs

# Create organized lab structure
mkdir -p {lab1_mutex,lab2_semaphore,lab3_spinlock,lab4_condvar,lab5_advanced}
mkdir -p shared/{include,src,configs}
mkdir -p results/{benchmarks,traces,analysis}

# Create shared utility header
cat > shared/include/lab_utils.h << 'EOF'
#ifndef LAB_UTILS_H
#define LAB_UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

// Common logging setup
#define LAB_LOG_LEVEL LOG_LEVEL_INF

// Performance measurement utilities
static inline uint64_t get_timestamp_ns(void)
{
    return k_cyc_to_ns_floor64(k_cycle_get_64());
}

// Random delay generator for testing
static inline void random_delay_ms(uint32_t min_ms, uint32_t max_ms)
{
    uint32_t delay = min_ms + (sys_rand32_get() % (max_ms - min_ms + 1));
    k_msleep(delay);
}

#endif /* LAB_UTILS_H */
EOF
```

## Lab 1: Mutex Fundamentals and Priority Inheritance

### Objective
Master mutex usage patterns, understand priority inheritance, and implement robust resource protection mechanisms.

### Implementation

#### Step 1: Basic Mutex Operations

Create `lab1_mutex/src/main.c`:

```c
/*
 * Lab 1: Mutex Fundamentals
 * 
 * This lab demonstrates:
 * - Basic mutex operations
 * - Priority inheritance behavior
 * - Error handling patterns
 * - Performance measurement
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "../shared/include/lab_utils.h"

LOG_MODULE_REGISTER(mutex_lab, LAB_LOG_LEVEL);

// Shared resource simulation
struct shared_bank_account {
    uint32_t balance;
    uint32_t transaction_count;
    char last_operation[64];
    uint64_t last_update_time;
};

static struct shared_bank_account account = {
    .balance = 10000,  // Initial balance: $100.00
    .transaction_count = 0
};

static K_MUTEX_DEFINE(account_mutex);

// Performance tracking
struct mutex_stats {
    uint32_t lock_attempts;
    uint32_t lock_successes;
    uint32_t lock_timeouts;
    uint64_t total_lock_time_ns;
    uint64_t max_lock_time_ns;
} stats = {0};

// Safe account operations with comprehensive error handling
int safe_deposit(uint32_t amount, const char *description)
{
    if (amount == 0 || amount > 100000) {
        LOG_ERR("Invalid deposit amount: %u", amount);
        return -EINVAL;
    }
    
    uint64_t lock_start = get_timestamp_ns();
    stats.lock_attempts++;
    
    int ret = k_mutex_lock(&account_mutex, K_MSEC(1000));
    if (ret != 0) {
        stats.lock_timeouts++;
        LOG_ERR("Failed to acquire account mutex: %d", ret);
        return ret;
    }
    
    uint64_t lock_acquired = get_timestamp_ns();
    uint64_t lock_time = lock_acquired - lock_start;
    
    stats.lock_successes++;
    stats.total_lock_time_ns += lock_time;
    if (lock_time > stats.max_lock_time_ns) {
        stats.max_lock_time_ns = lock_time;
    }
    
    // Critical section - exclusive access to account
    LOG_DBG("Depositing %u cents to account", amount);
    
    // Check for overflow
    if (account.balance > UINT32_MAX - amount) {
        LOG_WRN("Deposit would cause overflow, rejecting");
        k_mutex_unlock(&account_mutex);
        return -ERANGE;
    }
    
    account.balance += amount;
    account.transaction_count++;
    account.last_update_time = k_uptime_get();
    
    snprintf(account.last_operation, sizeof(account.last_operation),
             "DEPOSIT: %u cents (%s)", amount, description ? description : "N/A");
    
    uint32_t new_balance = account.balance;
    uint32_t tx_count = account.transaction_count;
    
    k_mutex_unlock(&account_mutex);
    
    LOG_INF("Deposit successful: +%u cents, balance=%u, transactions=%u", 
            amount, new_balance, tx_count);
    
    return 0;
}

int safe_withdraw(uint32_t amount, const char *description)
{
    if (amount == 0 || amount > 100000) {
        LOG_ERR("Invalid withdrawal amount: %u", amount);
        return -EINVAL;
    }
    
    uint64_t lock_start = get_timestamp_ns();
    stats.lock_attempts++;
    
    int ret = k_mutex_lock(&account_mutex, K_MSEC(1000));
    if (ret != 0) {
        stats.lock_timeouts++;
        LOG_ERR("Failed to acquire account mutex: %d", ret);
        return ret;
    }
    
    uint64_t lock_acquired = get_timestamp_ns();
    uint64_t lock_time = lock_acquired - lock_start;
    
    stats.lock_successes++;
    stats.total_lock_time_ns += lock_time;
    if (lock_time > stats.max_lock_time_ns) {
        stats.max_lock_time_ns = lock_time;
    }
    
    // Critical section
    LOG_DBG("Attempting to withdraw %u cents from account", amount);
    
    if (account.balance < amount) {
        LOG_WRN("Insufficient funds: balance=%u, requested=%u", 
                account.balance, amount);
        k_mutex_unlock(&account_mutex);
        return -ENOSPC;
    }
    
    account.balance -= amount;
    account.transaction_count++;
    account.last_update_time = k_uptime_get();
    
    snprintf(account.last_operation, sizeof(account.last_operation),
             "WITHDRAW: %u cents (%s)", amount, description ? description : "N/A");
    
    uint32_t new_balance = account.balance;
    uint32_t tx_count = account.transaction_count;
    
    k_mutex_unlock(&account_mutex);
    
    LOG_INF("Withdrawal successful: -%u cents, balance=%u, transactions=%u", 
            amount, new_balance, tx_count);
    
    return 0;
}

int safe_get_balance(uint32_t *balance, uint32_t *tx_count)
{
    if (balance == NULL || tx_count == NULL) {
        return -EINVAL;
    }
    
    int ret = k_mutex_lock(&account_mutex, K_MSEC(500));
    if (ret != 0) {
        return ret;
    }
    
    *balance = account.balance;
    *tx_count = account.transaction_count;
    
    k_mutex_unlock(&account_mutex);
    return 0;
}

// Thread implementations
#define THREAD_STACK_SIZE 2048
#define NUM_CUSTOMERS 3

// Customer threads performing transactions
void customer_thread(void *arg1, void *arg2, void *arg3)
{
    int customer_id = POINTER_TO_INT(arg1);
    int transaction_count = POINTER_TO_INT(arg2);
    
    LOG_INF("Customer %d starting with %d planned transactions", 
            customer_id, transaction_count);
    
    for (int i = 0; i < transaction_count; i++) {
        // Random transaction type and amount
        bool is_deposit = (sys_rand32_get() % 2) == 0;
        uint32_t amount = 50 + (sys_rand32_get() % 1000); // $0.50 to $10.00
        
        char desc[32];
        snprintf(desc, sizeof(desc), "Customer%d-Tx%d", customer_id, i);
        
        int ret;
        if (is_deposit) {
            ret = safe_deposit(amount, desc);
        } else {
            ret = safe_withdraw(amount, desc);
        }
        
        if (ret != 0 && ret != -ENOSPC) {
            LOG_ERR("Customer %d transaction failed: %d", customer_id, ret);
        }
        
        // Random delay between transactions
        random_delay_ms(100, 500);
    }
    
    LOG_INF("Customer %d completed all transactions", customer_id);
}

// High priority monitoring thread
void priority_monitor_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Priority monitor thread starting (priority: %d)", 
            k_thread_priority_get(k_current_get()));
    
    while (1) {
        uint32_t balance, tx_count;
        int ret = safe_get_balance(&balance, &tx_count);
        
        if (ret == 0) {
            LOG_INF("=== PRIORITY MONITOR === Balance: $%u.%02u, Transactions: %u",
                    balance / 100, balance % 100, tx_count);
        } else {
            LOG_WRN("Monitor failed to get balance: %d", ret);
        }
        
        k_msleep(2000);
    }
}

// Background statistics reporter
void stats_reporter_thread(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        k_msleep(5000);
        
        LOG_INF("=== MUTEX STATISTICS ===");
        LOG_INF("Lock attempts: %u", stats.lock_attempts);
        LOG_INF("Lock successes: %u", stats.lock_successes);
        LOG_INF("Lock timeouts: %u", stats.lock_timeouts);
        
        if (stats.lock_successes > 0) {
            uint64_t avg_lock_time = stats.total_lock_time_ns / stats.lock_successes;
            LOG_INF("Average lock time: %llu ns", avg_lock_time);
            LOG_INF("Maximum lock time: %llu ns", stats.max_lock_time_ns);
        }
        
        uint32_t final_balance, final_tx_count;
        if (safe_get_balance(&final_balance, &final_tx_count) == 0) {
            LOG_INF("Current balance: $%u.%02u", 
                    final_balance / 100, final_balance % 100);
        }
    }
}

int main(void)
{
    LOG_INF("=== Mutex Fundamentals Lab ===");
    LOG_INF("Initial account balance: $%u.%02u", 
            account.balance / 100, account.balance % 100);
    
    // Create customer threads
    static struct k_thread customer_threads[NUM_CUSTOMERS];
    static K_THREAD_STACK_ARRAY_DEFINE(customer_stacks, NUM_CUSTOMERS, THREAD_STACK_SIZE);
    
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        k_thread_create(&customer_threads[i],
                       customer_stacks[i],
                       K_THREAD_STACK_SIZEOF(customer_stacks[i]),
                       customer_thread,
                       INT_TO_POINTER(i + 1),    // Customer ID
                       INT_TO_POINTER(10),       // Transaction count
                       NULL,
                       7,    // Priority
                       0,    // Options
                       K_NO_WAIT);
    }
    
    // Create high-priority monitor (demonstrates priority inheritance)
    static struct k_thread monitor_thread;
    static K_THREAD_STACK_DEFINE(monitor_stack, THREAD_STACK_SIZE);
    
    k_thread_create(&monitor_thread,
                   monitor_stack,
                   K_THREAD_STACK_SIZEOF(monitor_stack),
                   priority_monitor_thread,
                   NULL, NULL, NULL,
                   2,    // High priority
                   0,
                   K_NO_WAIT);
    
    // Create statistics reporter
    static struct k_thread stats_thread;
    static K_THREAD_STACK_DEFINE(stats_stack, THREAD_STACK_SIZE);
    
    k_thread_create(&stats_thread,
                   stats_stack,
                   K_THREAD_STACK_SIZEOF(stats_stack),
                   stats_reporter_thread,
                   NULL, NULL, NULL,
                   9,    // Low priority
                   0,
                   K_NO_WAIT);
    
    // Main thread performs some transactions too
    LOG_INF("Main thread performing initial transactions");
    
    safe_deposit(5000, "Initial deposit");
    safe_withdraw(500, "Processing fee");
    safe_deposit(250, "Interest credit");
    
    // Let the system run
    LOG_INF("System running - customer threads active");
    
    while (1) {
        k_msleep(10000);
        
        // Periodic system status
        uint32_t balance, tx_count;
        if (safe_get_balance(&balance, &tx_count) == 0) {
            LOG_INF("=== SYSTEM STATUS === Balance: $%u.%02u, Total Transactions: %u",
                    balance / 100, balance % 100, tx_count);
        }
    }
    
    return 0;
}
```

#### Step 2: Configuration Files

Create `lab1_mutex/prj.conf`:

```ini
# Mutex Lab Configuration
CONFIG_MULTITHREADING=y
CONFIG_NUM_PREEMPT_PRIORITIES=16
CONFIG_PRINTK=y
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y

# Thread configuration
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Random number generation
CONFIG_TEST_RANDOM_GENERATOR=y
CONFIG_TIMER_RANDOM_GENERATOR=y

# Performance monitoring
CONFIG_THREAD_RUNTIME_STATS=y
CONFIG_SCHED_THREAD_USAGE=y

# Debugging aids
CONFIG_DEBUG=y
CONFIG_ASSERT=y
CONFIG_THREAD_NAME=y
```

Create `lab1_mutex/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mutex_fundamentals)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ../shared/include)
```

### Expected Results

```
*** Booting Zephyr OS build v4.2.99 ***
[00:00:00.000,000] <inf> mutex_lab: === Mutex Fundamentals Lab ===
[00:00:00.000,000] <inf> mutex_lab: Initial account balance: $100.00
[00:00:00.001,000] <inf> mutex_lab: Main thread performing initial transactions
[00:00:00.002,000] <inf> mutex_lab: Deposit successful: +5000 cents, balance=15000, transactions=1
[00:00:00.003,000] <inf> mutex_lab: Withdrawal successful: -500 cents, balance=14500, transactions=2
[00:00:00.004,000] <inf> mutex_lab: Deposit successful: +250 cents, balance=14750, transactions=3
[00:00:00.005,000] <inf> mutex_lab: System running - customer threads active
[00:00:00.006,000] <inf> mutex_lab: Customer 1 starting with 10 planned transactions
[00:00:00.007,000] <inf> mutex_lab: Customer 2 starting with 10 planned transactions
[00:00:00.008,000] <inf> mutex_lab: Customer 3 starting with 10 planned transactions
[00:00:00.009,000] <inf> mutex_lab: Priority monitor thread starting (priority: 2)
[00:00:00.010,000] <inf> mutex_lab: === PRIORITY MONITOR === Balance: $147.50, Transactions: 3
```

## Lab 2: Semaphore Resource Management

### Objective
Implement resource pooling, producer-consumer patterns, and complex signaling mechanisms using semaphores.

### Implementation

Create `lab2_semaphore/src/main.c`:

```c
/*
 * Lab 2: Semaphore Resource Management
 * 
 * This lab demonstrates:
 * - Resource pool management with counting semaphores
 * - Producer-consumer pattern implementation
 * - Binary semaphore signaling
 * - Performance analysis of different semaphore patterns
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include "../shared/include/lab_utils.h"

LOG_MODULE_REGISTER(semaphore_lab, LAB_LOG_LEVEL);

// Resource pool simulation (network connections)
#define MAX_CONNECTIONS 5
#define CONNECTION_BUFFER_SIZE 256

struct network_connection {
    bool in_use;
    int connection_id;
    char buffer[CONNECTION_BUFFER_SIZE];
    uint32_t bytes_transferred;
    uint64_t connect_time;
    uint64_t last_activity;
};

static struct network_connection connection_pool[MAX_CONNECTIONS];
static K_SEM_DEFINE(connection_semaphore, MAX_CONNECTIONS, MAX_CONNECTIONS);
static K_MUTEX_DEFINE(pool_mutex);

// Producer-Consumer buffer
#define BUFFER_SIZE 32
#define MAX_PRODUCERS 3
#define MAX_CONSUMERS 2

struct data_packet {
    uint32_t sequence_number;
    uint32_t payload_size;
    uint8_t payload[64];
    uint64_t timestamp;
};

static struct data_packet circular_buffer[BUFFER_SIZE];
static size_t buffer_head = 0;
static size_t buffer_tail = 0;
static K_SEM_DEFINE(buffer_full, 0, BUFFER_SIZE);      // Items available
static K_SEM_DEFINE(buffer_empty, BUFFER_SIZE, BUFFER_SIZE); // Space available
static K_MUTEX_DEFINE(buffer_mutex);

// Statistics tracking
struct semaphore_stats {
    uint32_t connections_acquired;
    uint32_t connections_released;
    uint32_t connection_timeouts;
    uint64_t total_connection_time;
    uint32_t packets_produced;
    uint32_t packets_consumed;
    uint32_t buffer_overruns;
    uint32_t buffer_underruns;
} sem_stats = {0};

// Connection pool management
struct network_connection *acquire_connection(k_timeout_t timeout)
{
    uint64_t start_time = get_timestamp_ns();
    
    // Wait for available connection
    int ret = k_sem_take(&connection_semaphore, timeout);
    if (ret != 0) {
        sem_stats.connection_timeouts++;
        LOG_WRN("Connection acquisition timeout after %llu ns", 
                get_timestamp_ns() - start_time);
        return NULL;
    }
    
    // Find and allocate connection from pool
    k_mutex_lock(&pool_mutex, K_FOREVER);
    
    struct network_connection *conn = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!connection_pool[i].in_use) {
            conn = &connection_pool[i];
            conn->in_use = true;
            conn->connection_id = i;
            conn->bytes_transferred = 0;
            conn->connect_time = k_uptime_get();
            conn->last_activity = conn->connect_time;
            memset(conn->buffer, 0, sizeof(conn->buffer));
            sem_stats.connections_acquired++;
            break;
        }
    }
    
    k_mutex_unlock(&pool_mutex);
    
    if (conn != NULL) {
        LOG_DBG("Acquired connection %d (total time: %llu ns)", 
                conn->connection_id, get_timestamp_ns() - start_time);
    } else {
        LOG_ERR("Semaphore/pool inconsistency detected");
        k_sem_give(&connection_semaphore);
    }
    
    return conn;
}

void release_connection(struct network_connection *conn)
{
    if (conn == NULL) {
        return;
    }
    
    k_mutex_lock(&pool_mutex, K_FOREVER);
    
    if (conn >= connection_pool && 
        conn < connection_pool + MAX_CONNECTIONS &&
        conn->in_use) {
        
        uint64_t connection_duration = k_uptime_get() - conn->connect_time;
        sem_stats.total_connection_time += connection_duration;
        sem_stats.connections_released++;
        
        LOG_DBG("Releasing connection %d (duration: %llu ms, bytes: %u)", 
                conn->connection_id, connection_duration, conn->bytes_transferred);
        
        conn->in_use = false;
        conn->connection_id = -1;
        conn->bytes_transferred = 0;
        
        k_sem_give(&connection_semaphore);
    } else {
        LOG_ERR("Invalid connection release attempt");
    }
    
    k_mutex_unlock(&pool_mutex);
}

// Producer-Consumer implementation
void produce_packet(uint32_t sequence, const char *data)
{
    // Wait for space in buffer
    int ret = k_sem_take(&buffer_empty, K_MSEC(2000));
    if (ret != 0) {
        sem_stats.buffer_overruns++;
        LOG_WRN("Buffer full - packet %u dropped", sequence);
        return;
    }
    
    // Critical section: add packet to buffer
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    
    struct data_packet *packet = &circular_buffer[buffer_tail];
    packet->sequence_number = sequence;
    packet->timestamp = get_timestamp_ns();
    
    if (data != NULL) {
        size_t data_len = strlen(data);
        packet->payload_size = MIN(data_len, sizeof(packet->payload) - 1);
        memcpy(packet->payload, data, packet->payload_size);
        packet->payload[packet->payload_size] = '\0';
    } else {
        packet->payload_size = 0;
        packet->payload[0] = '\0';
    }
    
    buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
    sem_stats.packets_produced++;
    
    k_mutex_unlock(&buffer_mutex);
    
    // Signal that data is available
    k_sem_give(&buffer_full);
    
    LOG_DBG("Produced packet %u (payload: %u bytes)", sequence, packet->payload_size);
}

bool consume_packet(struct data_packet *output)
{
    if (output == NULL) {
        return false;
    }
    
    // Wait for data in buffer
    int ret = k_sem_take(&buffer_full, K_MSEC(3000));
    if (ret != 0) {
        sem_stats.buffer_underruns++;
        LOG_DBG("Buffer empty - consumer timeout");
        return false;
    }
    
    // Critical section: remove packet from buffer
    k_mutex_lock(&buffer_mutex, K_FOREVER);
    
    struct data_packet *packet = &circular_buffer[buffer_head];
    memcpy(output, packet, sizeof(*output));
    
    buffer_head = (buffer_head + 1) % BUFFER_SIZE;
    sem_stats.packets_consumed++;
    
    k_mutex_unlock(&buffer_mutex);
    
    // Signal that space is available
    k_sem_give(&buffer_empty);
    
    uint64_t latency = get_timestamp_ns() - output->timestamp;
    LOG_DBG("Consumed packet %u (latency: %llu ns)", 
            output->sequence_number, latency);
    
    return true;
}

// Thread implementations
void connection_client_thread(void *arg1, void *arg2, void *arg3)
{
    int client_id = POINTER_TO_INT(arg1);
    int operations = POINTER_TO_INT(arg2);
    
    LOG_INF("Connection client %d starting (%d operations)", client_id, operations);
    
    for (int i = 0; i < operations; i++) {
        // Acquire connection with timeout
        struct network_connection *conn = acquire_connection(K_MSEC(1000));
        
        if (conn != NULL) {
            // Simulate network activity
            uint32_t transfer_size = 100 + (sys_rand32_get() % 1000);
            
            // Simulate data transfer
            for (uint32_t j = 0; j < transfer_size; j += 10) {
                conn->bytes_transferred += MIN(10, transfer_size - j);
                conn->last_activity = k_uptime_get();
                k_usleep(100); // Simulate transfer delay
            }
            
            LOG_INF("Client %d completed transfer: %u bytes on connection %d", 
                    client_id, transfer_size, conn->connection_id);
            
            // Hold connection for variable time
            random_delay_ms(200, 800);
            
            release_connection(conn);
        } else {
            LOG_WRN("Client %d failed to acquire connection", client_id);
        }
        
        // Delay between connection attempts
        random_delay_ms(300, 700);
    }
    
    LOG_INF("Connection client %d completed", client_id);
}

void data_producer_thread(void *arg1, void *arg2, void *arg3)
{
    int producer_id = POINTER_TO_INT(arg1);
    int packet_count = POINTER_TO_INT(arg2);
    
    LOG_INF("Data producer %d starting (%d packets)", producer_id, packet_count);
    
    for (int i = 0; i < packet_count; i++) {
        uint32_t sequence = producer_id * 1000 + i;
        
        char data[32];
        snprintf(data, sizeof(data), "Producer%d-Data%d", producer_id, i);
        
        produce_packet(sequence, data);
        
        // Variable production rate
        random_delay_ms(50, 200);
    }
    
    LOG_INF("Data producer %d completed", producer_id);
}

void data_consumer_thread(void *arg1, void *arg2, void *arg3)
{
    int consumer_id = POINTER_TO_INT(arg1);
    
    LOG_INF("Data consumer %d starting", consumer_id);
    
    struct data_packet packet;
    int packets_processed = 0;
    
    while (1) {
        if (consume_packet(&packet)) {
            packets_processed++;
            
            // Simulate packet processing
            uint32_t processing_time = 10 + (sys_rand32_get() % 50);
            k_usleep(processing_time * 1000);
            
            LOG_INF("Consumer %d processed packet %u (%u bytes, processed: %d)", 
                    consumer_id, packet.sequence_number, 
                    packet.payload_size, packets_processed);
        }
        
        // Consumer runs continuously
        if (packets_processed >= 50) {
            LOG_INF("Consumer %d processed %d packets, continuing...", 
                    consumer_id, packets_processed);
            packets_processed = 0;
        }
    }
}

void statistics_monitor_thread(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        k_msleep(10000);
        
        LOG_INF("=== SEMAPHORE STATISTICS ===");
        
        // Connection pool stats
        LOG_INF("Connections: acquired=%u, released=%u, timeouts=%u", 
                sem_stats.connections_acquired, 
                sem_stats.connections_released,
                sem_stats.connection_timeouts);
        
        if (sem_stats.connections_released > 0) {
            uint64_t avg_connection_time = sem_stats.total_connection_time / 
                                         sem_stats.connections_released;
            LOG_INF("Average connection duration: %llu ms", avg_connection_time);
        }
        
        // Producer-consumer stats
        LOG_INF("Buffer: produced=%u, consumed=%u, overruns=%u, underruns=%u", 
                sem_stats.packets_produced,
                sem_stats.packets_consumed,
                sem_stats.buffer_overruns,
                sem_stats.buffer_underruns);
        
        // Current buffer state
        k_mutex_lock(&buffer_mutex, K_FOREVER);
        size_t buffer_items = (buffer_tail - buffer_head + BUFFER_SIZE) % BUFFER_SIZE;
        k_mutex_unlock(&buffer_mutex);
        
        LOG_INF("Current buffer utilization: %zu/%d items", buffer_items, BUFFER_SIZE);
        
        // Connection pool utilization
        k_mutex_lock(&pool_mutex, K_FOREVER);
        int active_connections = 0;
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (connection_pool[i].in_use) {
                active_connections++;
            }
        }
        k_mutex_unlock(&pool_mutex);
        
        LOG_INF("Active connections: %d/%d", active_connections, MAX_CONNECTIONS);
    }
}

int main(void)
{
    LOG_INF("=== Semaphore Resource Management Lab ===");
    
    // Initialize connection pool
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connection_pool[i].in_use = false;
        connection_pool[i].connection_id = -1;
    }
    
    // Create connection client threads
    static struct k_thread client_threads[4];
    static K_THREAD_STACK_ARRAY_DEFINE(client_stacks, 4, 2048);
    
    for (int i = 0; i < 4; i++) {
        k_thread_create(&client_threads[i],
                       client_stacks[i],
                       K_THREAD_STACK_SIZEOF(client_stacks[i]),
                       connection_client_thread,
                       INT_TO_POINTER(i + 1),
                       INT_TO_POINTER(8),  // 8 operations per client
                       NULL,
                       6, 0, K_NO_WAIT);
    }
    
    // Create producer threads
    static struct k_thread producer_threads[MAX_PRODUCERS];
    static K_THREAD_STACK_ARRAY_DEFINE(producer_stacks, MAX_PRODUCERS, 2048);
    
    for (int i = 0; i < MAX_PRODUCERS; i++) {
        k_thread_create(&producer_threads[i],
                       producer_stacks[i],
                       K_THREAD_STACK_SIZEOF(producer_stacks[i]),
                       data_producer_thread,
                       INT_TO_POINTER(i + 1),
                       INT_TO_POINTER(20),  // 20 packets per producer
                       NULL,
                       7, 0, K_NO_WAIT);
    }
    
    // Create consumer threads
    static struct k_thread consumer_threads[MAX_CONSUMERS];
    static K_THREAD_STACK_ARRAY_DEFINE(consumer_stacks, MAX_CONSUMERS, 2048);
    
    for (int i = 0; i < MAX_CONSUMERS; i++) {
        k_thread_create(&consumer_threads[i],
                       consumer_stacks[i],
                       K_THREAD_STACK_SIZEOF(consumer_stacks[i]),
                       data_consumer_thread,
                       INT_TO_POINTER(i + 1),
                       NULL, NULL,
                       8, 0, K_NO_WAIT);
    }
    
    // Create statistics monitor
    static struct k_thread stats_thread;
    static K_THREAD_STACK_DEFINE(stats_stack, 2048);
    
    k_thread_create(&stats_thread,
                   stats_stack,
                   K_THREAD_STACK_SIZEOF(stats_stack),
                   statistics_monitor_thread,
                   NULL, NULL, NULL,
                   9, 0, K_NO_WAIT);
    
    LOG_INF("All threads created - system running");
    
    // Main thread produces some initial packets
    for (int i = 0; i < 5; i++) {
        char data[32];
        snprintf(data, sizeof(data), "MainThread-Packet%d", i);
        produce_packet(9000 + i, data);
        k_msleep(500);
    }
    
    // Keep system running
    while (1) {
        k_msleep(30000);
        LOG_INF("System heartbeat - continuing operation");
    }
    
    return 0;
}
```

## Lab 3: Spinlock and SMP Performance

### Objective
Understand spinlock behavior, implement high-performance synchronization, and analyze SMP scaling characteristics.

### Implementation

Create `lab3_spinlock/src/main.c`:

```c
/*
 * Lab 3: Spinlock and SMP Performance
 * 
 * This lab demonstrates:
 * - Spinlock usage patterns
 * - Performance comparison with mutexes
 * - SMP scaling behavior
 * - Lock-free alternatives
 */

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include "../shared/include/lab_utils.h"

LOG_MODULE_REGISTER(spinlock_lab, LAB_LOG_LEVEL);

// High-frequency counter with different protection mechanisms
struct performance_counter {
    atomic_t spinlock_protected;
    atomic_t mutex_protected;
    atomic_t lockfree_counter;
    uint64_t spinlock_time_ns;
    uint64_t mutex_time_ns;
    uint64_t lockfree_time_ns;
    uint32_t spinlock_iterations;
    uint32_t mutex_iterations;
    uint32_t lockfree_iterations;
} __aligned(64);  // Cache line alignment

static struct performance_counter perf_counters = {0};
static struct k_spinlock counter_spinlock;
static K_MUTEX_DEFINE(counter_mutex);

// Benchmark configuration
#define BENCHMARK_ITERATIONS 10000
#define CONTENTION_THREADS 4

// Performance measurement structure
struct benchmark_results {
    uint64_t spinlock_total_ns;
    uint64_t mutex_total_ns;
    uint64_t lockfree_total_ns;
    uint32_t spinlock_contentions;
    uint32_t mutex_contentions;
    uint32_t iterations_completed;
    int thread_id;
};

// Spinlock-protected increment
void spinlock_increment(int iterations, struct benchmark_results *results)
{
    uint64_t start_time = get_timestamp_ns();
    uint32_t contentions = 0;
    
    for (int i = 0; i < iterations; i++) {
        uint64_t lock_start = k_cycle_get_64();
        k_spinlock_key_t key = k_spin_lock(&counter_spinlock);
        
        // Check for contention (rough estimate)
        uint64_t lock_cycles = k_cycle_get_64() - lock_start;
        if (lock_cycles > 100) {  // Arbitrary threshold
            contentions++;
        }
        
        // Critical section
        atomic_inc(&perf_counters.spinlock_protected);
        
        k_spin_unlock(&counter_spinlock, key);
    }
    
    uint64_t end_time = get_timestamp_ns();
    
    results->spinlock_total_ns = end_time - start_time;
    results->spinlock_contentions = contentions;
    results->iterations_completed = iterations;
    
    // Update global statistics
    perf_counters.spinlock_time_ns += results->spinlock_total_ns;
    perf_counters.spinlock_iterations += iterations;
}

// Mutex-protected increment
void mutex_increment(int iterations, struct benchmark_results *results)
{
    uint64_t start_time = get_timestamp_ns();
    uint32_t contentions = 0;
    
    for (int i = 0; i < iterations; i++) {
        uint64_t lock_start = get_timestamp_ns();
        int ret = k_mutex_lock(&counter_mutex, K_FOREVER);
        uint64_t lock_time = get_timestamp_ns() - lock_start;
        
        if (ret == 0) {
            // Check for contention
            if (lock_time > 1000) {  // 1 microsecond threshold
                contentions++;
            }
            
            // Critical section
            atomic_inc(&perf_counters.mutex_protected);
            
            k_mutex_unlock(&counter_mutex);
        }
    }
    
    uint64_t end_time = get_timestamp_ns();
    
    results->mutex_total_ns = end_time - start_time;
    results->mutex_contentions = contentions;
    
    // Update global statistics
    perf_counters.mutex_time_ns += results->mutex_total_ns;
    perf_counters.mutex_iterations += iterations;
}

// Lock-free increment
void lockfree_increment(int iterations, struct benchmark_results *results)
{
    uint64_t start_time = get_timestamp_ns();
    
    for (int i = 0; i < iterations; i++) {
        atomic_inc(&perf_counters.lockfree_counter);
    }
    
    uint64_t end_time = get_timestamp_ns();
    
    results->lockfree_total_ns = end_time - start_time;
    
    // Update global statistics
    perf_counters.lockfree_time_ns += results->lockfree_total_ns;
    perf_counters.lockfree_iterations += iterations;
}

// High-contention benchmark thread
void contention_benchmark_thread(void *arg1, void *arg2, void *arg3)
{
    int thread_id = POINTER_TO_INT(arg1);
    int iterations = POINTER_TO_INT(arg2);
    
    struct benchmark_results results = {0};
    results.thread_id = thread_id;
    
    LOG_INF("Contention benchmark thread %d starting (%d iterations)", 
            thread_id, iterations);
    
    // Wait for all threads to be ready
    k_msleep(100);
    
    // Benchmark spinlock performance
    LOG_DBG("Thread %d: Starting spinlock benchmark", thread_id);
    spinlock_increment(iterations, &results);
    
    // Small delay between benchmarks
    k_msleep(50);
    
    // Benchmark mutex performance
    LOG_DBG("Thread %d: Starting mutex benchmark", thread_id);
    mutex_increment(iterations, &results);
    
    // Small delay between benchmarks
    k_msleep(50);
    
    // Benchmark lock-free performance
    LOG_DBG("Thread %d: Starting lock-free benchmark", thread_id);
    lockfree_increment(iterations, &results);
    
    LOG_INF("Thread %d completed benchmarks:", thread_id);
    LOG_INF("  Spinlock: %llu ns (%u contentions)", 
            results.spinlock_total_ns, results.spinlock_contentions);
    LOG_INF("  Mutex:    %llu ns (%u contentions)", 
            results.mutex_total_ns, results.mutex_contentions);
    LOG_INF("  Lock-free: %llu ns", results.lockfree_total_ns);
}

// Lock-free data structure example: Stack
struct lockfree_stack_node {
    atomic_ptr_t next;
    uint32_t data;
};

struct lockfree_stack {
    atomic_ptr_t head;
    atomic_t size;
};

static struct lockfree_stack test_stack = {
    .head = ATOMIC_PTR_INIT(NULL),
    .size = ATOMIC_INIT(0)
};

// Lock-free stack push
bool lockfree_stack_push(struct lockfree_stack *stack, uint32_t data)
{
    struct lockfree_stack_node *new_node = k_malloc(sizeof(*new_node));
    if (new_node == NULL) {
        return false;
    }
    
    new_node->data = data;
    
    struct lockfree_stack_node *head;
    do {
        head = atomic_ptr_get(&stack->head);
        atomic_ptr_set(&new_node->next, head);
    } while (!atomic_ptr_cas(&stack->head, head, new_node));
    
    atomic_inc(&stack->size);
    return true;
}

// Lock-free stack pop
bool lockfree_stack_pop(struct lockfree_stack *stack, uint32_t *data)
{
    struct lockfree_stack_node *head;
    struct lockfree_stack_node *next;
    
    do {
        head = atomic_ptr_get(&stack->head);
        if (head == NULL) {
            return false;  // Stack is empty
        }
        
        next = atomic_ptr_get(&head->next);
    } while (!atomic_ptr_cas(&stack->head, head, next));
    
    if (data != NULL) {
        *data = head->data;
    }
    
    k_free(head);
    atomic_dec(&stack->size);
    return true;
}

// Lock-free stack test thread
void lockfree_stack_test_thread(void *arg1, void *arg2, void *arg3)
{
    int thread_id = POINTER_TO_INT(arg1);
    int operations = POINTER_TO_INT(arg2);
    
    LOG_INF("Lock-free stack test thread %d starting (%d operations)", 
            thread_id, operations);
    
    uint32_t pushed = 0, popped = 0;
    
    for (int i = 0; i < operations; i++) {
        bool is_push = (sys_rand32_get() % 3) != 0;  // 2/3 push, 1/3 pop
        
        if (is_push) {
            uint32_t data = thread_id * 10000 + i;
            if (lockfree_stack_push(&test_stack, data)) {
                pushed++;
                LOG_DBG("Thread %d pushed %u", thread_id, data);
            }
        } else {
            uint32_t data;
            if (lockfree_stack_pop(&test_stack, &data)) {
                popped++;
                LOG_DBG("Thread %d popped %u", thread_id, data);
            }
        }
        
        // Small delay to create some contention
        if (i % 100 == 0) {
            k_usleep(10);
        }
    }
    
    LOG_INF("Thread %d completed: pushed=%u, popped=%u", 
            thread_id, pushed, popped);
}

// Performance analysis thread
void performance_analysis_thread(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        k_msleep(15000);
        
        LOG_INF("=== SPINLOCK PERFORMANCE ANALYSIS ===");
        
        uint32_t spinlock_count = atomic_get(&perf_counters.spinlock_protected);
        uint32_t mutex_count = atomic_get(&perf_counters.mutex_protected);
        uint32_t lockfree_count = atomic_get(&perf_counters.lockfree_counter);
        
        LOG_INF("Counter values:");
        LOG_INF("  Spinlock protected: %u", spinlock_count);
        LOG_INF("  Mutex protected:    %u", mutex_count);
        LOG_INF("  Lock-free:          %u", lockfree_count);
        
        if (perf_counters.spinlock_iterations > 0) {
            uint64_t avg_spinlock = perf_counters.spinlock_time_ns / 
                                   perf_counters.spinlock_iterations;
            LOG_INF("Average spinlock time: %llu ns/op", avg_spinlock);
        }
        
        if (perf_counters.mutex_iterations > 0) {
            uint64_t avg_mutex = perf_counters.mutex_time_ns / 
                                perf_counters.mutex_iterations;
            LOG_INF("Average mutex time: %llu ns/op", avg_mutex);
        }
        
        if (perf_counters.lockfree_iterations > 0) {
            uint64_t avg_lockfree = perf_counters.lockfree_time_ns / 
                                   perf_counters.lockfree_iterations;
            LOG_INF("Average lock-free time: %llu ns/op", avg_lockfree);
        }
        
        // Stack statistics
        int stack_size = atomic_get(&test_stack.size);
        LOG_INF("Lock-free stack size: %d", stack_size);
        
        LOG_INF("Performance ratios (relative to lock-free):");
        if (perf_counters.lockfree_time_ns > 0) {
            double spinlock_ratio = (double)perf_counters.spinlock_time_ns / 
                                   perf_counters.lockfree_time_ns;
            double mutex_ratio = (double)perf_counters.mutex_time_ns / 
                                perf_counters.lockfree_time_ns;
            
            LOG_INF("  Spinlock: %.2fx", spinlock_ratio);
            LOG_INF("  Mutex:    %.2fx", mutex_ratio);
            LOG_INF("  Lock-free: 1.00x (baseline)");
        }
    }
}

int main(void)
{
    LOG_INF("=== Spinlock and SMP Performance Lab ===");
    
#ifdef CONFIG_SMP
    LOG_INF("SMP enabled with %d CPUs", CONFIG_MP_MAX_NUM_CPUS);
#else
    LOG_INF("Single CPU configuration");
#endif
    
    // Create contention benchmark threads
    static struct k_thread benchmark_threads[CONTENTION_THREADS];
    static K_THREAD_STACK_ARRAY_DEFINE(benchmark_stacks, CONTENTION_THREADS, 2048);
    
    for (int i = 0; i < CONTENTION_THREADS; i++) {
        k_thread_create(&benchmark_threads[i],
                       benchmark_stacks[i],
                       K_THREAD_STACK_SIZEOF(benchmark_stacks[i]),
                       contention_benchmark_thread,
                       INT_TO_POINTER(i + 1),
                       INT_TO_POINTER(BENCHMARK_ITERATIONS / CONTENTION_THREADS),
                       NULL,
                       5, 0, K_NO_WAIT);
    }
    
    // Create lock-free stack test threads
    static struct k_thread stack_threads[3];
    static K_THREAD_STACK_ARRAY_DEFINE(stack_stacks, 3, 2048);
    
    for (int i = 0; i < 3; i++) {
        k_thread_create(&stack_threads[i],
                       stack_stacks[i],
                       K_THREAD_STACK_SIZEOF(stack_stacks[i]),
                       lockfree_stack_test_thread,
                       INT_TO_POINTER(i + 1),
                       INT_TO_POINTER(500),
                       NULL,
                       6, 0, K_NO_WAIT);
    }
    
    // Create performance analysis thread
    static struct k_thread analysis_thread;
    static K_THREAD_STACK_DEFINE(analysis_stack, 2048);
    
    k_thread_create(&analysis_thread,
                   analysis_stack,
                   K_THREAD_STACK_SIZEOF(analysis_stack),
                   performance_analysis_thread,
                   NULL, NULL, NULL,
                   9, 0, K_NO_WAIT);
    
    LOG_INF("All benchmark threads created");
    
    // Main thread performs some operations too
    for (int i = 0; i < 100; i++) {
        // Test all synchronization mechanisms
        k_spinlock_key_t key = k_spin_lock(&counter_spinlock);
        atomic_inc(&perf_counters.spinlock_protected);
        k_spin_unlock(&counter_spinlock, key);
        
        k_mutex_lock(&counter_mutex, K_FOREVER);
        atomic_inc(&perf_counters.mutex_protected);
        k_mutex_unlock(&counter_mutex);
        
        atomic_inc(&perf_counters.lockfree_counter);
        
        // Add some data to the lock-free stack
        lockfree_stack_push(&test_stack, 10000 + i);
        
        k_msleep(100);
    }
    
    LOG_INF("Main thread completed initial operations");
    
    // Keep system running
    while (1) {
        k_msleep(60000);
        LOG_INF("System heartbeat - benchmarks continuing");
    }
    
    return 0;
}
```

Create `lab3_spinlock/prj.conf`:

```ini
# Spinlock and SMP Lab Configuration
CONFIG_MULTITHREADING=y
CONFIG_SMP=y
CONFIG_MP_MAX_NUM_CPUS=2

# High-resolution timing
CONFIG_TIMING_FUNCTIONS=y
CONFIG_TIMING_FUNCTIONS_NEED_AT_BOOT=y

# Performance monitoring
CONFIG_THREAD_RUNTIME_STATS=y
CONFIG_SCHED_THREAD_USAGE=y

# Logging
CONFIG_PRINTK=y
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y

# Memory management
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Random number generation
CONFIG_TEST_RANDOM_GENERATOR=y

# Debugging
CONFIG_DEBUG=y
CONFIG_ASSERT=y
```

## Lab Summary and Analysis

### Key Learning Outcomes

After completing these comprehensive labs, students will have:

1. **Mastered Synchronization Primitives**: Demonstrated proficiency with mutexes, semaphores, spinlocks, and condition variables in real-world scenarios.

2. **Implemented Production Patterns**: Created robust resource management, producer-consumer, and thread coordination patterns suitable for embedded systems.

3. **Analyzed Performance Characteristics**: Measured and compared synchronization overhead under various load conditions and contention scenarios.

4. **Developed Debugging Skills**: Used Zephyr's logging and monitoring capabilities to analyze synchronization behavior and identify bottlenecks.

5. **Applied Advanced Techniques**: Implemented priority inheritance, lock-free data structures, and SMP-aware synchronization patterns.

### Performance Insights

The labs reveal important performance characteristics:

- **Spinlocks**: Best for short critical sections with low contention (< 1μs overhead)
- **Mutexes**: Suitable for longer critical sections with priority inheritance (5-50μs overhead)
- **Semaphores**: Optimal for resource pooling and signaling (2-10μs overhead)
- **Lock-free**: Highest performance but limited applicability (< 100ns overhead)

### Real-World Applications

These patterns directly apply to:

- **Industrial Control**: Coordinating sensor readings, actuator control, and safety systems
- **Automotive**: Managing ADAS components, engine control, and infotainment systems
- **Medical Devices**: Synchronizing monitoring, alerting, and therapeutic functions
- **IoT Systems**: Balancing data collection, processing, and communication activities

### Next Steps

Students should now be prepared to:
- Design complex synchronization architectures for embedded systems
- Optimize performance while maintaining correctness and real-time behavior
- Debug and troubleshoot concurrency issues in production systems
- Apply advanced synchronization patterns in specialized domains

This comprehensive laboratory experience provides the practical foundation for professional embedded systems development using Zephyr's traditional multithreading primitives.