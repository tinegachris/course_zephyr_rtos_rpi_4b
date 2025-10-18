# Chapter 16 - Device Driver Architecture Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## Lab Overview

This lab teaches you to build a complete device driver for Zephyr RTOS, demonstrating professional driver development practices including device tree integration, power management, interrupt handling, and API design.

## Learning Objectives

By completing this lab, you will:

* Implement a complete Zephyr device driver from scratch
* Create proper device tree bindings and integration
* Design robust driver APIs with error handling
* Implement interrupt-driven functionality
* Integrate power management features
* Apply professional driver development best practices

## Lab Setup

### Required Hardware

* Zephyr-supported development board (Nordic nRF52840-DK recommended)
* LED connected to GPIO pin
* Push button connected to GPIO pin
* Logic analyzer or oscilloscope (optional)

### Project Structure

We'll create a custom sensor driver with complete functionality:

```text
sensor_driver_lab/
├── CMakeLists.txt
├── prj.conf
├── dts/
│   └── bindings/
│       └── custom,sensor.yaml
├── boards/
│   └── nrf52840dk_nrf52840.overlay
├── drivers/
│   └── sensor/
│       ├── custom_sensor.c
│       └── custom_sensor.h
├── include/
│   └── zephyr/
│       └── drivers/
│           └── custom_sensor.h
└── src/
    └── main.c
```

## Part 1: Device Tree Binding and Configuration (30 minutes)

### Step 1: Create Device Tree Binding

Create `dts/bindings/custom,sensor.yaml`:

```yaml
# Custom sensor device tree binding
description: Custom sensor driver for demonstration

compatible: "custom,sensor"

properties:
  reg:
    type: array
    description: Register space (base address and size)
    required: true

  interrupts:
    type: array
    description: Interrupt configuration
    required: false

  data-pin:
    type: int
    description: GPIO pin for data signal
    required: true

  enable-pin:
    type: int
    description: GPIO pin for sensor enable
    required: false

  sampling-frequency:
    type: int
    description: Default sampling frequency in Hz
    default: 100

  resolution:
    type: int
    description: ADC resolution in bits
    default: 12
    enum:
      - 8
      - 10
      - 12
      - 14
      - 16

  custom,calibration-offset:
    type: int
    description: Calibration offset value
    default: 0

  custom,enable-filtering:
    type: boolean
    description: Enable digital filtering
```

### Step 2: Create Board Overlay

Create `boards/nrf52840dk_nrf52840.overlay`:

```dts
/ {
    custom_sensors {
        compatible = "simple-bus";
        
        sensor0: sensor_0 {
            compatible = "custom,sensor";
            reg = <0x40000000 0x1000>;
            data-pin = <3>;
            enable-pin = <4>;
            sampling-frequency = <500>;
            resolution = <12>;
            custom,calibration-offset = <100>;
            custom,enable-filtering;
            status = "okay";
        };
    };
    
    aliases {
        custom-sensor0 = &sensor0;
    };
};
```

### Step 3: Project Configuration

Create `prj.conf`:

```conf
# Custom sensor driver configuration
CONFIG_CUSTOM_SENSOR=y
CONFIG_CUSTOM_SENSOR_LOG_LEVEL=3
CONFIG_CUSTOM_SENSOR_INIT_PRIORITY=80

# Required subsystems
CONFIG_GPIO=y
CONFIG_ADC=y
CONFIG_SENSOR=y

# Interrupt support


# Power management
CONFIG_PM_DEVICE=y
CONFIG_PM_DEVICE_RUNTIME=y

# Logging and debugging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_PRINTK=y
CONFIG_ASSERT=y

# Threading
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y

# Memory management
CONFIG_HEAP_MEM_POOL_SIZE=16384
```

## Part 2: Driver Header and API Design (25 minutes)

### Step 4: Public API Header

Create `include/zephyr/drivers/custom_sensor.h`:

