# Chapter 14 - Modules Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Lab Overview

This lab teaches you to create, structure, and integrate custom modules in Zephyr. You'll build a complete sensor module that demonstrates professional module development practices, including proper API design, configuration management, and testing.

## Learning Objectives

By completing this lab, you will:

* Create a well-structured Zephyr module from scratch
* Implement proper module metadata and configuration
* Design clean public APIs with proper encapsulation
* Integrate modules with the Zephyr build system
* Test and validate module functionality
* Apply industry best practices for modular design

## Lab Setup

### Required Tools

* Zephyr SDK and development environment
* West build tool
* Git for version control
* Text editor or IDE
* Development board (Nordic nRF52840-DK or similar)

### Project Structure Overview

We'll create a temperature sensor module with the following structure:

```text
temp_sensor_module/
├── zephyr/
│   ├── module.yml
│   ├── CMakeLists.txt
│   └── Kconfig
├── include/
│   └── temp_sensor/
│       └── temp_sensor.h
├── src/
│   ├── temp_sensor.c
│   └── temp_sensor_internal.h
├── dts/
│   └── bindings/
│       └── temp-sensor.yaml
├── tests/
│   └── src/
│       └── test_temp_sensor.c
└── samples/
    └── basic/
        ├── src/
        │   └── main.c
        ├── CMakeLists.txt
        └── prj.conf
```

## Part 1: Module Foundation (45 minutes)

### Step 1: Create Module Directory Structure

Create the base directory structure:

```bash
mkdir -p temp_sensor_module/{zephyr,include/temp_sensor,src,dts/bindings,tests/src,samples/basic/src}
cd temp_sensor_module
```

### Step 2: Module Metadata

Create `zephyr/module.yml`:

```yaml
name: temp_sensor_module
description: "Professional temperature sensor module for Zephyr RTOS"
version: "1.0.0"

build:
  cmake: zephyr
  kconfig: zephyr/Kconfig

maintainer:
  - name: "Your Name"
    email: "your.email@example.com"

license: "Apache-2.0"

homepage: "https://github.com/yourusername/temp_sensor_module"

depends:
  - zephyr
```

### Step 3: Module Configuration

Create `zephyr/Kconfig`:

```kconfig
# Temperature Sensor Module Configuration

config TEMP_SENSOR_MODULE
    bool "Enable Temperature Sensor Module"
    default n
    select GPIO
    help
      Enable the custom temperature sensor module.
      This module provides high-level APIs for temperature
      sensor operations including calibration and filtering.

if TEMP_SENSOR_MODULE

config TEMP_SENSOR_INIT_PRIORITY
    int "Temperature sensor initialization priority"
    default 80
    help
      Initialization priority for the temperature sensor module.
      Should be after GPIO initialization (priority 40).

config TEMP_SENSOR_SAMPLE_RATE_MS
    int "Default sampling rate in milliseconds"
    default 1000
    range 100 60000
    help
      Default interval between temperature readings in milliseconds.

config TEMP_SENSOR_CALIBRATION
    bool "Enable temperature calibration"
    default y
    help
      Enable temperature calibration features for improved accuracy.

config TEMP_SENSOR_FILTERING
    bool "Enable temperature filtering"
    default y
    help
      Enable digital filtering to smooth temperature readings.

if TEMP_SENSOR_FILTERING

config TEMP_SENSOR_FILTER_SIZE
    int "Temperature filter window size"
    default 5
    range 3 20
    help
      Number of samples to use in the moving average filter.

endif # TEMP_SENSOR_FILTERING

endif # TEMP_SENSOR_MODULE
```

### Step 4: Build Configuration

Create `zephyr/CMakeLists.txt`:

```cmake
# Temperature Sensor Module Build Configuration

if(CONFIG_TEMP_SENSOR_MODULE)
    zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
    
    zephyr_library()
    zephyr_library_sources(
        ../src/temp_sensor.c
    )
    
    zephyr_library_compile_definitions(
        TEMP_SENSOR_VERSION="1.0.0"
    )
    
    # Add any additional compile flags if needed
    zephyr_library_compile_options(
        -Wextra
        -Wall
    )
endif()
```

## Part 2: Public API Design (30 minutes)

### Step 5: Public Header File

Create `include/temp_sensor/temp_sensor.h`:

```c
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file temp_sensor.h
 * @brief Temperature Sensor Module API
 * 
 * This module provides high-level APIs for temperature sensor operations
 * including initialization, reading, calibration, and filtering.
 */

/**
 * @brief Temperature sensor error codes
 */
typedef enum {
    TEMP_SENSOR_OK = 0,
    TEMP_SENSOR_ERROR_INIT = -1,
    TEMP_SENSOR_ERROR_READ = -2,
    TEMP_SENSOR_ERROR_CALIBRATION = -3,
    TEMP_SENSOR_ERROR_INVALID_PARAM = -4,
    TEMP_SENSOR_ERROR_NOT_READY = -5,
} temp_sensor_error_t;

/**
 * @brief Temperature sensor configuration
 */
struct temp_sensor_config {
    const struct device *gpio_dev;
    gpio_pin_t data_pin;
    gpio_pin_t power_pin;
    uint32_t sample_rate_ms;
    bool enable_calibration;
    bool enable_filtering;
    uint8_t filter_size;
};

/**
 * @brief Temperature sensor handle
 */
struct temp_sensor;

/**
 * @brief Temperature reading callback function type
 * 
 * @param temp_celsius Temperature in degrees Celsius
 * @param user_data User data passed to callback
 */
typedef void (*temp_sensor_callback_t)(float temp_celsius, void *user_data);

/**
 * @brief Initialize temperature sensor module
 * 
 * @param config Sensor configuration structure
 * @return Pointer to sensor handle or NULL on failure
 */
struct temp_sensor *temp_sensor_init(const struct temp_sensor_config *config);

/**
 * @brief Deinitialize temperature sensor
 * 
 * @param sensor Sensor handle
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_deinit(struct temp_sensor *sensor);

/**
 * @brief Read temperature synchronously
 * 
 * @param sensor Sensor handle
 * @param temp_celsius Pointer to store temperature value
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_read(struct temp_sensor *sensor, 
                                     float *temp_celsius);

/**
 * @brief Start continuous temperature monitoring
 * 
 * @param sensor Sensor handle
 * @param callback Callback function for temperature readings
 * @param user_data User data passed to callback
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_start_monitoring(struct temp_sensor *sensor,
                                                 temp_sensor_callback_t callback,
                                                 void *user_data);

/**
 * @brief Stop continuous temperature monitoring
 * 
 * @param sensor Sensor handle
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_stop_monitoring(struct temp_sensor *sensor);

/**
 * @brief Calibrate temperature sensor
 * 
 * @param sensor Sensor handle
 * @param reference_temp Known reference temperature
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_calibrate(struct temp_sensor *sensor,
                                          float reference_temp);

/**
 * @brief Get module version string
 * 
 * @return Version string
 */
const char *temp_sensor_get_version(void);

/**
 * @brief Check if sensor is ready for operation
 * 
 * @param sensor Sensor handle
 * @return true if ready, false otherwise
 */
bool temp_sensor_is_ready(struct temp_sensor *sensor);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_SENSOR_H */
```

## Part 3: Module Implementation (45 minutes)

### Step 6: Internal Header

Create `src/temp_sensor_internal.h`:

```c
#ifndef TEMP_SENSOR_INTERNAL_H
#define TEMP_SENSOR_INTERNAL_H

#include <temp_sensor/temp_sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal temperature sensor structure
 */
struct temp_sensor {
    struct temp_sensor_config config;
    struct k_timer monitoring_timer;
    struct k_work monitoring_work;
    temp_sensor_callback_t callback;
    void *user_data;
    float calibration_offset;
    float *filter_buffer;
    uint8_t filter_index;
    bool initialized;
    bool monitoring_active;
};

/**
 * @brief Convert raw ADC reading to temperature
 * 
 * @param raw_value Raw ADC value
 * @return Temperature in Celsius
 */
float temp_sensor_raw_to_celsius(uint32_t raw_value);

/**
 * @brief Apply calibration to temperature reading
 * 
 * @param sensor Sensor handle
 * @param temp_celsius Raw temperature
 * @return Calibrated temperature
 */
float temp_sensor_apply_calibration(struct temp_sensor *sensor, 
                                   float temp_celsius);

/**
 * @brief Apply filtering to temperature reading
 * 
 * @param sensor Sensor handle
 * @param temp_celsius New temperature sample
 * @return Filtered temperature
 */
float temp_sensor_apply_filter(struct temp_sensor *sensor, 
                              float temp_celsius);

/**
 * @brief Read raw temperature from hardware
 * 
 * @param sensor Sensor handle
 * @param raw_value Pointer to store raw value
 * @return TEMP_SENSOR_OK on success, error code on failure
 */
temp_sensor_error_t temp_sensor_read_raw(struct temp_sensor *sensor,
                                        uint32_t *raw_value);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_SENSOR_INTERNAL_H */
```

