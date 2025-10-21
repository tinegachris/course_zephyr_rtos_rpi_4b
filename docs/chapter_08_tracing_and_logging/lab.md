# Chapter 8: Tracing and Logging - Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

This lab provides hands-on experience with Zephyr's comprehensive tracing and logging capabilities. You'll build a professional monitoring system that demonstrates logging, thread analysis, performance measurement, and debugging workflows for your Raspberry Pi 4B platform.

---

## Hardware Setup

Connect components to your Raspberry Pi 4B:

```text
GPIO Connections:
- Status LED: GPIO18 (Pin 12) → 220Ω resistor → LED → GND
- Activity LED: GPIO19 (Pin 35) → 220Ω resistor → LED → GND
- Control Button: GPIO21 (Pin 40) → Button → GND (with internal pull-up)
- Temperature Sensor: I2C1 (SDA: GPIO2, SCL: GPIO3)
- Debug UART: UART0 (TX: GPIO14, RX: GPIO15) for logging output
```

---

## Lab 1: Modern Logging System Implementation

### Learning Goals

Implement Zephyr's modern logging system with multiple backends, hierarchical modules, and performance optimization for professional embedded system development.

### Project Structure

Create the project structure:

```bash
mkdir -p tracing_logging_lab/src
cd tracing_logging_lab
```

### Configuration Setup

Create `prj.conf`:

```kconfig
# Core system configuration
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_HEAP_RUNTIME_STATS=y

# GPIO and I2C support
CONFIG_GPIO=y
CONFIG_I2C=y

# Modern logging system
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_LOG_BACKEND_UART=y
CONFIG_LOG_PRINTK=y
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_BUFFER_SIZE=4096
CONFIG_LOG_PROCESS_THREAD_STACK_SIZE=2048

# Advanced logging features
CONFIG_LOG_FUNC_NAME_PREFIX_ERR=y
CONFIG_LOG_FUNC_NAME_PREFIX_WRN=y
CONFIG_LOG_TIMESTAMP_64BIT=y
CONFIG_LOG_SPEED=y

# Thread analyzer configuration
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_USE_LOG=y
CONFIG_THREAD_ANALYZER_AUTO=y
CONFIG_THREAD_ANALYZER_AUTO_INTERVAL=10
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y

# Stack monitoring
CONFIG_INIT_STACKS=y
CONFIG_THREAD_MONITOR=y

# Shell for runtime control
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
CONFIG_KERNEL_SHELL=y
CONFIG_SHELL_CMDS=y

# Timing support for performance measurement
CONFIG_TIMING_FUNCTIONS=y

# Device tree support
CONFIG_GPIO_GET_DIRECTION=y
CONFIG_GPIO_GET_CONFIG=y
```

### Device Tree Overlay

Create `boards/rpi_4b.overlay`:

```dts
/ {
    aliases {
        status-led = &status_led;
        activity-led = &activity_led;
        control-btn = &control_button;
        temp-sensor = &temp_sensor;
    };

    leds {
        compatible = "gpio-leds";
        status_led: led_0 {
            gpios = <&gpio 18 GPIO_ACTIVE_HIGH>;
            label = "Status LED";
        };
        activity_led: led_1 {
            gpios = <&gpio 19 GPIO_ACTIVE_HIGH>;
            label = "Activity LED";
        };
    };

    buttons {
        compatible = "gpio-keys";
        control_button: button_0 {
            gpios = <&gpio 21 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
            label = "Control Button";
        };
    };
};

&i2c1 {
    status = "okay";
    temp_sensor: temp@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
        label = "Temperature Sensor";
    };
};
```

### Main Application Implementation

Create `src/main.c`:

```c
/*
 * Comprehensive Tracing and Logging System
 * Demonstrates modern Zephyr logging, thread analysis, and performance monitoring
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/shell/shell.h>
#include <zephyr/timing/timing.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

/* Register logging modules for different subsystems */
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* Device tree specifications */
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(activity_led), gpios);
static const struct gpio_dt_spec control_btn = GPIO_DT_SPEC_GET(DT_ALIAS(control_btn), gpios);
static const struct i2c_dt_spec temp_sensor = I2C_DT_SPEC_GET(DT_ALIAS(temp_sensor));

/* Thread parameters */
#define SENSOR_THREAD_STACK_SIZE    2048
#define MONITOR_THREAD_STACK_SIZE   2048
#define LOGGER_THREAD_STACK_SIZE    2048

#define SENSOR_THREAD_PRIORITY      5
#define MONITOR_THREAD_PRIORITY     6
#define LOGGER_THREAD_PRIORITY      7

/* Thread stacks and control blocks */
K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(monitor_thread_stack, MONITOR_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(logger_thread_stack, LOGGER_THREAD_STACK_SIZE);

static struct k_thread sensor_thread_data;
static struct k_thread monitor_thread_data;
static struct k_thread logger_thread_data;

/* System state and statistics */
static volatile bool system_active = true;
static volatile uint32_t sensor_readings = 0;
static volatile uint32_t button_presses = 0;
static volatile float last_temperature = 0.0f;

/* Performance monitoring */
typedef struct {
    uint32_t total_operations;
    uint32_t total_cycles;
    uint32_t min_cycles;
    uint32_t max_cycles;
} performance_stats_t;

static performance_stats_t sensor_perf_stats = {0};
static performance_stats_t logging_perf_stats = {0};

/* GPIO callback structure */
static struct gpio_callback button_cb_data;

/* Function prototypes */
static int initialize_hardware(void);
static void sensor_thread_entry(void *p1, void *p2, void *p3);
static void monitor_thread_entry(void *p1, void *p2, void *p3);
static void logger_thread_entry(void *p1, void *p2, void *p3);
static void button_pressed_callback(const struct device *dev, 
                                   struct gpio_callback *cb, uint32_t pins);
static void measure_logging_performance(void);
static void custom_thread_analyzer_callback(struct thread_analyzer_info *info);

/* Hardware initialization with comprehensive logging */
static int initialize_hardware(void)
{
    LOG_INF("Starting hardware initialization");
    int ret;

    /* Initialize status LED */
    if (!gpio_is_ready_dt(&status_led)) {
        LOG_ERR("Status LED GPIO device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure status LED: %d", ret);
        return ret;
    }
    LOG_DBG("Status LED configured on pin %d", status_led.pin);

    /* Initialize activity LED */
    if (!gpio_is_ready_dt(&activity_led)) {
        LOG_ERR("Activity LED GPIO device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure activity LED: %d", ret);
        return ret;
    }
    LOG_DBG("Activity LED configured on pin %d", activity_led.pin);

    /* Initialize control button */
    if (!gpio_is_ready_dt(&control_btn)) {
        LOG_ERR("Control button GPIO device not ready");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&control_btn, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure control button: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&control_btn, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt: %d", ret);
        return ret;
    }

    gpio_init_callback(&button_cb_data, button_pressed_callback, BIT(control_btn.pin));
    LOG_DBG("Control button configured with interrupt on pin %d", control_btn.pin);

    /* Initialize I2C temperature sensor */
    if (!i2c_is_ready_dt(&temp_sensor)) {
        LOG_WRN("Temperature sensor not available - using simulated values");
    } else {
        LOG_INF("Temperature sensor ready on I2C bus");
    }

    LOG_INF("Hardware initialization completed successfully");
    return 0;
}

/* Button press callback with logging */
static void button_pressed_callback(const struct device *dev, 
                                   struct gpio_callback *cb, uint32_t pins)
{
    button_presses++;
    LOG_INF("Button pressed (count: %u)", button_presses);
    
    /* Flash activity LED */
    gpio_pin_set_dt(&activity_led, 1);
    k_msleep(100);
    gpio_pin_set_dt(&activity_led, 0);
}

/* Sensor monitoring thread with performance tracking */
static void sensor_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Sensor thread started (TID: %p)", k_current_get());
    
    timing_t start_time, end_time;
    uint64_t cycles;
    
    while (system_active) {
        /* Start performance measurement */
        timing_start();
        start_time = timing_counter_get();
        
        /* Simulate sensor reading */
        if (i2c_is_ready_dt(&temp_sensor)) {
            uint8_t temp_data[2];
            int ret = i2c_read_dt(&temp_sensor, temp_data, sizeof(temp_data));
            if (ret == 0) {
                int16_t temp_raw = (temp_data[0] << 4) | (temp_data[1] >> 4);
                last_temperature = temp_raw * 0.0625f;
                LOG_DBG("Temperature sensor read: %.2f°C", last_temperature);
            } else {
                LOG_WRN("I2C read failed: %d", ret);
                last_temperature = 20.0f + (sys_rand32_get() % 200) / 10.0f;
            }
        } else {
            /* Simulated temperature reading */
            last_temperature = 20.0f + (sys_rand32_get() % 200) / 10.0f;
        }
        
        sensor_readings++;
        
        /* End performance measurement */
        end_time = timing_counter_get();
        cycles = timing_cycles_get(&start_time, &end_time);
        
        /* Update performance statistics */
        sensor_perf_stats.total_operations++;
        sensor_perf_stats.total_cycles += cycles;
        
        if (sensor_perf_stats.min_cycles == 0 || cycles < sensor_perf_stats.min_cycles) {
            sensor_perf_stats.min_cycles = cycles;
        }
        if (cycles > sensor_perf_stats.max_cycles) {
            sensor_perf_stats.max_cycles = cycles;
        }
        
        /* Log with different levels based on conditions */
        if (last_temperature > 30.0f) {
            LOG_WRN("High temperature detected: %.2f°C", last_temperature);
        } else if (last_temperature < 15.0f) {
            LOG_WRN("Low temperature detected: %.2f°C", last_temperature);
        } else {
            LOG_INF("Temperature reading %u: %.2f°C (cycles: %llu)", 
                    sensor_readings, last_temperature, cycles);
        }
        
        /* Toggle status LED */
        static bool led_state = false;
        led_state = !led_state;
        gpio_pin_set_dt(&status_led, led_state);
        
        k_sleep(K_SECONDS(2));
    }
    
    LOG_INF("Sensor thread exiting");
}

/* System monitoring thread */
static void monitor_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Monitor thread started (TID: %p)", k_current_get());
    
    uint32_t health_check_count = 0;
    
    while (system_active) {
        k_sleep(K_SECONDS(15));
        
        health_check_count++;
        LOG_INF("=== System Health Check #%u ===", health_check_count);
        
        /* Thread analysis */
        LOG_INF("Running thread analyzer...");
        thread_analyzer_run(custom_thread_analyzer_callback, 0);
        
        /* Performance statistics */
        if (sensor_perf_stats.total_operations > 0) {
            uint32_t avg_cycles = sensor_perf_stats.total_cycles / 
                                 sensor_perf_stats.total_operations;
            LOG_INF("Sensor performance: avg=%u, min=%u, max=%u cycles",
                    avg_cycles, sensor_perf_stats.min_cycles, 
                    sensor_perf_stats.max_cycles);
        }
        
        /* System statistics */
        LOG_INF("System stats: temp=%.1f°C, readings=%u, button_presses=%u",
                last_temperature, sensor_readings, button_presses);
        
        /* Memory usage */
        LOG_INF("Heap free: %zu bytes", k_heap_free_get(&k_malloc_heap));
    }
    
    LOG_INF("Monitor thread exiting");
}

/* Custom thread analyzer callback */
static void custom_thread_analyzer_callback(struct thread_analyzer_info *info)
{
    size_t stack_used_percent = (info->stack_used * 100) / info->stack_size;
    
    if (stack_used_percent > 75) {
        LOG_WRN("High stack usage - Thread: %s, %zu%% (%zu/%zu bytes)",
                info->name, stack_used_percent, info->stack_used, info->stack_size);
    } else {
        LOG_INF("Thread: %-16s Stack: %3zu%% (%zu/%zu) CPU: %3u%%",
                info->name, stack_used_percent, info->stack_used, 
                info->stack_size, info->utilization);
    }
}

/* Logger performance testing thread */
static void logger_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Logger thread started (TID: %p)", k_current_get());
    
    k_sleep(K_SECONDS(5)); // Wait for system stabilization
    
    while (system_active) {
        measure_logging_performance();
        k_sleep(K_SECONDS(30));
    }
    
    LOG_INF("Logger thread exiting");
}

/* Measure logging performance */
static void measure_logging_performance(void)
{
    LOG_INF("Starting logging performance measurement");
    
    timing_t start_time, end_time;
    uint64_t total_cycles = 0;
    const int test_iterations = 100;
    
    timing_start();
    
    for (int i = 0; i < test_iterations; i++) {
        start_time = timing_counter_get();
        LOG_DBG("Performance test message %d with parameter %d", i, i * 2);
        end_time = timing_counter_get();
        
        total_cycles += timing_cycles_get(&start_time, &end_time);
    }
    
    uint32_t avg_cycles = total_cycles / test_iterations;
    uint32_t avg_us = timing_cycles_to_ns(avg_cycles) / 1000;
    
    LOG_INF("Logging performance: %u cycles (%u μs) average per message", 
            avg_cycles, avg_us);
    
    /* Update global logging performance statistics */
    logging_perf_stats.total_operations++;
    logging_perf_stats.total_cycles += avg_cycles;
    
    if (logging_perf_stats.min_cycles == 0 || avg_cycles < logging_perf_stats.min_cycles) {
        logging_perf_stats.min_cycles = avg_cycles;
    }
    if (avg_cycles > logging_perf_stats.max_cycles) {
        logging_perf_stats.max_cycles = avg_cycles;
    }
}

/* Shell command implementations */
static int cmd_system_stats(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== System Statistics ===");
    shell_print(sh, "Temperature: %.2f°C", last_temperature);
    shell_print(sh, "Sensor readings: %u", sensor_readings);
    shell_print(sh, "Button presses: %u", button_presses);
    shell_print(sh, "Heap free: %zu bytes", k_heap_free_get(&k_malloc_heap));
    
    return 0;
}

static int cmd_thread_analysis(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== Thread Analysis ===");
    thread_analyzer_print(0);
    return 0;
}

static int cmd_performance_stats(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "=== Performance Statistics ===");
    
    if (sensor_perf_stats.total_operations > 0) {
        uint32_t avg = sensor_perf_stats.total_cycles / sensor_perf_stats.total_operations;
        shell_print(sh, "Sensor operations: %u", sensor_perf_stats.total_operations);
        shell_print(sh, "  Average: %u cycles", avg);
        shell_print(sh, "  Min: %u cycles", sensor_perf_stats.min_cycles);
        shell_print(sh, "  Max: %u cycles", sensor_perf_stats.max_cycles);
    }
    
    if (logging_perf_stats.total_operations > 0) {
        uint32_t avg = logging_perf_stats.total_cycles / logging_perf_stats.total_operations;
        shell_print(sh, "Logging operations: %u", logging_perf_stats.total_operations);
        shell_print(sh, "  Average: %u cycles", avg);
        shell_print(sh, "  Min: %u cycles", logging_perf_stats.min_cycles);
        shell_print(sh, "  Max: %u cycles", logging_perf_stats.max_cycles);
    }
    
    return 0;
}

static int cmd_log_level(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "Usage: loglevel <0-4>");
        shell_print(sh, "0=OFF, 1=ERR, 2=WRN, 3=INF, 4=DBG");
        return -EINVAL;
    }
    
    int level = atoi(argv[1]);
    if (level < 0 || level > 4) {
        shell_print(sh, "Invalid log level: %d", level);
        return -EINVAL;
    }
    
    /* Update log level for main module */
    log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID, 
                  log_source_id_get("main"), level);
    
    shell_print(sh, "Log level set to %d", level);
    LOG_INF("Log level changed to %d via shell command", level);
    
    return 0;
}

/* Register shell commands */
SHELL_CMD_REGISTER(stats, NULL, "Show system statistics", cmd_system_stats);
SHELL_CMD_REGISTER(threads, NULL, "Show thread analysis", cmd_thread_analysis);
SHELL_CMD_REGISTER(perf, NULL, "Show performance statistics", cmd_performance_stats);
SHELL_CMD_REGISTER(loglevel, NULL, "Set logging level", cmd_log_level);

/* Main application entry point */
int main(void)
{
    LOG_INF("=== Comprehensive Tracing and Logging System ===");
    LOG_INF("Build time: %s %s", __DATE__, __TIME__);
    LOG_INF("Board: %s", CONFIG_BOARD);
    
    /* Initialize timing subsystem */
    timing_init();
    LOG_INF("Timing subsystem initialized");
    
    /* Initialize hardware */
    int ret = initialize_hardware();
    if (ret < 0) {
        LOG_ERR("Hardware initialization failed: %d", ret);
        return ret;
    }
    
    /* Configure logging system */
    log_init();
    LOG_INF("Logging system initialized");
    
    /* Create and start threads */
    k_tid_t sensor_tid = k_thread_create(&sensor_thread_data, sensor_thread_stack,
                                        K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                                        sensor_thread_entry, NULL, NULL, NULL,
                                        SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(sensor_tid, "sensor");
    
    k_tid_t monitor_tid = k_thread_create(&monitor_thread_data, monitor_thread_stack,
                                         K_THREAD_STACK_SIZEOF(monitor_thread_stack),
                                         monitor_thread_entry, NULL, NULL, NULL,
                                         MONITOR_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(monitor_tid, "monitor");
    
    k_tid_t logger_tid = k_thread_create(&logger_thread_data, logger_thread_stack,
                                        K_THREAD_STACK_SIZEOF(logger_thread_stack),
                                        logger_thread_entry, NULL, NULL, NULL,
                                        LOGGER_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(logger_tid, "logger");
    
    LOG_INF("All threads created and started");
    LOG_INF("System fully operational - monitoring active");
    LOG_INF("Use shell commands: stats, threads, perf, loglevel");
    
    /* Main thread becomes a supervisor */
    uint32_t supervisor_cycles = 0;
    
    while (system_active) {
        supervisor_cycles++;
        
        /* Supervisor health check every 60 seconds */
        if ((supervisor_cycles % 60) == 0) {
            LOG_INF("Supervisor check #%u - system running normally", 
                    supervisor_cycles / 60);
        }
        
        k_sleep(K_SECONDS(1));
    }
    
    LOG_INF("System shutdown initiated");
    return 0;
}
```