```c
#ifndef ZEPHYR_INCLUDE_DRIVERS_CUSTOM_SENSOR_H_
#define ZEPHYR_INCLUDE_DRIVERS_CUSTOM_SENSOR_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file custom_sensor.h
 * @brief Custom Sensor Driver API
 * 
 * This driver provides a template for creating custom sensor drivers
 * with device tree integration, power management, and interrupt support.
 */

/**
 * @brief Custom sensor channels
 */
enum custom_sensor_channel {
    CUSTOM_SENSOR_CHAN_DATA = SENSOR_CHAN_PRIV_START,
    CUSTOM_SENSOR_CHAN_TEMP,
    CUSTOM_SENSOR_CHAN_STATUS,
};

/**
 * @brief Custom sensor attributes
 */
enum custom_sensor_attribute {
    CUSTOM_SENSOR_ATTR_SAMPLING_FREQ = SENSOR_ATTR_PRIV_START,
    CUSTOM_SENSOR_ATTR_RESOLUTION,
    CUSTOM_SENSOR_ATTR_CALIBRATION_OFFSET,
    CUSTOM_SENSOR_ATTR_FILTER_ENABLE,
};

/**
 * @brief Custom sensor events
 */
enum custom_sensor_event {
    CUSTOM_SENSOR_EVENT_DATA_READY,
    CUSTOM_SENSOR_EVENT_THRESHOLD_EXCEEDED,
    CUSTOM_SENSOR_EVENT_ERROR,
};

/**
 * @brief Custom sensor trigger types
 */
enum custom_sensor_trigger_type {
    CUSTOM_SENSOR_TRIG_DATA_READY = SENSOR_TRIG_PRIV_START,
    CUSTOM_SENSOR_TRIG_THRESHOLD,
};

/**
 * @brief Custom sensor configuration structure
 */
struct custom_sensor_config {
    /* Device tree configuration */
    uintptr_t base_addr;
    uint32_t data_pin;
    uint32_t enable_pin;
    uint32_t sampling_freq;
    uint8_t resolution;
    int16_t calibration_offset;
    bool enable_filtering;
    
    /* Interrupt configuration */
    void (*irq_config_func)(void);
    uint32_t irq_num;
    uint32_t irq_priority;
};

/**
 * @brief Custom sensor runtime data
 */
struct custom_sensor_data {
    /* Device state */
    bool initialized;
    bool enabled;
    uint32_t sample_count;
    uint32_t error_count;
    
    /* Current readings */
    int32_t last_sample;
    int32_t temperature;
    uint32_t status_flags;
    
    /* Synchronization */
    struct k_mutex lock;
    struct k_sem data_ready;
    struct k_work process_work;
    
    /* Trigger support */
    const struct device *dev;
    sensor_trigger_handler_t trigger_handler;
    const struct sensor_trigger *trigger;
    
    /* Power management */
#ifdef CONFIG_PM_DEVICE
    bool pm_suspended;
#endif
    
    /* Filtering */
    int32_t filter_buffer[8];
    uint8_t filter_index;
};

/**
 * @brief Get device configuration
 * 
 * @param dev Device instance
 * @return Configuration structure pointer
 */
static inline const struct custom_sensor_config *
custom_sensor_get_config(const struct device *dev)
{
    return dev->config;
}

/**
 * @brief Get device runtime data
 * 
 * @param dev Device instance
 * @return Runtime data structure pointer
 */
static inline struct custom_sensor_data *
custom_sensor_get_data(const struct device *dev)
{
    return dev->data;
}

/**
 * @brief Enable custom sensor
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int custom_sensor_enable(const struct device *dev);

/**
 * @brief Disable custom sensor
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int custom_sensor_disable(const struct device *dev);

/**
 * @brief Check if sensor is ready
 * 
 * @param dev Device instance
 * @return true if ready, false otherwise
 */
bool custom_sensor_is_ready(const struct device *dev);

/**
 * @brief Get sensor statistics
 * 
 * @param dev Device instance
 * @param sample_count Pointer to store sample count
 * @param error_count Pointer to store error count
 * @return 0 on success, negative errno on failure
 */
int custom_sensor_get_stats(const struct device *dev,
                           uint32_t *sample_count,
                           uint32_t *error_count);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_CUSTOM_SENSOR_H_ */
```

## Part 3: Driver Implementation (45 minutes)

### Step 5: Driver Implementation

Create `drivers/sensor/custom_sensor.c`:

```c
#define DT_DRV_COMPAT custom_sensor

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/custom_sensor.h>

LOG_MODULE_REGISTER(custom_sensor, CONFIG_CUSTOM_SENSOR_LOG_LEVEL);

/* Forward declarations */
static int custom_sensor_sample_fetch(const struct device *dev,
                                     enum sensor_channel chan);
static int custom_sensor_channel_get(const struct device *dev,
                                    enum sensor_channel chan,
                                    struct sensor_value *val);
static int custom_sensor_attr_set(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 const struct sensor_value *val);
static int custom_sensor_attr_get(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 struct sensor_value *val);
static int custom_sensor_trigger_set(const struct device *dev,
                                    const struct sensor_trigger *trig,
                                    sensor_trigger_handler_t handler);

/* Sensor API */
static const struct sensor_driver_api custom_sensor_api = {
    .sample_fetch = custom_sensor_sample_fetch,
    .channel_get = custom_sensor_channel_get,
    .attr_set = custom_sensor_attr_set,
    .attr_get = custom_sensor_attr_get,
    .trigger_set = custom_sensor_trigger_set,
};

/* Helper functions */
static int custom_sensor_hardware_init(const struct device *dev)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    LOG_DBG("Initializing hardware at base 0x%08x", config->base_addr);
    
    /* Simulate hardware initialization */
    /* In real implementation, configure registers, clocks, etc. */
    
    /* Configure GPIO pins */
    const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO device not ready");
        return -ENODEV;
    }
    
    /* Configure enable pin as output */
    if (config->enable_pin != 0) {
        int ret = gpio_pin_configure(gpio_dev, config->enable_pin,
                                   GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure enable pin: %d", ret);
            return ret;
        }
    }
    
    /* Configure data pin as input */
    int ret = gpio_pin_configure(gpio_dev, config->data_pin, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure data pin: %d", ret);
        return ret;
    }
    
    /* Initialize filter buffer */
    memset(data->filter_buffer, 0, sizeof(data->filter_buffer));
    data->filter_index = 0;
    
    LOG_INF("Hardware initialized successfully");
    return 0;
}

static int32_t custom_sensor_read_raw(const struct device *dev)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    
    /* Simulate sensor reading */
    /* In real implementation, perform ADC conversion or register read */
    static uint32_t counter = 0;
    int32_t raw_value;
    
    /* Generate simulated sensor data */
    raw_value = 2048 + (counter % 100) - 50; /* Base + variation */
    counter++;
    
    /* Apply calibration offset */
    raw_value += config->calibration_offset;
    
    LOG_DBG("Raw sensor reading: %d", raw_value);
    return raw_value;
}

static int32_t custom_sensor_apply_filter(struct custom_sensor_data *data,
                                         int32_t new_sample)
{
    const struct custom_sensor_config *config = 
        custom_sensor_get_config(data->dev);
    
    if (!config->enable_filtering) {
        return new_sample;
    }
    
    /* Simple moving average filter */
    data->filter_buffer[data->filter_index] = new_sample;
    data->filter_index = (data->filter_index + 1) % ARRAY_SIZE(data->filter_buffer);
    
    int64_t sum = 0;
    for (int i = 0; i < ARRAY_SIZE(data->filter_buffer); i++) {
        sum += data->filter_buffer[i];
    }
    
    return (int32_t)(sum / ARRAY_SIZE(data->filter_buffer));
}

static void custom_sensor_work_handler(struct k_work *work)
{
    struct custom_sensor_data *data = 
        CONTAINER_OF(work, struct custom_sensor_data, process_work);
    const struct device *dev = data->dev;
    
    LOG_DBG("Processing sensor data");
    
    /* Read sensor data */
    int32_t raw_sample = custom_sensor_read_raw(dev);
    
    /* Apply filtering if enabled */
    int32_t filtered_sample = custom_sensor_apply_filter(data, raw_sample);
    
    k_mutex_lock(&data->lock, K_FOREVER);
    data->last_sample = filtered_sample;
    data->sample_count++;
    k_mutex_unlock(&data->lock);
    
    /* Signal data ready */
    k_sem_give(&data->data_ready);
    
    /* Call trigger handler if registered */
    if (data->trigger_handler && data->trigger) {
        data->trigger_handler(dev, data->trigger);
    }
    
    LOG_DBG("Sample processed: %d (raw: %d)", filtered_sample, raw_sample);
}

static void custom_sensor_isr(const struct device *dev)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    LOG_DBG("Sensor interrupt");
    
    /* Submit work to system workqueue */
    k_work_submit(&data->process_work);
}

/* Sensor API implementations */
static int custom_sensor_sample_fetch(const struct device *dev,
                                     enum sensor_channel chan)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    if (!data->initialized || !data->enabled) {
        return -ENODEV;
    }
    
    if (chan != SENSOR_CHAN_ALL && 
        chan != CUSTOM_SENSOR_CHAN_DATA) {
        return -ENOTSUP;
    }
    
    LOG_DBG("Fetching sensor sample");
    
    /* Trigger data collection */
    k_work_submit(&data->process_work);
    
    /* Wait for data ready with timeout */
    int ret = k_sem_take(&data->data_ready, K_MSEC(1000));
    if (ret != 0) {
        LOG_WRN("Sample fetch timeout");
        k_mutex_lock(&data->lock, K_FOREVER);
        data->error_count++;
        k_mutex_unlock(&data->lock);
        return -ETIMEDOUT;
    }
    
    return 0;
}

static int custom_sensor_channel_get(const struct device *dev,
                                    enum sensor_channel chan,
                                    struct sensor_value *val)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    if (!val) {
        return -EINVAL;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    switch (chan) {
    case CUSTOM_SENSOR_CHAN_DATA:
        val->val1 = data->last_sample;
        val->val2 = 0;
        break;
        
    case CUSTOM_SENSOR_CHAN_TEMP:
        /* Simulate temperature reading */
        val->val1 = 25; /* 25°C */
        val->val2 = 500000; /* 0.5°C */
        break;
        
    case CUSTOM_SENSOR_CHAN_STATUS:
        val->val1 = data->status_flags;
        val->val2 = 0;
        break;
        
    default:
        k_mutex_unlock(&data->lock);
        return -ENOTSUP;
    }
    
    k_mutex_unlock(&data->lock);
    LOG_DBG("Channel %d value: %d.%06d", chan, val->val1, val->val2);
    return 0;
}

static int custom_sensor_attr_set(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 const struct sensor_value *val)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    
    if (!val) {
        return -EINVAL;
    }
    
    LOG_DBG("Setting attribute %d to %d.%06d", attr, val->val1, val->val2);
    
    switch (attr) {
    case CUSTOM_SENSOR_ATTR_SAMPLING_FREQ:
        /* In real implementation, update hardware sampling rate */
        LOG_INF("Sampling frequency set to %d Hz", val->val1);
        break;
        
    case CUSTOM_SENSOR_ATTR_RESOLUTION:
        /* In real implementation, update ADC resolution */
        LOG_INF("Resolution set to %d bits", val->val1);
        break;
        
    case CUSTOM_SENSOR_ATTR_CALIBRATION_OFFSET:
        /* Update calibration offset */
        LOG_INF("Calibration offset set to %d", val->val1);
        break;
        
    case CUSTOM_SENSOR_ATTR_FILTER_ENABLE:
        /* Enable/disable filtering */
        LOG_INF("Filtering %s", val->val1 ? "enabled" : "disabled");
        break;
        
    default:
        return -ENOTSUP;
    }
    
    return 0;
}

static int custom_sensor_attr_get(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 struct sensor_value *val)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    if (!val) {
        return -EINVAL;
    }
    
    switch (attr) {
    case CUSTOM_SENSOR_ATTR_SAMPLING_FREQ:
        val->val1 = config->sampling_freq;
        val->val2 = 0;
        break;
        
    case CUSTOM_SENSOR_ATTR_RESOLUTION:
        val->val1 = config->resolution;
        val->val2 = 0;
        break;
        
    case CUSTOM_SENSOR_ATTR_CALIBRATION_OFFSET:
        val->val1 = config->calibration_offset;
        val->val2 = 0;
        break;
        
    case CUSTOM_SENSOR_ATTR_FILTER_ENABLE:
        val->val1 = config->enable_filtering ? 1 : 0;
        val->val2 = 0;
        break;
        
    default:
        return -ENOTSUP;
    }
    
    LOG_DBG("Attribute %d value: %d.%06d", attr, val->val1, val->val2);
    return 0;
}

static int custom_sensor_trigger_set(const struct device *dev,
                                    const struct sensor_trigger *trig,
                                    sensor_trigger_handler_t handler)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    
    if (!trig) {
        return -EINVAL;
    }
    
    LOG_DBG("Setting trigger type %d", trig->type);
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    switch (trig->type) {
    case CUSTOM_SENSOR_TRIG_DATA_READY:
        data->trigger = trig;
        data->trigger_handler = handler;
        
        if (handler && config->irq_config_func) {
            /* Enable interrupt */
            config->irq_config_func();
            LOG_INF("Data ready trigger enabled");
        } else {
            /* Disable interrupt */
            if (config->irq_num) {
                irq_disable(config->irq_num);
            }
            data->trigger_handler = NULL;
            LOG_INF("Data ready trigger disabled");
        }
        break;
        
    default:
        k_mutex_unlock(&data->lock);
        return -ENOTSUP;
    }
    
    k_mutex_unlock(&data->lock);
    return 0;
}

/* Driver lifecycle functions */
static int custom_sensor_init(const struct device *dev)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    int ret;
    
    LOG_INF("Initializing custom sensor %s", dev->name);
    
    /* Store device reference */
    data->dev = dev;
    
    /* Initialize synchronization primitives */
    k_mutex_init(&data->lock);
    k_sem_init(&data->data_ready, 0, 1);
    k_work_init(&data->process_work, custom_sensor_work_handler);
    
    /* Initialize hardware */
    ret = custom_sensor_hardware_init(dev);
    if (ret < 0) {
        LOG_ERR("Hardware initialization failed: %d", ret);
        return ret;
    }
    
    /* Configure interrupt if available */
    if (config->irq_config_func) {
        config->irq_config_func();
        LOG_DBG("Interrupt configured");
    }
    
    /* Set initial state */
    data->initialized = true;
    data->enabled = false;
    data->sample_count = 0;
    data->error_count = 0;
    data->last_sample = 0;
    data->status_flags = 0;
    
    LOG_INF("Custom sensor initialized successfully");
    LOG_DBG("Sampling frequency: %d Hz", config->sampling_freq);
    LOG_DBG("Resolution: %d bits", config->resolution);
    LOG_DBG("Calibration offset: %d", config->calibration_offset);
    LOG_DBG("Filtering: %s", config->enable_filtering ? "enabled" : "disabled");
    
    return 0;
}

/* Public API implementations */
int custom_sensor_enable(const struct device *dev)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    if (!data->initialized) {
        return -ENODEV;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    if (data->enabled) {
        k_mutex_unlock(&data->lock);
        return 0;
    }
    
    /* Enable sensor hardware */
    if (config->enable_pin != 0) {
        const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
        gpio_pin_set(gpio_dev, config->enable_pin, 1);
    }
    
    data->enabled = true;
    k_mutex_unlock(&data->lock);
    
    LOG_INF("Sensor enabled");
    return 0;
}

int custom_sensor_disable(const struct device *dev)
{
    const struct custom_sensor_config *config = custom_sensor_get_config(dev);
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    if (!data->enabled) {
        k_mutex_unlock(&data->lock);
        return 0;
    }
    
    /* Disable sensor hardware */
    if (config->enable_pin != 0) {
        const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
        gpio_pin_set(gpio_dev, config->enable_pin, 0);
    }
    
    data->enabled = false;
    k_mutex_unlock(&data->lock);
    
    LOG_INF("Sensor disabled");
    return 0;
}

bool custom_sensor_is_ready(const struct device *dev)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    k_mutex_lock(&data->lock, K_FOREVER);
    bool ready = data->initialized && data->enabled;
    k_mutex_unlock(&data->lock);
    
    return ready;
}

int custom_sensor_get_stats(const struct device *dev,
                           uint32_t *sample_count,
                           uint32_t *error_count)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    
    if (!sample_count || !error_count) {
        return -EINVAL;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    *sample_count = data->sample_count;
    *error_count = data->error_count;
    k_mutex_unlock(&data->lock);
    
    return 0;
}

/* Power management */
#ifdef CONFIG_PM_DEVICE
static int custom_sensor_pm_action(const struct device *dev,
                                  enum pm_device_action action)
{
    struct custom_sensor_data *data = custom_sensor_get_data(dev);
    int ret = 0;
    
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        LOG_DBG("Suspending sensor");
        k_mutex_lock(&data->lock, K_FOREVER);
        data->pm_suspended = true;
        /* Save state and power down hardware */
        ret = custom_sensor_disable(dev);
        k_mutex_unlock(&data->lock);
        break;
        
    case PM_DEVICE_ACTION_RESUME:
        LOG_DBG("Resuming sensor");
        k_mutex_lock(&data->lock, K_FOREVER);
        data->pm_suspended = false;
        /* Restore state and power up hardware */
        ret = custom_sensor_enable(dev);
        k_mutex_unlock(&data->lock);
        break;
        
    default:
        ret = -ENOTSUP;
        break;
    }
    
    return ret;
}
#endif /* CONFIG_PM_DEVICE */

/* Device instantiation macro */
#define CUSTOM_SENSOR_INIT(inst)                                        \
    static void custom_sensor_irq_config_##inst(void)                   \
    {                                                                    \
        IRQ_CONNECT(DT_INST_IRQN(inst),                                  \
                    DT_INST_IRQ(inst, priority),                         \
                    custom_sensor_isr,                                   \
                    DEVICE_DT_GET(inst), 0);                             \
        irq_enable(DT_INST_IRQN(inst));                                  \
    }                                                                    \
                                                                        \
    static const struct custom_sensor_config custom_sensor_config_##inst = { \
        .base_addr = DT_INST_REG_ADDR(inst),                           \
        .data_pin = DT_INST_PROP(inst, data_pin),                      \
        .enable_pin = DT_INST_PROP_OR(inst, enable_pin, 0),            \
        .sampling_freq = DT_INST_PROP(inst, sampling_frequency),       \
        .resolution = DT_INST_PROP(inst, resolution),                  \
        .calibration_offset = DT_INST_PROP(inst, custom_calibration_offset), \
        .enable_filtering = DT_INST_PROP(inst, custom_enable_filtering), \
        .irq_config_func = COND_CODE_1(DT_INST_IRQ_HAS_IDX(inst, 0),   \
                                      (custom_sensor_irq_config_##inst), \
                                      (NULL)),                          \
        .irq_num = COND_CODE_1(DT_INST_IRQ_HAS_IDX(inst, 0),           \
                              (DT_INST_IRQN(inst)), (0)),              \
        .irq_priority = COND_CODE_1(DT_INST_IRQ_HAS_IDX(inst, 0),      \
                                   (DT_INST_IRQ(inst, priority)), (0)), \
    };                                                                  \
                                                                        \
    static struct custom_sensor_data custom_sensor_data_##inst;        \
                                                                        \
    DEVICE_DT_INST_DEFINE(inst, custom_sensor_init,                    \
                          IF_ENABLED(CONFIG_PM_DEVICE,                 \
                                   (custom_sensor_pm_action)),          \
                          &custom_sensor_data_##inst,                   \
                          &custom_sensor_config_##inst,                 \
                          POST_KERNEL,                                  \
                          CONFIG_CUSTOM_SENSOR_INIT_PRIORITY,           \
                          &custom_sensor_api);

/* Create device instances for all enabled device tree nodes */
DT_INST_FOREACH_STATUS_OKAY(CUSTOM_SENSOR_INIT)
```