### Step 7: Module Implementation

Create `src/temp_sensor.c`:

```c
#include "temp_sensor_internal.h"
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(temp_sensor, CONFIG_LOG_DEFAULT_LEVEL);

/* Static memory pool for sensor instances */
#define MAX_SENSORS 4
static struct temp_sensor sensor_pool[MAX_SENSORS];
static bool sensor_pool_used[MAX_SENSORS];

/* Module version */
#ifndef TEMP_SENSOR_VERSION
#define TEMP_SENSOR_VERSION "1.0.0"
#endif

/* Forward declarations */
static void monitoring_timer_handler(struct k_timer *timer);
static void monitoring_work_handler(struct k_work *work);

struct temp_sensor *temp_sensor_init(const struct temp_sensor_config *config)
{
    if (!config || !config->gpio_dev) {
        LOG_ERR("Invalid configuration");
        return NULL;
    }

    /* Find available sensor slot */
    struct temp_sensor *sensor = NULL;
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (!sensor_pool_used[i]) {
            sensor = &sensor_pool[i];
            sensor_pool_used[i] = true;
            break;
        }
    }

    if (!sensor) {
        LOG_ERR("No available sensor slots");
        return NULL;
    }

    /* Initialize sensor structure */
    memset(sensor, 0, sizeof(*sensor));
    memcpy(&sensor->config, config, sizeof(*config));

    /* Configure GPIO pins */
    int ret = gpio_pin_configure(config->gpio_dev, config->data_pin, 
                                GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure data pin: %d", ret);
        goto error;
    }

    ret = gpio_pin_configure(config->gpio_dev, config->power_pin, 
                            GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure power pin: %d", ret);
        goto error;
    }

    /* Initialize timer and work */
    k_timer_init(&sensor->monitoring_timer, monitoring_timer_handler, NULL);
    k_work_init(&sensor->monitoring_work, monitoring_work_handler);

    /* Initialize calibration */
    sensor->calibration_offset = 0.0f;

    /* Initialize filtering */
    if (config->enable_filtering) {
        sensor->filter_buffer = k_malloc(config->filter_size * sizeof(float));
        if (!sensor->filter_buffer) {
            LOG_ERR("Failed to allocate filter buffer");
            goto error;
        }
        memset(sensor->filter_buffer, 0, 
               config->filter_size * sizeof(float));
    }

    sensor->initialized = true;
    LOG_INF("Temperature sensor initialized successfully");
    return sensor;

error:
    /* Cleanup on error */
    if (sensor) {
        if (sensor->filter_buffer) {
            k_free(sensor->filter_buffer);
        }
        sensor_pool_used[sensor - sensor_pool] = false;
    }
    return NULL;
}

temp_sensor_error_t temp_sensor_deinit(struct temp_sensor *sensor)
{
    if (!sensor || !sensor->initialized) {
        return TEMP_SENSOR_ERROR_INVALID_PARAM;
    }

    /* Stop monitoring if active */
    temp_sensor_stop_monitoring(sensor);

    /* Cleanup resources */
    if (sensor->filter_buffer) {
        k_free(sensor->filter_buffer);
        sensor->filter_buffer = NULL;
    }

    /* Power down sensor */
    gpio_pin_set(sensor->config.gpio_dev, sensor->config.power_pin, 0);

    /* Mark slot as available */
    sensor_pool_used[sensor - sensor_pool] = false;
    sensor->initialized = false;

    LOG_INF("Temperature sensor deinitialized");
    return TEMP_SENSOR_OK;
}

temp_sensor_error_t temp_sensor_read(struct temp_sensor *sensor, 
                                     float *temp_celsius)
{
    if (!sensor || !sensor->initialized || !temp_celsius) {
        return TEMP_SENSOR_ERROR_INVALID_PARAM;
    }

    uint32_t raw_value;
    temp_sensor_error_t ret = temp_sensor_read_raw(sensor, &raw_value);
    if (ret != TEMP_SENSOR_OK) {
        return ret;
    }

    /* Convert to celsius */
    float temp = temp_sensor_raw_to_celsius(raw_value);

    /* Apply calibration */
    if (sensor->config.enable_calibration) {
        temp = temp_sensor_apply_calibration(sensor, temp);
    }

    /* Apply filtering */
    if (sensor->config.enable_filtering) {
        temp = temp_sensor_apply_filter(sensor, temp);
    }

    *temp_celsius = temp;
    return TEMP_SENSOR_OK;
}

temp_sensor_error_t temp_sensor_start_monitoring(struct temp_sensor *sensor,
                                                 temp_sensor_callback_t callback,
                                                 void *user_data)
{
    if (!sensor || !sensor->initialized || !callback) {
        return TEMP_SENSOR_ERROR_INVALID_PARAM;
    }

    if (sensor->monitoring_active) {
        LOG_WRN("Monitoring already active");
        return TEMP_SENSOR_OK;
    }

    sensor->callback = callback;
    sensor->user_data = user_data;
    sensor->monitoring_active = true;

    /* Start periodic timer */
    k_timer_start(&sensor->monitoring_timer,
                  K_MSEC(sensor->config.sample_rate_ms),
                  K_MSEC(sensor->config.sample_rate_ms));

    LOG_INF("Temperature monitoring started");
    return TEMP_SENSOR_OK;
}

temp_sensor_error_t temp_sensor_stop_monitoring(struct temp_sensor *sensor)
{
    if (!sensor || !sensor->initialized) {
        return TEMP_SENSOR_ERROR_INVALID_PARAM;
    }

    if (!sensor->monitoring_active) {
        return TEMP_SENSOR_OK;
    }

    k_timer_stop(&sensor->monitoring_timer);
    k_work_cancel(&sensor->monitoring_work);
    
    sensor->monitoring_active = false;
    sensor->callback = NULL;
    sensor->user_data = NULL;

    LOG_INF("Temperature monitoring stopped");
    return TEMP_SENSOR_OK;
}

/* Timer handler - runs in interrupt context */
static void monitoring_timer_handler(struct k_timer *timer)
{
    struct temp_sensor *sensor = CONTAINER_OF(timer, struct temp_sensor, 
                                              monitoring_timer);
    /* Submit work to system workqueue */
    k_work_submit(&sensor->monitoring_work);
}

/* Work handler - runs in thread context */
static void monitoring_work_handler(struct k_work *work)
{
    struct temp_sensor *sensor = CONTAINER_OF(work, struct temp_sensor, 
                                              monitoring_work);
    
    if (!sensor->monitoring_active || !sensor->callback) {
        return;
    }

    float temp_celsius;
    temp_sensor_error_t ret = temp_sensor_read(sensor, &temp_celsius);
    if (ret == TEMP_SENSOR_OK) {
        sensor->callback(temp_celsius, sensor->user_data);
    } else {
        LOG_ERR("Failed to read temperature: %d", ret);
    }
}

/* Helper function implementations */
float temp_sensor_raw_to_celsius(uint32_t raw_value)
{
    /* Convert raw ADC value to temperature using sensor characteristics */
    /* This is a simplified linear conversion - adjust for your sensor */
    const float vref = 3.3f;
    const float resolution = 4096.0f; /* 12-bit ADC */
    const float temp_coefficient = 0.01f; /* V/°C */
    const float offset_voltage = 0.5f; /* V at 0°C */
    
    float voltage = (raw_value / resolution) * vref;
    float temp_celsius = (voltage - offset_voltage) / temp_coefficient;
    
    return temp_celsius;
}

float temp_sensor_apply_calibration(struct temp_sensor *sensor, 
                                   float temp_celsius)
{
    return temp_celsius + sensor->calibration_offset;
}

float temp_sensor_apply_filter(struct temp_sensor *sensor, 
                              float temp_celsius)
{
    if (!sensor->filter_buffer) {
        return temp_celsius;
    }

    /* Add new sample to circular buffer */
    sensor->filter_buffer[sensor->filter_index] = temp_celsius;
    sensor->filter_index = (sensor->filter_index + 1) % 
                          sensor->config.filter_size;

    /* Calculate moving average */
    float sum = 0.0f;
    for (int i = 0; i < sensor->config.filter_size; i++) {
        sum += sensor->filter_buffer[i];
    }

    return sum / sensor->config.filter_size;
}

temp_sensor_error_t temp_sensor_read_raw(struct temp_sensor *sensor,
                                        uint32_t *raw_value)
{
    /* Simplified raw reading - in real implementation, use ADC driver */
    /* This simulates a temperature reading */
    static int32_t simulated_temp = 2048; /* Mid-range value */
    
    /* Add some variation */
    simulated_temp += (k_uptime_get_32() % 20) - 10;
    
    /* Keep in valid range */
    if (simulated_temp > 4095) simulated_temp = 4095;
    if (simulated_temp < 0) simulated_temp = 0;
    
    *raw_value = simulated_temp;
    return TEMP_SENSOR_OK;
}

const char *temp_sensor_get_version(void)
{
    return TEMP_SENSOR_VERSION;
}

bool temp_sensor_is_ready(struct temp_sensor *sensor)
{
    return sensor && sensor->initialized;
}

temp_sensor_error_t temp_sensor_calibrate(struct temp_sensor *sensor,
                                          float reference_temp)
{
    if (!sensor || !sensor->initialized) {
        return TEMP_SENSOR_ERROR_INVALID_PARAM;
    }

    float current_reading;
    temp_sensor_error_t ret = temp_sensor_read(sensor, &current_reading);
    if (ret != TEMP_SENSOR_OK) {
        return ret;
    }

    sensor->calibration_offset = reference_temp - current_reading;
    LOG_INF("Calibration complete. Offset: %.2f°C", 
            sensor->calibration_offset);

    return TEMP_SENSOR_OK;
}
```

