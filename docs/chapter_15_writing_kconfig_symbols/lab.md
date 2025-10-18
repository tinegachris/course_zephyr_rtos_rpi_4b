# Chapter 15 - Writing Kconfig Symbols Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Lab Overview

This lab teaches you to create comprehensive Kconfig configurations for Zephyr modules. You'll build a complete data logging module with sophisticated configuration options, demonstrating professional Kconfig design patterns and integration techniques.

## Learning Objectives

By completing this lab, you will:

* Design and implement complex Kconfig hierarchies
* Create maintainable configuration dependencies
* Integrate Kconfig with source code effectively
* Validate configuration combinations
* Apply professional Kconfig best practices
* Build configurable, modular embedded software

## Lab Setup

### Required Tools

* Zephyr SDK and development environment
* West build tool
* Text editor with Kconfig syntax highlighting
* Development board for testing

### Project Structure

We'll create a data logging module with comprehensive configuration:

```text
data_logger_module/
├── Kconfig
├── CMakeLists.txt
├── src/
│   ├── data_logger.c
│   ├── storage_backend.c
│   └── compression.c
├── include/
│   └── data_logger/
│       └── data_logger.h
├── configs/
│   ├── minimal.conf
│   ├── full_featured.conf
│   └── performance.conf
└── tests/
    └── test_configs/
        ├── test_minimal.conf
        └── test_full.conf
```

## Part 1: Basic Kconfig Structure (30 minutes)

### Step 1: Create Base Module Configuration

Create the main `Kconfig` file:

```kconfig
# Data Logger Module Configuration

menuconfig DATA_LOGGER
    bool "Data Logger Module"
    help
      Enable data logging functionality with configurable storage
      backends, compression options, and performance tuning.
      
      This module provides a flexible framework for logging
      sensor data, system events, and application-specific
      information with various storage and processing options.

if DATA_LOGGER

# Basic configuration options
config DATA_LOGGER_LOG_LEVEL
    int "Module log level"
    default 3
    range 0 4
    help
      Logging level for the data logger module:
      0 = Off (no logging)
      1 = Error messages only
      2 = Error and warning messages
      3 = Error, warning, and info messages
      4 = All messages including debug

config DATA_LOGGER_THREAD_STACK_SIZE
    int "Thread stack size (bytes)"
    default 2048
    range 1024 8192
    help
      Stack size for the data logger thread. Larger stacks
      support more complex processing but use more memory.
      
      Recommended minimum: 1024 bytes
      For compression features: 2048+ bytes
      For network storage: 4096+ bytes

config DATA_LOGGER_THREAD_PRIORITY
    int "Thread priority"
    default 7
    range 0 15
    help
      Priority of the data logger thread. Lower numbers
      indicate higher priority.
      
      Recommended: 5-10 for most applications
      Real-time logging: 0-4
      Background logging: 8-15

config DATA_LOGGER_QUEUE_SIZE
    int "Log entry queue size"
    default 32
    range 8 256
    help
      Number of log entries that can be queued before
      processing. Larger queues handle burst logging
      better but use more memory.
      
      Memory usage: queue_size * entry_size
      Typical entry size: 64-128 bytes

endif # DATA_LOGGER
```

### Step 2: Add Storage Backend Configuration

Extend the Kconfig with storage options:

```kconfig
# Storage backend configuration (add to existing Kconfig)

if DATA_LOGGER

choice DATA_LOGGER_STORAGE_BACKEND
    prompt "Primary storage backend"
    default DATA_LOGGER_STORAGE_FLASH
    help
      Select the primary storage backend for log data.
      Multiple backends can be enabled simultaneously.

config DATA_LOGGER_STORAGE_FLASH
    bool "Flash storage"
    help
      Store log data in on-chip or external flash memory.
      Provides non-volatile storage with good performance.

config DATA_LOGGER_STORAGE_RAM
    bool "RAM storage (volatile)"
    help
      Store log data in RAM. Fastest option but data
      is lost on power cycle. Useful for debugging.

config DATA_LOGGER_STORAGE_SD
    bool "SD card storage"
    depends on DISK_ACCESS_SD
    help
      Store log data on SD card. Provides large storage
      capacity but requires SD card interface.

config DATA_LOGGER_STORAGE_NETWORK
    bool "Network storage"
    depends on NETWORKING
    help
      Send log data over network (TCP/UDP). Requires
      network connectivity and remote logging server.

endchoice

# Flash storage specific options
if DATA_LOGGER_STORAGE_FLASH

config DATA_LOGGER_FLASH_PARTITION_SIZE
    hex "Flash partition size"
    default 0x10000
    help
      Size of flash partition reserved for log storage.
      Must be aligned to flash sector boundaries.

config DATA_LOGGER_FLASH_WEAR_LEVELING
    bool "Enable wear leveling"
    default y
    help
      Enable wear leveling to extend flash lifetime.
      Recommended for applications with frequent logging.

endif # DATA_LOGGER_STORAGE_FLASH

# RAM storage specific options
if DATA_LOGGER_STORAGE_RAM

config DATA_LOGGER_RAM_BUFFER_SIZE
    int "RAM buffer size (bytes)"
    default 4096
    range 1024 32768
    help
      Size of RAM buffer for log storage. Larger buffers
      store more data but use more memory.

config DATA_LOGGER_RAM_CIRCULAR_BUFFER
    bool "Use circular buffer"
    default y
    help
      Use circular buffer that overwrites oldest data
      when full. Disable to stop logging when full.

endif # DATA_LOGGER_STORAGE_RAM

# Network storage specific options
if DATA_LOGGER_STORAGE_NETWORK

config DATA_LOGGER_NETWORK_SERVER_IP
    string "Log server IP address"
    default "192.168.1.100"
    help
      IP address of the remote logging server.

config DATA_LOGGER_NETWORK_SERVER_PORT
    int "Log server port"
    default 514
    range 1 65535
    help
      Port number for the remote logging server.
      514 is the standard syslog port.

config DATA_LOGGER_NETWORK_PROTOCOL
    choice
    prompt "Network protocol"
    default DATA_LOGGER_NETWORK_UDP

config DATA_LOGGER_NETWORK_UDP
    bool "UDP"
    help
      Use UDP for fast, connectionless logging.
      Some log entries may be lost in poor network conditions.

config DATA_LOGGER_NETWORK_TCP
    bool "TCP"
    help
      Use TCP for reliable logging with guaranteed delivery.
      Higher overhead but ensures all logs are received.

endchoice

endif # DATA_LOGGER_STORAGE_NETWORK

endif # DATA_LOGGER
```

## Part 2: Advanced Features and Dependencies (30 minutes)

### Step 3: Add Compression and Processing Features