### Production Configuration

Create `overlay-production.conf`:

```kconfig
# Production logging configuration - reduced verbosity
CONFIG_LOG_DEFAULT_LEVEL=2
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_BUFFER_SIZE=1024

# Disable debug features for production
CONFIG_THREAD_ANALYZER_AUTO=n
CONFIG_LOG_FUNC_NAME_PREFIX_ERR=n
CONFIG_LOG_FUNC_NAME_PREFIX_WRN=n

# Optimize for performance
CONFIG_LOG_SPEED=y
CONFIG_LOG_PRINTK=n
```

### Testing and Expected Behavior

1. **Build and flash:**

   ```bash
   west build -b rpi_4b -p auto
   west flash
   west attach
   ```

2. **Expected output:**
   * Comprehensive initialization logging with timestamps
   * Temperature readings every 2 seconds with performance metrics
   * Thread analysis every 15 seconds showing stack and CPU usage
   * Button press events logged with activity LED feedback
   * Logging performance measurements every 30 seconds

3. **Shell interaction:**
   * `stats` - Display current system statistics
   * `threads` - Show detailed thread analysis
   * `perf` - Display performance statistics
   * `loglevel <0-4>` - Adjust logging verbosity dynamically

4. **Performance verification:**
   * Logging overhead typically <100 cycles per message
   * Thread stack usage monitoring with warnings >75%
   * CPU utilization tracking for all threads
   * Memory usage monitoring and reporting