## Part 4: Test Application and Integration (25 minutes)

### Step 6: Test Application

Create `src/main.c`:

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/custom_sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_test, LOG_LEVEL_INF);

/* Get the custom sensor device */
static const struct device *sensor_dev = DEVICE_DT_GET_ANY(custom_sensor);

/* Trigger callback function */
static void sensor_trigger_handler(const struct device *dev,
                                  const struct sensor_trigger *trigger)
{
    LOG_INF("Sensor trigger received: type=%d", trigger->type);
    
    /* Fetch and display sensor data */
    if (sensor_sample_fetch(dev, SENSOR_CHAN_ALL) == 0) {
        struct sensor_value data;
        
        if (sensor_channel_get(dev, CUSTOM_SENSOR_CHAN_DATA, &data) == 0) {
            LOG_INF("Triggered data reading: %d.%06d", 
                    data.val1, data.val2);
        }
    }
}

/* Test basic sensor operations */
static void test_basic_operations(void)
{
    struct sensor_value val;
    int ret;
    
    LOG_INF("=== Testing Basic Sensor Operations ===");
    
    /* Test device readiness */
    if (!custom_sensor_is_ready(sensor_dev)) {
        LOG_INF("Sensor not ready, enabling...");
        ret = custom_sensor_enable(sensor_dev);
        if (ret != 0) {
            LOG_ERR("Failed to enable sensor: %d", ret);
            return;
        }
    }
    
    /* Test sample fetch and channel get */
    for (int i = 0; i < 10; i++) {
        ret = sensor_sample_fetch(sensor_dev, CUSTOM_SENSOR_CHAN_DATA);
        if (ret != 0) {
            LOG_ERR("Sample fetch failed: %d", ret);
            continue;
        }
        
        ret = sensor_channel_get(sensor_dev, CUSTOM_SENSOR_CHAN_DATA, &val);
        if (ret == 0) {
            LOG_INF("Sample %d: %d.%06d", i + 1, val.val1, val.val2);
        }
        
        /* Test temperature channel */
        ret = sensor_channel_get(sensor_dev, CUSTOM_SENSOR_CHAN_TEMP, &val);
        if (ret == 0) {
            LOG_INF("Temperature: %d.%06d°C", val.val1, val.val2);
        }
        
        k_msleep(500);
    }
}

