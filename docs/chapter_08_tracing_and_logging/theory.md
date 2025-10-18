# Chapter 8: Tracing and Logging - Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

This theory section provides comprehensive understanding of Zephyr's tracing and logging capabilities, covering modern logging APIs, thread analysis, performance monitoring, and professional debugging workflows for embedded systems development.

---

## Zephyr Logging System Architecture

### Modern Logging Framework

Zephyr's logging system provides a sophisticated framework for capturing, processing, and outputting diagnostic information. The system is designed for minimal runtime overhead while providing comprehensive debugging capabilities.

**Core Architecture Components:**

```c
#include <zephyr/logging/log.h>

// Module registration - must be done once per source file
LOG_MODULE_REGISTER(sensor_manager, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("System initialization starting");
    LOG_DBG("Debug level: %d", CONFIG_LOG_DEFAULT_LEVEL);
    
    // Application logic with integrated logging
    if (initialize_sensors() < 0) {
        LOG_ERR("Sensor initialization failed");
        return -1;
    }
    
    LOG_INF("System ready - all subsystems initialized");
    return 0;
}
```

**Logging Levels and Control:**

```c
// Logging levels (from highest to lowest priority)
// LOG_LEVEL_OFF    (0) - No logging
// LOG_LEVEL_ERR    (1) - Error conditions only
// LOG_LEVEL_WRN    (2) - Warnings and errors
// LOG_LEVEL_INF    (3) - Informational messages
// LOG_LEVEL_DBG    (4) - Debug information

// Dynamic level control
void adjust_logging_verbosity(int system_load)
{
    if (system_load > 80) {
        // Reduce logging under high load
        log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID, 
                      log_source_id_get("sensor_manager"), 
                      LOG_LEVEL_ERR);
        LOG_WRN("System load high - reducing log verbosity");
    } else {
        // Full debugging when resources available
        log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                      log_source_id_get("sensor_manager"),
                      LOG_LEVEL_DBG);
    }
}
```

### Backend Configuration and Management

**Console Backend for Development:**

```c
// prj.conf configuration for development
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_LOG_BACKEND_UART=y
CONFIG_LOG_PRINTK=y
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_BUFFER_SIZE=4096
CONFIG_LOG_PROCESS_THREAD_STACK_SIZE=2048

// Advanced logging features
CONFIG_LOG_FUNC_NAME_PREFIX_ERR=y
CONFIG_LOG_FUNC_NAME_PREFIX_WRN=y
CONFIG_LOG_TIMESTAMP_64BIT=y
```

**Production Logging Configuration:**

```c
// overlay-production.conf for deployed systems
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=2
CONFIG_LOG_BACKEND_UART=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_PRINTK=n

// Minimize overhead in production
CONFIG_LOG_FUNC_NAME_PREFIX_ERR=n
CONFIG_LOG_FUNC_NAME_PREFIX_WRN=n
CONFIG_LOG_BUFFER_SIZE=1024
```

**Multi-Backend Logging Implementation:**

```c
#include <zephyr/logging/log_ctrl.h>

void configure_logging_backends(void)
{
    // Initialize logging subsystem
    log_init();
    
    // Configure UART backend for immediate output
    STRUCT_SECTION_FOREACH(log_backend, backend) {
        if (strcmp(backend->name, "uart") == 0) {
            log_backend_enable(backend, backend->cb->ctx, LOG_LEVEL_DBG);
            LOG_INF("UART logging backend enabled");
        }
    }
    
    // Set custom timestamp function for precise timing
    log_set_timestamp_func(k_cycle_get_32, sys_clock_hw_cycles_per_sec());
}
```

---

## Thread Analyzer System

### Comprehensive Thread Monitoring

The Thread Analyzer provides real-time insight into thread behavior, stack usage, and CPU utilization critical for embedded system optimization.

**Thread Analyzer Configuration:**

```c
// Thread analyzer configuration (prj.conf)
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_USE_LOG=y
CONFIG_THREAD_ANALYZER_AUTO=y
CONFIG_THREAD_ANALYZER_AUTO_INTERVAL=5
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y

// Stack monitoring configuration
CONFIG_INIT_STACKS=y
CONFIG_THREAD_MONITOR=y
CONFIG_KERNEL_SHELL=y
```

**Programmatic Thread Analysis:**

```c
#include <zephyr/debug/thread_analyzer.h>

void system_health_monitor(void)
{
    LOG_INF("=== System Thread Analysis ===");
    
    // Automatic analysis with callback
    thread_analyzer_run(custom_thread_analysis_cb, 0);
    
    // Manual analysis output
    thread_analyzer_print(0);
}

void custom_thread_analysis_cb(struct thread_analyzer_info *info)
{
    size_t stack_used_percent = (info->stack_used * 100) / info->stack_size;
    
    LOG_INF("Thread: %s", info->name);
    LOG_INF("  Stack: %zu/%zu bytes (%zu%% used)", 
            info->stack_used, info->stack_size, stack_used_percent);
    
    // Alert on high stack usage
    if (stack_used_percent > 80) {
        LOG_WRN("High stack usage in thread %s: %zu%%", 
                info->name, stack_used_percent);
    }
    
#ifdef CONFIG_THREAD_RUNTIME_STATS
    LOG_INF("  CPU utilization: %u%%", info->utilization);
    
    if (info->utilization > 90) {
        LOG_ERR("Thread %s consuming excessive CPU: %u%%",
                info->name, info->utilization);
    }
#endif
}
```