---

## Lab 2: Advanced Tracing and Production Monitoring

### Advanced Monitoring Goals

Implement advanced tracing techniques, error pattern recognition, and production-ready monitoring systems with minimal performance impact.

### Advanced Logging Module

Create `src/advanced_logging.c`:

```c
/*
 * Advanced Logging and Tracing Module
 * Demonstrates structured logging, error pattern recognition, and performance optimization
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/timing/timing.h>

/* Register advanced logging module */
LOG_MODULE_REGISTER(advanced, LOG_LEVEL_DBG);

/* Error classification system */
typedef enum {
    ERROR_NONE = 0,
    ERROR_SENSOR_TIMEOUT = 1000,
    ERROR_COMM_FAILURE = 2000,
    ERROR_MEMORY_ALLOCATION = 3000,
    ERROR_HARDWARE_FAULT = 4000,
    ERROR_TIMING_VIOLATION = 5000
} system_error_codes_t;

/* Error tracking structure */
typedef struct {
    system_error_codes_t code;
    uint32_t count;
    uint32_t last_occurrence;
    bool pattern_detected;
} error_tracker_t;

static error_tracker_t error_trackers[6] = {0};

/* Structured logging implementation */
void log_structured_event(const char *event_type, const char *component,
                         int severity, const char *message, int value)
{
    /* Structured logging format for analysis tools */
    LOG_INF("EVENT|%s|%s|%d|%s|%d|%u", 
            event_type, component, severity, message, value, k_uptime_get_32());
}

/* Error pattern recognition */
void log_system_error(system_error_codes_t error_code, const char *context, int value)
{
    uint32_t current_time = k_uptime_get_32();
    uint32_t error_index = (error_code / 1000);
    
    if (error_index < ARRAY_SIZE(error_trackers)) {
        error_tracker_t *tracker = &error_trackers[error_index];
        tracker->code = error_code;
        tracker->count++;
        
        /* Pattern detection - 3 errors within 30 seconds */
        if ((current_time - tracker->last_occurrence) < 30000) {
            if (tracker->count >= 3 && !tracker->pattern_detected) {
                tracker->pattern_detected = true;
                LOG_ERR("ERROR PATTERN DETECTED: Code %d, Count %u in %u ms",
                        error_code, tracker->count, 
                        current_time - tracker->last_occurrence);
                
                /* Trigger system health analysis */
                log_structured_event("ERROR_PATTERN", context, 3, 
                                    "Pattern detected", error_code);
            }
        } else {
            /* Reset pattern detection after timeout */
            tracker->pattern_detected = false;
            tracker->count = 1;
        }
        
        tracker->last_occurrence = current_time;
    }
    
    LOG_ERR("System error %d in %s: value=%d, count=%u", 
            error_code, context, value, 
            error_trackers[error_index].count);
}

/* Performance-optimized logging for high-frequency events */
void log_high_frequency_event(const char *event, uint32_t value)
{
    static uint32_t event_counter = 0;
    static uint32_t last_log_time = 0;
    uint32_t current_time = k_uptime_get_32();
    
    event_counter++;
    
    /* Log only every 100 events or every 5 seconds */
    if ((event_counter % 100) == 0 || (current_time - last_log_time) > 5000) {
        LOG_INF("High frequency event '%s': count=%u, last_value=%u, rate=%u/sec",
                event, event_counter, value,
                event_counter * 1000 / (current_time + 1));
        last_log_time = current_time;
    }
}

/* Memory-efficient hex dump for debugging */
void log_efficient_hexdump(const uint8_t *data, size_t length, const char *label)
{
    /* Limit hex dump size to prevent log buffer overflow */
    size_t dump_size = MIN(length, 32);
    
    if (LOG_LEVEL_DBG <= CONFIG_LOG_DEFAULT_LEVEL) {
        LOG_HEXDUMP_DBG(data, dump_size, label);
        
        if (length > dump_size) {
            LOG_DBG("Hex dump truncated: showing %zu of %zu bytes", dump_size, length);
        }
    }
}

/* Critical section logging with timing measurement */
void log_critical_section_entry(const char *section_name)
{
    static timing_t section_start_time;
    section_start_time = timing_counter_get();
    
    LOG_DBG("Entering critical section: %s", section_name);
    
    /* Store start time for exit measurement */
    k_thread_custom_data_set((void *)section_start_time);
    k_thread_custom_data_set((void *)section_start_time);
}

void log_critical_section_exit(const char *section_name)
{
    timing_t start_time = (timing_t)k_thread_custom_data_get();
    timing_t end_time = timing_counter_get();
    uint64_t cycles = timing_cycles_get(&start_time, &end_time);
    uint32_t us = timing_cycles_to_ns(cycles) / 1000;
    
    LOG_DBG("Exiting critical section: %s (duration: %u μs)", section_name, us);
    
    /* Warn if critical section took too long */
    if (us > 1000) {
        LOG_WRN("Long critical section: %s took %u μs", section_name, us);
    }
}

/* Adaptive logging based on system load */
void adaptive_logging_control(void)
{
    static uint32_t last_check = 0;
    uint32_t current_time = k_uptime_get_32();
    
    /* Check system load every 10 seconds */
    if ((current_time - last_check) > 10000) {
        last_check = current_time;
        
        /* Simple load estimation based on available heap */
        size_t free_heap = k_heap_free_get(&k_malloc_heap);
        
        if (free_heap < 1024) {
            /* High memory pressure - reduce logging */
            log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                          log_source_id_get("advanced"), LOG_LEVEL_WRN);
            LOG_WRN("Memory pressure detected - reducing log verbosity");
        } else if (free_heap > 4096) {
            /* Sufficient memory - enable full logging */
            log_filter_set(NULL, Z_LOG_LOCAL_DOMAIN_ID,
                          log_source_id_get("advanced"), LOG_LEVEL_DBG);
        }
    }
}

/* Initialize advanced logging system */
void advanced_logging_init(void)
{
    LOG_INF("Advanced logging system initialized");
    
    /* Register custom log processing if needed */
    log_structured_event("SYSTEM", "advanced_logging", 1, "Initialized", 0);
    
    /* Initialize error tracking */
    memset(error_trackers, 0, sizeof(error_trackers));
    
    LOG_INF("Error pattern recognition active");
}
```