/* Test sensor attributes */
static void test_sensor_attributes(void)
{
    struct sensor_value val;
    int ret;
    
    LOG_INF("=== Testing Sensor Attributes ===");
    
    /* Get current sampling frequency */
    ret = sensor_attr_get(sensor_dev, SENSOR_CHAN_ALL,
                         CUSTOM_SENSOR_ATTR_SAMPLING_FREQ, &val);
    if (ret == 0) {
        LOG_INF("Current sampling frequency: %d Hz", val.val1);
    }
    
    /* Set new sampling frequency */
    val.val1 = 200;
    val.val2 = 0;
    ret = sensor_attr_set(sensor_dev, SENSOR_CHAN_ALL,
                         CUSTOM_SENSOR_ATTR_SAMPLING_FREQ, &val);
    if (ret == 0) {
        LOG_INF("Sampling frequency set to %d Hz", val.val1);
    }
    
    /* Get resolution */
    ret = sensor_attr_get(sensor_dev, SENSOR_CHAN_ALL,
                         CUSTOM_SENSOR_ATTR_RESOLUTION, &val);
    if (ret == 0) {
        LOG_INF("ADC resolution: %d bits", val.val1);
    }
    
    /* Get calibration offset */
    ret = sensor_attr_get(sensor_dev, SENSOR_CHAN_ALL,
                         CUSTOM_SENSOR_ATTR_CALIBRATION_OFFSET, &val);
    if (ret == 0) {
        LOG_INF("Calibration offset: %d", val.val1);
    }
}