### Runtime Thread Inspection

**Shell Integration for Live Monitoring:**

```c
// Shell configuration for runtime inspection
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
CONFIG_KERNEL_SHELL=y
CONFIG_SHELL_CMDS=y
CONFIG_SHELL_CMDS_RESIZE=y

// Shell commands available at runtime:
// kernel thread list       - List all threads
// kernel thread stacks     - Show stack usage
// kernel thread suspend <id> - Suspend thread
// kernel thread resume <id>  - Resume thread
```

**Custom Shell Commands for System Monitoring:**

```c
#include <zephyr/shell/shell.h>

static int cmd_system_health(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== System Health Report ===");
    
    // Memory usage
    shell_print(sh, "Heap usage: %zu bytes", k_heap_free_get(&k_malloc_heap));
    
    // Thread analysis
    thread_analyzer_run(shell_thread_analysis_cb, (unsigned int)sh);
    
    return 0;
}

static void shell_thread_analysis_cb(struct thread_analyzer_info *info)
{
    const struct shell *sh = (const struct shell *)info;
    size_t stack_percent = (info->stack_used * 100) / info->stack_size;
    
    shell_print(sh, "%-16s: Stack %3zu%% CPU %3u%%", 
                info->name, stack_percent, info->utilization);
}

SHELL_CMD_REGISTER(health, NULL, "System health report", cmd_system_health);
```

---

## Performance Analysis and Optimization

### Logging Performance Impact

**Measuring Logging Overhead:**

```c
#include <zephyr/timing/timing.h>

void measure_logging_performance(void)
{
    timing_t start_time, end_time;
    uint64_t cycles;
    
    // Measure logging overhead
    timing_start();
    start_time = timing_counter_get();
    
    // Test different logging scenarios
    LOG_INF("Performance test message with parameter: %d", 42);
    
    end_time = timing_counter_get();
    cycles = timing_cycles_get(&start_time, &end_time);
    
    LOG_INF("Logging overhead: %llu cycles (%llu us)", 
            cycles, timing_cycles_to_ns(cycles) / 1000);
}

void optimize_logging_for_performance(void)
{
    // Conditional compilation for performance-critical sections
    #if CONFIG_LOG_DEFAULT_LEVEL >= LOG_LEVEL_DBG
        uint32_t debug_start = k_cycle_get_32();
        // Debug-only processing
        uint32_t debug_cycles = k_cycle_get_32() - debug_start;
        LOG_DBG("Debug processing took %u cycles", debug_cycles);
    #endif
    
    // Use log level checking to avoid expensive operations
    if (LOG_LEVEL_DBG <= CONFIG_LOG_DEFAULT_LEVEL) {
        char detailed_status[256];
        generate_detailed_status_string(detailed_status, sizeof(detailed_status));
        LOG_DBG("System status: %s", detailed_status);
    }
}
```

**Memory Usage Optimization:**

```c
// Efficient logging for memory-constrained systems
void memory_efficient_logging(void)
{
    // Use immediate mode for critical systems
    #ifdef CONFIG_LOG_MODE_IMMEDIATE
        LOG_ERR("Critical error - immediate logging active");
    #endif
    
    // Minimize buffer usage
    LOG_HEXDUMP_DBG(&sensor_data, MIN(sizeof(sensor_data), 32), "sensor");
    
    // Use structured logging for analysis
    LOG_STRUCTURED(LOG_LEVEL_INF, "sensor_reading",
                  LOG_STRCT_INT("temperature", temperature),
                  LOG_STRCT_INT("humidity", humidity),
                  LOG_STRCT_STR("status", status_string));
}
```

### Advanced Tracing Techniques

**Custom Tracing Implementation:**

```c
#include <zephyr/tracing/tracing.h>

// Custom trace points for application-specific monitoring
#define TRACE_SENSOR_READ_START(sensor_id) \
    sys_trace_user_start(0x1000 | (sensor_id))

#define TRACE_SENSOR_READ_END(sensor_id, value) \
    sys_trace_user_end(0x1000 | (sensor_id), value)

void traced_sensor_operation(uint8_t sensor_id)
{
    TRACE_SENSOR_READ_START(sensor_id);
    
    uint32_t start_cycles = k_cycle_get_32();
    
    // Sensor reading operation
    int sensor_value = read_sensor_hardware(sensor_id);
    
    uint32_t operation_cycles = k_cycle_get_32() - start_cycles;
    
    TRACE_SENSOR_READ_END(sensor_id, sensor_value);
    
    LOG_DBG("Sensor %u: value=%d, cycles=%u", 
            sensor_id, sensor_value, operation_cycles);
}
```

**Integration with External Tracing Tools:**