```kconfig
# Advanced features (add to existing Kconfig)

if DATA_LOGGER

menuconfig DATA_LOGGER_ADVANCED_FEATURES
    bool "Advanced Features"
    help
      Enable advanced data processing and optimization features.
      These features provide enhanced functionality but increase
      memory usage and processing overhead.

if DATA_LOGGER_ADVANCED_FEATURES

config DATA_LOGGER_COMPRESSION
    bool "Enable data compression"
    help
      Compress log data to reduce storage requirements.
      Uses CPU cycles but can significantly reduce storage needs.

if DATA_LOGGER_COMPRESSION

choice DATA_LOGGER_COMPRESSION_ALGORITHM
    prompt "Compression algorithm"
    default DATA_LOGGER_COMPRESSION_LZ77

config DATA_LOGGER_COMPRESSION_LZ77
    bool "LZ77 compression"
    help
      Fast compression with moderate compression ratio.
      Good balance of speed and space savings.

config DATA_LOGGER_COMPRESSION_HUFFMAN
    bool "Huffman encoding"
    help
      Good compression for text data with repeated patterns.
      Lower CPU usage than LZ77.

config DATA_LOGGER_COMPRESSION_CUSTOM
    bool "Custom compression"
    help
      Use application-specific compression algorithm.
      Requires implementation of compression callbacks.

endchoice

config DATA_LOGGER_COMPRESSION_LEVEL
    int "Compression level"
    default 3
    range 1 9
    help
      Compression level (1=fast, 9=best compression).
      Higher levels use more CPU but achieve better compression.

endif # DATA_LOGGER_COMPRESSION

config DATA_LOGGER_ENCRYPTION
    bool "Enable data encryption"
    select CRYPTO
    help
      Encrypt log data for secure storage and transmission.
      Requires crypto subsystem and increases processing overhead.

if DATA_LOGGER_ENCRYPTION

choice DATA_LOGGER_ENCRYPTION_ALGORITHM
    prompt "Encryption algorithm"
    default DATA_LOGGER_ENCRYPTION_AES128

config DATA_LOGGER_ENCRYPTION_AES128
    bool "AES-128"
    help
      AES encryption with 128-bit keys. Good security
      with reasonable performance.

config DATA_LOGGER_ENCRYPTION_AES256
    bool "AES-256"
    help
      AES encryption with 256-bit keys. Higher security
      but increased processing overhead.

endchoice

config DATA_LOGGER_ENCRYPTION_KEY_SOURCE
    choice
    prompt "Encryption key source"
    default DATA_LOGGER_KEY_BUILTIN

config DATA_LOGGER_KEY_BUILTIN
    bool "Built-in key"
    help
      Use compile-time embedded encryption key.
      Convenient but less secure.

config DATA_LOGGER_KEY_EXTERNAL
    bool "External key source"
    help
      Load encryption key from external source at runtime.
      More secure but requires key management implementation.

endchoice

endif # DATA_LOGGER_ENCRYPTION

config DATA_LOGGER_FILTERING
    bool "Enable log filtering"
    help
      Filter log entries based on level, source, or custom criteria.
      Reduces storage requirements and processing overhead.

if DATA_LOGGER_FILTERING

config DATA_LOGGER_FILTER_BY_LEVEL
    bool "Filter by log level"
    default y
    help
      Only store log entries above specified level.

if DATA_LOGGER_FILTER_BY_LEVEL

config DATA_LOGGER_MIN_LOG_LEVEL
    int "Minimum log level to store"
    default 2
    range 0 4
    help
      Minimum log level to store (0=all, 4=critical only).

endif # DATA_LOGGER_FILTER_BY_LEVEL

config DATA_LOGGER_FILTER_BY_SOURCE
    bool "Filter by log source"
    help
      Only store logs from specific modules or sources.

config DATA_LOGGER_CUSTOM_FILTER
    bool "Enable custom filtering"
    help
      Allow application-specific filtering logic.
      Requires implementation of filter callback functions.

endif # DATA_LOGGER_FILTERING

endif # DATA_LOGGER_ADVANCED_FEATURES

endif # DATA_LOGGER
```

## Part 3: Performance and Debug Configuration (25 minutes)

### Step 4: Add Performance Tuning Options

```kconfig
# Performance configuration (add to existing Kconfig)

if DATA_LOGGER

menuconfig DATA_LOGGER_PERFORMANCE
    bool "Performance Tuning"
    help
      Advanced performance configuration options for
      optimizing throughput, latency, and resource usage.

if DATA_LOGGER_PERFORMANCE

config DATA_LOGGER_BATCH_SIZE
    int "Batch processing size"
    default 8
    range 1 64
    help
      Number of log entries to process in a single batch.
      Larger batches improve throughput but increase latency.
      
      Recommended:
      - Real-time applications: 1-4
      - High throughput: 16-32
      - Memory constrained: 4-8

config DATA_LOGGER_FLUSH_INTERVAL_MS
    int "Flush interval (milliseconds)"
    default 1000
    range 100 30000
    help
      Maximum time to wait before flushing buffered data
      to storage. Shorter intervals reduce data loss risk
      but increase storage overhead.

config DATA_LOGGER_ASYNC_PROCESSING
    bool "Enable asynchronous processing"
    default y
    help
      Process log entries asynchronously in background thread.
      Improves application responsiveness but uses more memory.

if DATA_LOGGER_ASYNC_PROCESSING

config DATA_LOGGER_ASYNC_QUEUE_SIZE
    int "Async processing queue size"
    default 64
    range 16 512
    help
      Size of queue for asynchronous log processing.
      Larger queues handle burst logging better.

endif # DATA_LOGGER_ASYNC_PROCESSING

config DATA_LOGGER_ZERO_COPY
    bool "Enable zero-copy optimization"
    depends on DATA_LOGGER_STORAGE_RAM
    help
      Use zero-copy techniques to reduce memory copying overhead.
      Only available with RAM storage backend.

config DATA_LOGGER_DMA_TRANSFERS
    bool "Use DMA for storage transfers"
    depends on DMA
    help
      Use DMA for high-speed data transfers to storage.
      Reduces CPU overhead but requires DMA support.

endif # DATA_LOGGER_PERFORMANCE

menuconfig DATA_LOGGER_DEBUG
    bool "Debug and Diagnostics"
    help
      Enable debugging and diagnostic features for
      development and troubleshooting.

if DATA_LOGGER_DEBUG

config DATA_LOGGER_STATISTICS
    bool "Enable runtime statistics"
    help
      Collect and report runtime statistics including
      throughput, queue utilization, and error counts.

config DATA_LOGGER_SELF_TEST
    bool "Enable self-test functionality"
    help
      Include self-test routines to verify module functionality.
      Useful for development and system validation.

config DATA_LOGGER_TRACE
    bool "Enable execution tracing"
    help
      Add tracing points for debugging and performance analysis.
      Increases code size and runtime overhead.

config DATA_LOGGER_ASSERT_ENABLED
    bool "Enable assertions"
    default y
    help
      Enable runtime assertions for parameter validation
      and error detection. Disable for production builds.

endif # DATA_LOGGER_DEBUG

endif # DATA_LOGGER
```