/* Test trigger functionality */
static void test_sensor_triggers(void)
{
    struct sensor_trigger trigger;
    int ret;
    
    LOG_INF("=== Testing Sensor Triggers ===");
    
    /* Configure data ready trigger */
    trigger.type = CUSTOM_SENSOR_TRIG_DATA_READY;
    trigger.chan = CUSTOM_SENSOR_CHAN_DATA;
    
    ret = sensor_trigger_set(sensor_dev, &trigger, sensor_trigger_handler);
    if (ret == 0) {
        LOG_INF("Data ready trigger configured");
        LOG_INF("Waiting for triggered samples...");
        
        /* Wait for some triggered samples */
        k_msleep(5000);
        
        /* Disable trigger */
        ret = sensor_trigger_set(sensor_dev, &trigger, NULL);
        if (ret == 0) {
            LOG_INF("Trigger disabled");
        }
    } else {
        LOG_WRN("Trigger configuration failed: %d", ret);
    }
}

/* Test statistics and error handling */
static void test_statistics(void)
{
    uint32_t sample_count, error_count;
    int ret;
    
    LOG_INF("=== Testing Statistics ===");
    
    ret = custom_sensor_get_stats(sensor_dev, &sample_count, &error_count);
    if (ret == 0) {
        LOG_INF("Total samples: %u", sample_count);
        LOG_INF("Total errors: %u", error_count);
    }
}