### Production Monitoring Configuration

Create `overlay-field-deployment.conf`:

```kconfig
# Field deployment configuration - minimal logging overhead
CONFIG_LOG_DEFAULT_LEVEL=1
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_BUFFER_SIZE=512

# Essential monitoring only
CONFIG_THREAD_ANALYZER=n
CONFIG_LOG_FUNC_NAME_PREFIX_ERR=y
CONFIG_LOG_FUNC_NAME_PREFIX_WRN=n

# Optimize for production
CONFIG_LOG_SPEED=y
CONFIG_LOG_PRINTK=n
CONFIG_SHELL=n

# Keep essential debugging capabilities
CONFIG_THREAD_MONITOR=y
CONFIG_INIT_STACKS=y
```

### Testing Advanced Features

1. **Build with advanced features:**

   ```bash
   west build -b rpi_4b -- -DOVERLAY_CONFIG=overlay-production.conf
   west flash
   west attach
   ```

2. **Verify advanced logging:**
   * Structured event logging for analysis tools
   * Error pattern recognition with automatic alerts
   * Adaptive logging based on system resources
   * Performance-optimized high-frequency event logging

3. **Production deployment testing:**
   * Minimal logging overhead verification
   * Critical error capture without performance impact
   * Field diagnostic information collection

---

## Lab Summary

Through these comprehensive labs, you've mastered:

**Modern Logging System:**

* LOG_MODULE_REGISTER for hierarchical logging organization
* Multiple backend configuration (UART, console) for different environments
* Dynamic log level control for development and production
* Performance measurement and optimization of logging operations

**Thread Analysis and Monitoring:**

* Thread Analyzer configuration and automated monitoring
* Stack usage analysis with overflow prevention
* CPU utilization tracking and performance optimization
* Custom analysis callbacks for application-specific monitoring

**Professional Debugging Workflows:**

* Shell integration for runtime system inspection
* Structured logging for automated analysis tools
* Error pattern recognition and alerting systems
* Production vs. development configuration management

**Advanced System Monitoring:**

* Performance impact measurement and minimization
* Memory-efficient logging for resource-constrained systems
* Critical section timing analysis
* Adaptive logging based on system load

These capabilities provide the foundation for developing, deploying, and maintaining professional embedded systems with comprehensive observability and diagnostic capabilities required for commercial applications.

[Next Chapter: Chapter 9 - Memory Management](../chapter_09_memory_management/README.md)