## Part 4: Source Code Integration (30 minutes)

### Step 5: Create Header File

Create `include/data_logger/data_logger.h`:

```c
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file data_logger.h
 * @brief Data Logger Module API
 * 
 * Configurable data logging module with multiple storage backends,
 * compression, encryption, and performance optimization features.
 */

/* Configuration-dependent includes */
#ifdef CONFIG_DATA_LOGGER_COMPRESSION
#include <zephyr/sys/crc.h>
#endif

#ifdef CONFIG_DATA_LOGGER_ENCRYPTION
#include <zephyr/crypto/crypto.h>
#endif

/**
 * @brief Log entry structure
 */
struct log_entry {
    uint32_t timestamp;
    uint16_t level;
    uint16_t source_id;
    uint16_t data_length;
    uint8_t data[CONFIG_DATA_LOGGER_QUEUE_SIZE];
} __packed;

/**
 * @brief Data logger configuration
 */
struct data_logger_config {
    uint32_t queue_size;
    uint32_t thread_stack_size;
    int thread_priority;
    uint32_t flush_interval_ms;
    bool async_processing;
    
    #ifdef CONFIG_DATA_LOGGER_COMPRESSION
    uint8_t compression_level;
    #endif
    
    #ifdef CONFIG_DATA_LOGGER_ENCRYPTION
    uint8_t encryption_key[32];
    #endif
};

/**
 * @brief Initialize data logger
 * 
 * @param config Logger configuration
 * @return 0 on success, negative errno on failure
 */
int data_logger_init(const struct data_logger_config *config);

/**
 * @brief Log data entry
 * 
 * @param level Log level
 * @param source_id Source identifier
 * @param data Data to log
 * @param length Data length
 * @return 0 on success, negative errno on failure
 */
int data_logger_log(uint16_t level, uint16_t source_id, 
                   const void *data, size_t length);

/**
 * @brief Flush pending log entries
 * 
 * @return 0 on success, negative errno on failure
 */
int data_logger_flush(void);

/**
 * @brief Get runtime statistics
 * 
 * @param stats Pointer to statistics structure
 * @return 0 on success, negative errno on failure
 */
#ifdef CONFIG_DATA_LOGGER_STATISTICS
struct data_logger_stats {
    uint32_t entries_logged;
    uint32_t bytes_stored;
    uint32_t compression_ratio;
    uint32_t queue_high_water;
    uint32_t storage_errors;
};

int data_logger_get_stats(struct data_logger_stats *stats);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DATA_LOGGER_H */
```

### Step 6: Implement Core Module

Create `src/data_logger.c`:

```c
#include <data_logger/data_logger.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(data_logger, CONFIG_DATA_LOGGER_LOG_LEVEL);

/* Configuration-dependent definitions */
#define THREAD_STACK_SIZE CONFIG_DATA_LOGGER_THREAD_STACK_SIZE
#define THREAD_PRIORITY CONFIG_DATA_LOGGER_THREAD_PRIORITY
#define QUEUE_SIZE CONFIG_DATA_LOGGER_QUEUE_SIZE
#define FLUSH_INTERVAL K_MSEC(CONFIG_DATA_LOGGER_FLUSH_INTERVAL_MS)

/* Static data structures */
static struct k_thread logger_thread;
static K_THREAD_STACK_DEFINE(logger_stack, THREAD_STACK_SIZE);

static struct k_msgq log_queue;
static char queue_buffer[QUEUE_SIZE * sizeof(struct log_entry)];

static struct k_timer flush_timer;
static struct k_work flush_work;

#ifdef CONFIG_DATA_LOGGER_STATISTICS
static struct data_logger_stats runtime_stats;
#endif

/* Forward declarations */
static void logger_thread_func(void *arg1, void *arg2, void *arg3);
static void flush_timer_handler(struct k_timer *timer);
static void flush_work_handler(struct k_work *work);

int data_logger_init(const struct data_logger_config *config)
{
    if (!config) {
        LOG_ERR("Invalid configuration");
        return -EINVAL;
    }

    LOG_INF("Initializing data logger");
    LOG_INF("Queue size: %d entries", QUEUE_SIZE);
    LOG_INF("Thread stack: %d bytes", THREAD_STACK_SIZE);
    LOG_INF("Thread priority: %d", THREAD_PRIORITY);

    /* Initialize message queue */
    k_msgq_init(&log_queue, queue_buffer, sizeof(struct log_entry), QUEUE_SIZE);

    /* Initialize timer and work */
    k_timer_init(&flush_timer, flush_timer_handler, NULL);
    k_work_init(&flush_work, flush_work_handler);

    /* Start background thread */
    k_thread_create(&logger_thread, logger_stack, THREAD_STACK_SIZE,
                    logger_thread_func, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&logger_thread, "data_logger");

    /* Start periodic flush timer */
    k_timer_start(&flush_timer, FLUSH_INTERVAL, FLUSH_INTERVAL);

    #ifdef CONFIG_DATA_LOGGER_STATISTICS
    memset(&runtime_stats, 0, sizeof(runtime_stats));
    #endif

    LOG_INF("Data logger initialized successfully");

    #ifdef CONFIG_DATA_LOGGER_SELF_TEST
    /* Run self-test if enabled */
    if (data_logger_self_test() != 0) {
        LOG_WRN("Self-test failed");
    }
    #endif

    return 0;
}

int data_logger_log(uint16_t level, uint16_t source_id, 
                   const void *data, size_t length)
{
    #ifdef CONFIG_DATA_LOGGER_ASSERT_ENABLED
    __ASSERT(data != NULL, "Data cannot be null");
    __ASSERT(length > 0, "Length must be greater than zero");
    #endif

    /* Check if filtering is enabled */
    #ifdef CONFIG_DATA_LOGGER_FILTER_BY_LEVEL
    if (level < CONFIG_DATA_LOGGER_MIN_LOG_LEVEL) {
        return 0; /* Filtered out */
    }
    #endif

    struct log_entry entry = {
        .timestamp = k_uptime_get_32(),
        .level = level,
        .source_id = source_id,
        .data_length = MIN(length, sizeof(entry.data))
    };

    memcpy(entry.data, data, entry.data_length);

    /* Try to add to queue */
    int ret = k_msgq_put(&log_queue, &entry, K_NO_WAIT);
    if (ret != 0) {
        LOG_WRN("Log queue full, dropping entry");
        #ifdef CONFIG_DATA_LOGGER_STATISTICS
        runtime_stats.storage_errors++;
        #endif
        return -ENOMEM;
    }

    #ifdef CONFIG_DATA_LOGGER_STATISTICS
    runtime_stats.entries_logged++;
    runtime_stats.bytes_stored += entry.data_length;
    
    /* Update high water mark */
    uint32_t used = k_msgq_num_used_get(&log_queue);
    if (used > runtime_stats.queue_high_water) {
        runtime_stats.queue_high_water = used;
    }
    #endif

    return 0;
}

/* Background thread for processing log entries */
static void logger_thread_func(void *arg1, void *arg2, void *arg3)
{
    struct log_entry entry;
    
    LOG_INF("Data logger thread started");
    
    while (1) {
        /* Wait for log entry */
        if (k_msgq_get(&log_queue, &entry, K_FOREVER) == 0) {
            
            #ifdef CONFIG_DATA_LOGGER_TRACE
            LOG_DBG("Processing log entry: level=%d, source=%d, len=%d",
                    entry.level, entry.source_id, entry.data_length);
            #endif
            
            /* Process the entry based on configuration */
            process_log_entry(&entry);
        }
    }
}

/* Helper function to process log entries */
static void process_log_entry(struct log_entry *entry)
{
    uint8_t *processed_data = entry->data;
    size_t processed_length = entry->data_length;
    
    #ifdef CONFIG_DATA_LOGGER_COMPRESSION
    /* Apply compression if enabled */
    if (compress_data(entry->data, entry->data_length, 
                     compressed_buffer, &compressed_length) == 0) {
        processed_data = compressed_buffer;
        processed_length = compressed_length;
        
        #ifdef CONFIG_DATA_LOGGER_STATISTICS
        runtime_stats.compression_ratio = 
            (entry->data_length * 100) / compressed_length;
        #endif
    }
    #endif
    
    #ifdef CONFIG_DATA_LOGGER_ENCRYPTION
    /* Apply encryption if enabled */
    if (encrypt_data(processed_data, processed_length,
                    encrypted_buffer, &encrypted_length) == 0) {
        processed_data = encrypted_buffer;
        processed_length = encrypted_length;
    }
    #endif
    
    /* Store to selected backend */
    #ifdef CONFIG_DATA_LOGGER_STORAGE_FLASH
    store_to_flash(entry, processed_data, processed_length);
    #elif defined(CONFIG_DATA_LOGGER_STORAGE_RAM)
    store_to_ram(entry, processed_data, processed_length);
    #elif defined(CONFIG_DATA_LOGGER_STORAGE_SD)
    store_to_sd(entry, processed_data, processed_length);
    #elif defined(CONFIG_DATA_LOGGER_STORAGE_NETWORK)
    store_to_network(entry, processed_data, processed_length);
    #endif
}

/* Timer and work handlers for periodic flushing */
static void flush_timer_handler(struct k_timer *timer)
{
    k_work_submit(&flush_work);
}

static void flush_work_handler(struct k_work *work)
{
    data_logger_flush();
}

int data_logger_flush(void)
{
    LOG_DBG("Flushing log data");
    
    /* Implementation depends on storage backend */
    #ifdef CONFIG_DATA_LOGGER_STORAGE_FLASH
    return flash_flush();
    #elif defined(CONFIG_DATA_LOGGER_STORAGE_SD)
    return sd_flush();
    #elif defined(CONFIG_DATA_LOGGER_STORAGE_NETWORK)
    return network_flush();
    #else
    return 0; /* RAM storage doesn't need flushing */
    #endif
}

#ifdef CONFIG_DATA_LOGGER_STATISTICS
int data_logger_get_stats(struct data_logger_stats *stats)
{
    if (!stats) {
        return -EINVAL;
    }
    
    memcpy(stats, &runtime_stats, sizeof(*stats));
    return 0;
}
#endif
```