/* Test power management */
static void test_power_management(void)
{
    int ret;
    
    LOG_INF("=== Testing Power Management ===");
    
#ifdef CONFIG_PM_DEVICE
    /* Test device suspend */
    ret = pm_device_action_run(sensor_dev, PM_DEVICE_ACTION_SUSPEND);
    if (ret == 0) {
        LOG_INF("Device suspended successfully");
        
        /* Verify device is not ready */
        if (!custom_sensor_is_ready(sensor_dev)) {
            LOG_INF("Device correctly reported as not ready");
        }
        
        k_msleep(2000);
        
        /* Resume device */
        ret = pm_device_action_run(sensor_dev, PM_DEVICE_ACTION_RESUME);
        if (ret == 0) {
            LOG_INF("Device resumed successfully");
            
            /* Verify device is ready again */
            if (custom_sensor_is_ready(sensor_dev)) {
                LOG_INF("Device correctly resumed and ready");
            }
        }
    }
#else
    LOG_INF("Power management not enabled");
#endif
}

int main(void)
{
    LOG_INF("Custom Sensor Driver Test Application");
    LOG_INF("Zephyr version: %s", KERNEL_VERSION_STRING);
    
    /* Check if sensor device is available */
    if (!device_is_ready(sensor_dev)) {
        LOG_ERR("Sensor device not ready");
        return -1;
    }
    
    LOG_INF("Sensor device: %s", sensor_dev->name);
    
    /* Run test sequence */
    test_basic_operations();
    k_msleep(1000);
    
    test_sensor_attributes();
    k_msleep(1000);
    
    test_sensor_triggers();
    k_msleep(1000);
    
    test_statistics();
    k_msleep(1000);
    
    test_power_management();
    
    LOG_INF("All tests completed");
    
    /* Continuous operation demo */
    LOG_INF("Starting continuous operation demo...");
    
    while (1) {
        struct sensor_value data;
        
        if (sensor_sample_fetch(sensor_dev, SENSOR_CHAN_ALL) == 0) {
            if (sensor_channel_get(sensor_dev, CUSTOM_SENSOR_CHAN_DATA, 
                                 &data) == 0) {
                LOG_INF("Continuous reading: %d.%06d", 
                        data.val1, data.val2);
            }
        }
        
        k_msleep(2000);
    }
    
    return 0;
}
```

## Part 5: Build Configuration and Testing (15 minutes)

### Step 7: Build Configuration

Update `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_driver_lab)