## Part 4: Testing and Validation (30 minutes)

### Step 8: Create Test Application

Create `samples/basic/src/main.c`:

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <temp_sensor/temp_sensor.h>

LOG_MODULE_REGISTER(temp_sensor_sample, LOG_LEVEL_INF);

/* Temperature reading callback */
void temp_callback(float temp_celsius, void *user_data)
{
    LOG_INF("Temperature: %.2f°C", temp_celsius);
}

int main(void)
{
    LOG_INF("Temperature Sensor Module Sample");
    LOG_INF("Version: %s", temp_sensor_get_version());

    /* Configure sensor */
    const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    
    struct temp_sensor_config config = {
        .gpio_dev = gpio_dev,
        .data_pin = 3,
        .power_pin = 4,
        .sample_rate_ms = 2000,
        .enable_calibration = true,
        .enable_filtering = true,
        .filter_size = 5,
    };

    /* Initialize sensor */
    struct temp_sensor *sensor = temp_sensor_init(&config);
    if (!sensor) {
        LOG_ERR("Failed to initialize temperature sensor");
        return -1;
    }

    LOG_INF("Sensor initialized successfully");

    /* Test single reading */
    float temp;
    if (temp_sensor_read(sensor, &temp) == TEMP_SENSOR_OK) {
        LOG_INF("Initial temperature: %.2f°C", temp);
    }

    /* Start continuous monitoring */
    temp_sensor_start_monitoring(sensor, temp_callback, NULL);
    LOG_INF("Started continuous monitoring");

    /* Run for 30 seconds then stop */
    k_msleep(30000);
    
    temp_sensor_stop_monitoring(sensor);
    LOG_INF("Stopped monitoring");

    /* Cleanup */
    temp_sensor_deinit(sensor);
    LOG_INF("Sensor deinitialized");

    return 0;
}
```

Create `samples/basic/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(temp_sensor_sample)