```c
// SystemView integration
CONFIG_TRACING=y
CONFIG_SEGGER_SYSTEMVIEW=y
CONFIG_USE_SEGGER_RTT=y

// Custom trace events for SystemView
void system_trace_integration(void)
{
    // Thread context switches automatically traced
    // Custom application events
    SEGGER_SYSVIEW_RecordU32(0x100, sensor_reading_count);
    SEGGER_SYSVIEW_RecordString(0x101, "Sensor calibration complete");
}
```

---

## Production Logging Strategies

### Hierarchical Logging Architecture

**Module-Based Logging Organization:**

```c
// Sensor subsystem logging
LOG_MODULE_REGISTER(sensors, CONFIG_SENSORS_LOG_LEVEL);

// Communication subsystem logging  
LOG_MODULE_REGISTER(comm, CONFIG_COMM_LOG_LEVEL);

// Application logic logging
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

// System health monitoring
LOG_MODULE_REGISTER(health, CONFIG_HEALTH_LOG_LEVEL);

void configure_subsystem_logging(void)
{
    // Production: Errors and warnings only for most subsystems
    log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                  log_source_id_get("sensors"), LOG_LEVEL_WRN);
    
    // Debug: Full logging for development focus area
    log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                  log_source_id_get("comm"), LOG_LEVEL_DBG);
    
    // Health monitoring: Always informational
    log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                  log_source_id_get("health"), LOG_LEVEL_INF);
}
```

### Error Pattern Recognition and Analysis

**Structured Error Logging:**

```c
typedef enum {
    ERROR_SENSOR_TIMEOUT = 1000,
    ERROR_COMM_FAILURE = 2000,
    ERROR_MEMORY_ALLOCATION = 3000,
    ERROR_HARDWARE_FAULT = 4000
} system_error_codes_t;

void log_system_error(system_error_codes_t error_code, 
                     const char *context, int value)
{
    static uint32_t error_counts[5] = {0};
    uint32_t error_index = (error_code / 1000) - 1;
    
    if (error_index < ARRAY_SIZE(error_counts)) {
        error_counts[error_index]++;
    }
    
    LOG_ERR("Error %d in %s: value=%d, count=%u", 
            error_code, context, value, error_counts[error_index]);
    
    // Trigger system health analysis on repeated errors
    if (error_counts[error_index] > 5) {
        LOG_ERR("Repeated error pattern detected - triggering health check");
        system_health_monitor();
        error_counts[error_index] = 0; // Reset counter
    }
}
```

**Watchdog Integration with Logging:**

```c
#include <zephyr/drivers/watchdog.h>

void watchdog_with_logging(void)
{
    const struct device *wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    
    if (!device_is_ready(wdt)) {
        LOG_ERR("Watchdog device not ready");
        return;
    }
    
    struct wdt_timeout_cfg wdt_config = {
        .window.min = 0,
        .window.max = 5000, // 5 second timeout
        .callback = watchdog_timeout_handler,
        .flags = WDT_FLAG_RESET_SOC,
    };
    
    int wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    if (wdt_channel_id < 0) {
        LOG_ERR("Watchdog timeout installation failed: %d", wdt_channel_id);
        return;
    }
    
    wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
    LOG_INF("Watchdog configured - 5s timeout");
}

void watchdog_timeout_handler(const struct device *wdt_dev, int channel_id)
{
    LOG_ERR("WATCHDOG TIMEOUT - System reset imminent");
    thread_analyzer_print(0); // Last-chance thread analysis
    LOG_ERR("System will reset in watchdog handler");
}
```

---

## Integration with Development Workflows

### Continuous Integration Logging

**Automated Test Logging:**

```c
#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

ZTEST(logging_tests, test_logging_performance)
{
    uint32_t start_time = k_cycle_get_32();
    
    for (int i = 0; i < 1000; i++) {
        LOG_INF("Test message %d", i);
    }
    
    uint32_t total_cycles = k_cycle_get_32() - start_time;
    uint32_t avg_cycles_per_log = total_cycles / 1000;
    
    LOG_INF("Average logging overhead: %u cycles per message", avg_cycles_per_log);
    
    // Assert reasonable performance
    zassert_true(avg_cycles_per_log < 1000, 
                "Logging overhead too high: %u cycles", avg_cycles_per_log);
}

ZTEST(logging_tests, test_thread_analyzer)
{
    // Create test thread
    k_tid_t test_thread = k_thread_create(&test_thread_data, test_stack,
                                         K_THREAD_STACK_SIZEOF(test_stack),
                                         test_thread_entry, NULL, NULL, NULL,
                                         5, 0, K_NO_WAIT);
    
    k_sleep(K_MSEC(100)); // Let thread run
    
    // Analyze thread performance
    thread_analyzer_run(test_analyzer_callback, 0);
    
    k_thread_abort(test_thread);
}
#endif
```

---

This comprehensive theoretical foundation prepares you for implementing professional-grade tracing and logging systems. The Lab section will demonstrate these concepts through hands-on implementation of a complete monitoring system for your Raspberry Pi 4B platform.

[Next: Tracing and Logging Lab](./lab.md)