# Add source files
target_sources(app PRIVATE src/main.c)

# Add driver sources
zephyr_library_sources_ifdef(CONFIG_CUSTOM_SENSOR 
                             drivers/sensor/custom_sensor.c)

# Add include directories
zephyr_include_directories(include)
```

Add to `prj.conf`:

```conf
# Enable custom sensor driver
CONFIG_CUSTOM_SENSOR=y
CONFIG_CUSTOM_SENSOR_INIT_PRIORITY=80
CONFIG_CUSTOM_SENSOR_LOG_LEVEL=3
```

### Step 8: Build and Test

Build the project:

```bash
west build -b nrf52840dk_nrf52840 --pristine
west flash
```

Monitor the output:

```bash
west debug
# or use serial terminal
minicom -D /dev/ttyACM0 -b 115200
```

## Lab Exercises

### Exercise 1: Add DMA Support

Implement DMA-based data transfer for high-speed sensor reading.

### Exercise 2: Multi-Instance Support

Modify the driver to support multiple sensor instances with different configurations.

### Exercise 3: Advanced Filtering

Implement more sophisticated filtering algorithms (Kalman filter, low-pass filter).

### Exercise 4: Error Recovery

Add comprehensive error detection and recovery mechanisms.

## Expected Outcomes

Upon completion, you will have:

1. **Complete Device Driver**: A fully functional, professional-grade device driver
2. **Device Tree Integration**: Proper device tree bindings and configuration
3. **API Design**: Clean, consistent sensor API following Zephyr conventions
4. **Power Management**: Working suspend/resume functionality
5. **Interrupt Handling**: Proper interrupt-driven operation
6. **Error Handling**: Robust error detection and reporting
7. **Testing Framework**: Comprehensive test application

## Troubleshooting Guide

| Issue | Cause | Solution |
|-------|-------|----------|
| Build errors | Missing includes or config | Check prj.conf and includes |
| Device not found | Device tree issues | Verify overlay and bindings |
| No sensor readings | Hardware not initialized | Check init function and GPIO config |
| Interrupt not working | IRQ configuration | Verify device tree interrupt config |
| Power management fails | PM function issues | Check PM_DEVICE config and functions |

This lab provides comprehensive experience in professional device driver development for Zephyr RTOS.

[Next: Chapter 17 - Power Management](../chapter_17_power_management/README.md)