target_sources(app PRIVATE src/main.c)
```

Create `samples/basic/prj.conf`:

```conf
CONFIG_TEMP_SENSOR_MODULE=y
CONFIG_TEMP_SENSOR_CALIBRATION=y
CONFIG_TEMP_SENSOR_FILTERING=y
CONFIG_GPIO=y
CONFIG_PRINTK=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

## Part 5: Module Integration and Testing (20 minutes)

### Step 9: Build and Test

Build the sample application:

```bash
cd samples/basic
west build -b qemu_x86
west build -t run
```

### Step 10: Validation Checklist

Verify the following functionality:

* ✅ Module loads and initializes correctly
* ✅ Temperature readings are generated
* ✅ Filtering smooths temperature values
* ✅ Continuous monitoring works
* ✅ Clean shutdown and resource cleanup
* ✅ Configuration options are respected
* ✅ Error handling works properly

## Lab Exercises

### Exercise 1: Add Temperature Alerts

Extend the module to support temperature threshold alerts.

### Exercise 2: Implement Device Tree Bindings

Create proper device tree bindings for hardware configuration.

### Exercise 3: Add Unit Tests

Create comprehensive unit tests for all module functions.

### Exercise 4: Power Management

Add power management features to reduce power consumption.

## Expected Outcomes

Upon completion, you will have:

1. A professional-grade Zephyr module with proper structure
2. Clean API design with good encapsulation
3. Comprehensive configuration system
4. Working test application
5. Understanding of module integration patterns
6. Skills to create reusable embedded components

This lab demonstrates industry best practices for modular embedded software development using Zephyr RTOS.

[Next: Chapter 15 - Writing Kconfig Symbols](../chapter_15_writing_kconfig_symbols/README.md)