## Part 5: Testing and Validation (20 minutes)

### Step 7: Create Test Configurations

Create `configs/minimal.conf`:

```conf
# Minimal configuration for memory-constrained systems
CONFIG_DATA_LOGGER=y
CONFIG_DATA_LOGGER_LOG_LEVEL=1
CONFIG_DATA_LOGGER_THREAD_STACK_SIZE=1024
CONFIG_DATA_LOGGER_THREAD_PRIORITY=10
CONFIG_DATA_LOGGER_QUEUE_SIZE=8
CONFIG_DATA_LOGGER_STORAGE_RAM=y
CONFIG_DATA_LOGGER_RAM_BUFFER_SIZE=2048
CONFIG_DATA_LOGGER_RAM_CIRCULAR_BUFFER=y
CONFIG_DATA_LOGGER_FLUSH_INTERVAL_MS=5000
```

Create `configs/full_featured.conf`:

```conf
# Full-featured configuration with all options
CONFIG_DATA_LOGGER=y
CONFIG_DATA_LOGGER_LOG_LEVEL=4
CONFIG_DATA_LOGGER_THREAD_STACK_SIZE=4096
CONFIG_DATA_LOGGER_THREAD_PRIORITY=5
CONFIG_DATA_LOGGER_QUEUE_SIZE=64
CONFIG_DATA_LOGGER_STORAGE_FLASH=y
CONFIG_DATA_LOGGER_FLASH_PARTITION_SIZE=0x20000
CONFIG_DATA_LOGGER_FLASH_WEAR_LEVELING=y
CONFIG_DATA_LOGGER_ADVANCED_FEATURES=y
CONFIG_DATA_LOGGER_COMPRESSION=y
CONFIG_DATA_LOGGER_COMPRESSION_LZ77=y
CONFIG_DATA_LOGGER_COMPRESSION_LEVEL=5
CONFIG_DATA_LOGGER_ENCRYPTION=y
CONFIG_DATA_LOGGER_ENCRYPTION_AES128=y
CONFIG_DATA_LOGGER_KEY_BUILTIN=y
CONFIG_DATA_LOGGER_FILTERING=y
CONFIG_DATA_LOGGER_FILTER_BY_LEVEL=y
CONFIG_DATA_LOGGER_MIN_LOG_LEVEL=2
CONFIG_DATA_LOGGER_PERFORMANCE=y
CONFIG_DATA_LOGGER_BATCH_SIZE=16
CONFIG_DATA_LOGGER_FLUSH_INTERVAL_MS=1000
CONFIG_DATA_LOGGER_ASYNC_PROCESSING=y
CONFIG_DATA_LOGGER_ASYNC_QUEUE_SIZE=128
CONFIG_DATA_LOGGER_DEBUG=y
CONFIG_DATA_LOGGER_STATISTICS=y
CONFIG_DATA_LOGGER_SELF_TEST=y
CONFIG_DATA_LOGGER_ASSERT_ENABLED=y
```

### Step 8: Build and Test Different Configurations

Test the minimal configuration:

```bash
# Copy minimal configuration
cp configs/minimal.conf prj.conf

# Build and test
west build -b qemu_x86 --pristine
west build -t run
```

Test the full-featured configuration:

```bash
# Copy full configuration
cp configs/full_featured.conf prj.conf

# Build and test
west build -b qemu_x86 --pristine
west build -t run
```

## Lab Exercises

### Exercise 1: Add Custom Storage Backend

Create a new storage backend option with its own configuration parameters.

### Exercise 2: Implement Configuration Validation

Add Kconfig logic to validate configuration combinations and prevent invalid settings.

### Exercise 3: Create Profile Configurations

Design configuration profiles for specific use cases (IoT sensor, automotive, medical device).

### Exercise 4: Add Runtime Configuration

Implement runtime configuration changes through a configuration API.

## Expected Outcomes

Upon completion, you will have:

1. **Comprehensive Kconfig Structure**: A complete, hierarchical configuration system
2. **Professional Dependencies**: Proper use of dependencies and selections
3. **Source Integration**: Seamless integration between Kconfig and C code
4. **Configuration Validation**: Working test configurations for different scenarios
5. **Best Practices**: Understanding of professional Kconfig design patterns

## Troubleshooting Guide

| Issue | Cause | Solution |
|-------|-------|----------|
| Build fails with undefined CONFIG_ | Missing Kconfig definition | Check symbol name and add to Kconfig |
| Option not visible in menuconfig | Missing dependencies | Verify `depends on` clauses |
| Default values not applied | Incorrect syntax | Check default value format |
| Circular dependencies | Conflicting select/depends | Review dependency chain |

This lab demonstrates professional-grade Kconfig design for configurable embedded systems using Zephyr RTOS.

[Next: Chapter 16 - Device Driver Architecture](../chapter_16_device_driver_architecture/README.md)
