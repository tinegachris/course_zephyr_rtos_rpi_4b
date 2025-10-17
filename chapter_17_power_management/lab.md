# Chapter 17 - Power Management Lab

## Lab Overview

In this lab, you will implement a comprehensive power management system for a battery-powered environmental monitoring device. The project demonstrates system-level power management, device runtime PM, and power optimization strategies.

## Learning Objectives

By the end of this lab, you will be able to:
- Implement device-level power management for sensors and peripherals
- Configure system power states and wake-up sources
- Use runtime power management APIs effectively
- Optimize power consumption for battery-powered applications
- Debug and monitor power management behavior

## Prerequisites

- Completion of Chapters 1-16
- Understanding of device drivers and interrupt handling
- Familiarity with device tree configuration
- Basic knowledge of power management concepts

## Lab Project: Environmental Monitoring Station

### Project Description

You'll create a battery-powered environmental monitoring station that:
- Collects temperature and humidity data periodically
- Transmits data over Bluetooth Low Energy (BLE)
- Uses aggressive power management to extend battery life
- Supports multiple wake-up sources (timer, button, BLE connection)
- Implements dynamic power scaling based on activity

## Part 1: Project Setup

### 1.1 Create Project Structure

```bash
# Create project directory
mkdir -p ~/zephyr-workspace/power_management_lab
cd ~/zephyr-workspace/power_management_lab

# Create source files
mkdir -p src include boards overlays
mkdir -p dts/bindings/sensor

# Initialize Git repository
git init
```

### 1.2 Main Application Structure

Create `src/main.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>

#include "power_manager.h"
#include "sensor_manager.h"
#include "bluetooth_manager.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Configuration */
#define MEASUREMENT_INTERVAL_MS    30000  /* 30 seconds */
#define DEEP_SLEEP_TIMEOUT_MS      300000 /* 5 minutes */
#define LOW_BATTERY_THRESHOLD_MV   3200

/* Thread stacks */
K_THREAD_STACK_DEFINE(sensor_stack, 2048);
K_THREAD_STACK_DEFINE(bluetooth_stack, 2048);
K_THREAD_STACK_DEFINE(power_mgmt_stack, 1024);

/* Thread control blocks */
static struct k_thread sensor_thread;
static struct k_thread bluetooth_thread;
static struct k_thread power_mgmt_thread;

/* Synchronization */
static struct k_event system_events;

/* System events */
#define EVENT_MEASUREMENT_READY    BIT(0)
#define EVENT_BLE_CONNECTED        BIT(1)
#define EVENT_BLE_DISCONNECTED     BIT(2)
#define EVENT_LOW_BATTERY          BIT(3)
#define EVENT_WAKEUP_BUTTON        BIT(4)

/* Global system state */
static struct system_state {
    bool ble_connected;
    bool low_battery;
    uint32_t battery_voltage_mv;
    uint32_t measurement_count;
    k_timeout_t measurement_interval;
    uint64_t last_activity_time;
} sys_state = {
    .measurement_interval = K_MSEC(MEASUREMENT_INTERVAL_MS),
};

/* Sensor measurement thread */
static void sensor_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    struct sensor_data data;
    int ret;
    
    LOG_INF("Sensor thread started");
    
    /* Initialize sensor manager */
    ret = sensor_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize sensor manager: %d", ret);
        return;
    }
    
    while (1) {
        /* Wait for measurement interval or BLE connection */
        if (sys_state.ble_connected) {
            /* More frequent measurements when connected */
            k_sleep(K_MSEC(5000));
        } else {
            k_sleep(sys_state.measurement_interval);
        }
        
        /* Take measurement */
        ret = sensor_manager_read(&data);
        if (ret == 0) {
            sys_state.measurement_count++;
            
            LOG_INF("Measurement %u: Temp=%.2f°C, Humidity=%.1f%%",
                   sys_state.measurement_count,
                   data.temperature_c,
                   data.humidity_percent);
            
            /* Store measurement for transmission */
            bluetooth_manager_queue_data(&data);
            
            /* Notify other threads */
            k_event_post(&system_events, EVENT_MEASUREMENT_READY);
        }
        
        /* Check battery voltage */
        uint32_t battery_mv = sensor_manager_read_battery();
        sys_state.battery_voltage_mv = battery_mv;
        
        if (battery_mv < LOW_BATTERY_THRESHOLD_MV && !sys_state.low_battery) {
            sys_state.low_battery = true;
            k_event_post(&system_events, EVENT_LOW_BATTERY);
        }
    }
}

/* Bluetooth management thread */
static void bluetooth_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    int ret;
    uint32_t events;
    
    LOG_INF("Bluetooth thread started");
    
    /* Initialize Bluetooth */
    ret = bluetooth_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize Bluetooth: %d", ret);
        return;
    }
    
    while (1) {
        /* Wait for events */
        events = k_event_wait(&system_events,
                             EVENT_MEASUREMENT_READY | EVENT_BLE_CONNECTED | 
                             EVENT_BLE_DISCONNECTED | EVENT_LOW_BATTERY,
                             false, K_FOREVER);
        
        if (events & EVENT_BLE_CONNECTED) {
            LOG_INF("BLE connected");
            sys_state.ble_connected = true;
            
            /* Switch to active power mode */
            power_manager_set_performance_mode(POWER_MODE_ACTIVE);
        }
        
        if (events & EVENT_BLE_DISCONNECTED) {
            LOG_INF("BLE disconnected");
            sys_state.ble_connected = false;
            sys_state.last_activity_time = k_uptime_get();
            
            /* Return to power saving mode */
            power_manager_set_performance_mode(POWER_MODE_ECO);
        }
        
        if (events & EVENT_MEASUREMENT_READY) {
            /* Transmit queued data if connected */
            if (sys_state.ble_connected) {
                bluetooth_manager_transmit_queued();
                sys_state.last_activity_time = k_uptime_get();
            }
        }
        
        if (events & EVENT_LOW_BATTERY) {
            LOG_WRN("Low battery detected: %u mV", sys_state.battery_voltage_mv);
            
            /* Enter ultra-low power mode */
            power_manager_set_performance_mode(POWER_MODE_ULTRA_LOW);
            
            /* Extend measurement interval */
            sys_state.measurement_interval = K_MSEC(MEASUREMENT_INTERVAL_MS * 4);
        }
    }
}

/* Power management thread */
static void power_mgmt_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Power management thread started");
    
    /* Initialize power manager */
    int ret = power_manager_init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize power manager: %d", ret);
        return;
    }
    
    /* Start in eco mode */
    power_manager_set_performance_mode(POWER_MODE_ECO);
    
    while (1) {
        /* Monitor system activity and adjust power settings */
        k_sleep(K_SECONDS(10));
        
        /* Print power statistics */
        power_manager_print_stats();
        
        /* Check for deep sleep opportunity */
        if (!sys_state.ble_connected && 
            (k_uptime_get() - sys_state.last_activity_time) > DEEP_SLEEP_TIMEOUT_MS) {
            
            LOG_INF("Entering deep sleep mode");
            power_manager_enter_deep_sleep();
        }
    }
}

/* Button interrupt callback */
static struct gpio_callback button_cb_data;
static void button_pressed_callback(const struct device *dev,
                                   struct gpio_callback *cb,
                                   uint32_t pins)
{
    LOG_INF("Wake-up button pressed");
    k_event_post(&system_events, EVENT_WAKEUP_BUTTON);
}

/* Initialize wake-up sources */
static int init_wakeup_sources(void)
{
    const struct device *button_dev = DEVICE_DT_GET(DT_ALIAS(sw0));
    int ret;
    
    if (!device_is_ready(button_dev)) {
        LOG_ERR("Button device not ready");
        return -ENODEV;
    }
    
    /* Configure button as wake-up source */
    ret = gpio_pin_configure_dt(&(struct gpio_dt_spec)DT_GPIO_CTLR_PIN(DT_ALIAS(sw0), gpios),
                               GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        return ret;
    }
    
    /* Configure interrupt */
    ret = gpio_pin_interrupt_configure_dt(&(struct gpio_dt_spec)DT_GPIO_CTLR_PIN(DT_ALIAS(sw0), gpios),
                                         GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    
    /* Add callback */
    gpio_init_callback(&button_cb_data, button_pressed_callback,
                       BIT(DT_GPIO_PIN(DT_ALIAS(sw0), gpios)));
    gpio_add_callback(button_dev, &button_cb_data);
    
    /* Enable as wake-up source */
    pm_device_wakeup_enable(button_dev, true);
    
    LOG_INF("Wake-up sources initialized");
    return 0;
}

int main(void)
{
    int ret;
    
    LOG_INF("Environmental Monitoring Station Starting");
    LOG_INF("Build time: " __DATE__ " " __TIME__);
    
    /* Initialize system events */
    k_event_init(&system_events);
    
    /* Initialize wake-up sources */
    ret = init_wakeup_sources();
    if (ret < 0) {
        LOG_ERR("Failed to initialize wake-up sources: %d", ret);
        return ret;
    }
    
    /* Create application threads */
    k_thread_create(&sensor_thread, sensor_stack,
                   K_THREAD_STACK_SIZEOF(sensor_stack),
                   sensor_thread_entry, NULL, NULL, NULL,
                   K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&sensor_thread, "sensor");
    
    k_thread_create(&bluetooth_thread, bluetooth_stack,
                   K_THREAD_STACK_SIZEOF(bluetooth_stack),
                   bluetooth_thread_entry, NULL, NULL, NULL,
                   K_PRIO_COOP(6), 0, K_NO_WAIT);
    k_thread_name_set(&bluetooth_thread, "bluetooth");
    
    k_thread_create(&power_mgmt_thread, power_mgmt_stack,
                   K_THREAD_STACK_SIZEOF(power_mgmt_stack),
                   power_mgmt_thread_entry, NULL, NULL, NULL,
                   K_PRIO_COOP(8), 0, K_NO_WAIT);
    k_thread_name_set(&power_mgmt_thread, "power_mgmt");
    
    LOG_INF("System initialized successfully");
    
    return 0;
}
```

### 1.3 Supporting Modules

Create `include/bluetooth_manager.h`:
```c
#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include "sensor_manager.h"

int bluetooth_manager_init(void);
void bluetooth_manager_queue_data(const struct sensor_data *data);
void bluetooth_manager_transmit_queued(void);

#endif /* BLUETOOTH_MANAGER_H */
```

Create `src/bluetooth_manager.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "bluetooth_manager.h"

LOG_MODULE_REGISTER(bluetooth_manager, LOG_LEVEL_INF);

int bluetooth_manager_init(void)
{
    LOG_INF("Bluetooth Manager Initialized (simulation)");
    return 0;
}

void bluetooth_manager_queue_data(const struct sensor_data *data)
{
    LOG_DBG("Queued sensor data for BLE transmission");
}

void bluetooth_manager_transmit_queued(void)
{
    LOG_INF("Transmitting queued data over BLE (simulation)");
}
```

## Part 2: Power Manager Implementation

### 2.1 Power Manager Header

Create `include/power_manager.h`:
```c
#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/pm.h>

/**
 * @brief Power performance modes
 */
enum power_mode {
    POWER_MODE_ACTIVE,      /* Maximum performance */
    POWER_MODE_BALANCED,    /* Balanced performance/power */
    POWER_MODE_ECO,         /* Power saving mode */
    POWER_MODE_ULTRA_LOW,   /* Ultra low power mode */
};

/**
 * @brief Power statistics
 */
struct power_stats {
    uint64_t active_time_us;
    uint64_t idle_time_us;
    uint32_t sleep_count;
    uint32_t wakeup_count;
    uint32_t deep_sleep_count;
    uint32_t cpu_frequency_hz;
    enum power_mode current_mode;
};

/**
 * @brief Initialize power management system
 * 
 * @return 0 on success, negative errno on failure
 */
int power_manager_init(void);

/**
 * @brief Set system performance mode
 * 
 * @param mode Target power mode
 * @return 0 on success, negative errno on failure
 */
int power_manager_set_performance_mode(enum power_mode mode);

/**
 * @brief Get current performance mode
 * 
 * @return Current power mode
 */
enum power_mode power_manager_get_performance_mode(void);

/**
 * @brief Enter deep sleep mode
 * 
 * @return 0 on success, negative errno on failure
 */
int power_manager_enter_deep_sleep(void);

/**
 * @brief Get power management statistics
 * 
 * @param stats Pointer to statistics structure
 * @return 0 on success, negative errno on failure
 */
int power_manager_get_stats(struct power_stats *stats);

/**
 * @brief Print power management statistics
 */
void power_manager_print_stats(void);

/**
 * @brief Register device for power management
 * 
 * @param dev Device to register
 * @return 0 on success, negative errno on failure
 */
int power_manager_register_device(const struct device *dev);

/**
 * @brief Unregister device from power management
 * 
 * @param dev Device to unregister
 * @return 0 on success, negative errno on failure
 */
int power_manager_unregister_device(const struct device *dev);

#endif /* POWER_MANAGER_H */
```

### 2.2 Power Manager Implementation

Create `src/power_manager.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "power_manager.h"

LOG_MODULE_REGISTER(power_manager, LOG_LEVEL_DBG);

/* Configuration */
#define MAX_MANAGED_DEVICES    16
#define DEEP_SLEEP_MIN_TIME_MS 5000

/* Power mode configurations */
static const struct {
    uint32_t cpu_freq_hz;
    bool peripheral_power;
    uint32_t idle_threshold_ms;
} power_mode_config[] = {
    [POWER_MODE_ACTIVE] = {
        .cpu_freq_hz = 168000000,    /* 168 MHz */
        .peripheral_power = true,
        .idle_threshold_ms = 10,
    },
    [POWER_MODE_BALANCED] = {
        .cpu_freq_hz = 84000000,     /* 84 MHz */
        .peripheral_power = true,
        .idle_threshold_ms = 50,
    },
    [POWER_MODE_ECO] = {
        .cpu_freq_hz = 48000000,     /* 48 MHz */
        .peripheral_power = true,
        .idle_threshold_ms = 100,
    },
    [POWER_MODE_ULTRA_LOW] = {
        .cpu_freq_hz = 16000000,     /* 16 MHz */
        .peripheral_power = false,
        .idle_threshold_ms = 500,
    },
};

/* Power manager state */
static struct {
    enum power_mode current_mode;
    bool initialized;
    
    /* Managed devices */
    const struct device *managed_devices[MAX_MANAGED_DEVICES];
    size_t device_count;
    
    /* Statistics */
    struct power_stats stats;
    uint64_t last_update_time;
    
    /* Deep sleep */
    bool deep_sleep_enabled;
    struct k_work_delayable deep_sleep_work;
    
} pm_state = {
    .current_mode = POWER_MODE_ACTIVE,
};

/* Forward declarations */
static int configure_cpu_frequency(uint32_t freq_hz);
static int configure_peripheral_power(bool enable);
static void deep_sleep_work_handler(struct k_work *work);

int power_manager_init(void)
{
    if (pm_state.initialized) {
        return -EALREADY;
    }
    
    LOG_INF("Initializing power management system");
    
    /* Initialize work queue for deep sleep */
    k_work_init_delayable(&pm_state.deep_sleep_work, deep_sleep_work_handler);
    
    /* Initialize statistics */
    pm_state.stats.cpu_frequency_hz = power_mode_config[POWER_MODE_ACTIVE].cpu_freq_hz;
    pm_state.stats.current_mode = POWER_MODE_ACTIVE;
    pm_state.last_update_time = k_uptime_get();
    
    /* Enable runtime PM for system devices */
    STRUCT_SECTION_FOREACH(device, dev) {
        if (pm_device_runtime_is_enabled(dev)) {
            pm_device_runtime_enable(dev);
        }
    }
    
    pm_state.initialized = true;
    LOG_INF("Power management initialized");
    
    return 0;
}

int power_manager_set_performance_mode(enum power_mode mode)
{
    if (!pm_state.initialized) {
        return -ENODEV;
    }
    
    if (mode >= ARRAY_SIZE(power_mode_config)) {
        return -EINVAL;
    }
    
    if (mode == pm_state.current_mode) {
        return 0; /* No change needed */
    }
    
    LOG_INF("Switching power mode: %d -> %d", pm_state.current_mode, mode);
    
    /* Configure CPU frequency */
    int ret = configure_cpu_frequency(power_mode_config[mode].cpu_freq_hz);
    if (ret < 0) {
        LOG_ERR("Failed to configure CPU frequency: %d", ret);
        return ret;
    }
    
    /* Configure peripheral power */
    ret = configure_peripheral_power(power_mode_config[mode].peripheral_power);
    if (ret < 0) {
        LOG_ERR("Failed to configure peripheral power: %d", ret);
        return ret;
    }
    
    /* Update state */
    enum power_mode old_mode = pm_state.current_mode;
    pm_state.current_mode = mode;
    pm_state.stats.current_mode = mode;
    pm_state.stats.cpu_frequency_hz = power_mode_config[mode].cpu_freq_hz;
    
    /* Enable/disable deep sleep based on mode */
    if (mode == POWER_MODE_ULTRA_LOW) {
        pm_state.deep_sleep_enabled = true;
        k_work_schedule(&pm_state.deep_sleep_work,
                       K_MSEC(DEEP_SLEEP_MIN_TIME_MS));
    } else {
        pm_state.deep_sleep_enabled = false;
        k_work_cancel_delayable(&pm_state.deep_sleep_work);
    }
    
    LOG_INF("Power mode changed: %d -> %d (CPU: %u Hz)",
           old_mode, mode, power_mode_config[mode].cpu_freq_hz);
    
    return 0;
}

enum power_mode power_manager_get_performance_mode(void)
{
    return pm_state.current_mode;
}

static int configure_cpu_frequency(uint32_t freq_hz)
{
    const struct device *clk_dev = DEVICE_DT_GET(DT_NODELABEL(rcc));
    
    if (!device_is_ready(clk_dev)) {
        return -ENODEV;
    }
    
    /* Implementation depends on specific SoC */
    /* This is a placeholder for actual clock configuration */
    
    LOG_DBG("CPU frequency configured to %u Hz", freq_hz);
    return 0;
}

static int configure_peripheral_power(bool enable)
{
    int ret = 0;
    
    /* Manage power for registered devices */
    for (size_t i = 0; i < pm_state.device_count; i++) {
        const struct device *dev = pm_state.managed_devices[i];
        
        if (enable) {
            ret = pm_device_runtime_get(dev);
        } else {
            ret = pm_device_runtime_put(dev);
        }
        
        if (ret < 0) {
            LOG_WRN("Failed to %s device %s: %d",
                   enable ? "enable" : "disable",
                   dev->name, ret);
        }
    }
    
    LOG_DBG("Peripheral power %s", enable ? "enabled" : "disabled");
    return 0;
}

int power_manager_enter_deep_sleep(void)
{
    if (!pm_state.deep_sleep_enabled) {
        return -EACCES;
    }
    
    LOG_INF("Entering deep sleep mode");
    
    /* Suspend all managed devices */
    for (size_t i = 0; i < pm_state.device_count; i++) {
        pm_device_action_run(pm_state.managed_devices[i],
                            PM_DEVICE_ACTION_SUSPEND);
    }
    
    /* Update statistics */
    pm_state.stats.deep_sleep_count++;
    
    /* Enter system suspend state */
    pm_system_suspend(K_FOREVER);
    
    LOG_INF("Resumed from deep sleep");
    
    /* Resume all managed devices */
    for (size_t i = 0; i < pm_state.device_count; i++) {
        pm_device_action_run(pm_state.managed_devices[i],
                            PM_DEVICE_ACTION_RESUME);
    }
    
    return 0;
}

static void deep_sleep_work_handler(struct k_work *work)
{
    if (pm_state.deep_sleep_enabled) {
        power_manager_enter_deep_sleep();
    }
}

int power_manager_register_device(const struct device *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    if (pm_state.device_count >= MAX_MANAGED_DEVICES) {
        return -ENOMEM;
    }
    
    /* Check if already registered */
    for (size_t i = 0; i < pm_state.device_count; i++) {
        if (pm_state.managed_devices[i] == dev) {
            return -EALREADY;
        }
    }
    
    /* Add to managed devices */
    pm_state.managed_devices[pm_state.device_count++] = dev;
    
    /* Enable runtime PM if supported */
    pm_device_runtime_enable(dev);
    
    LOG_DBG("Registered device %s for power management", dev->name);
    return 0;
}

int power_manager_unregister_device(const struct device *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    /* Find device in list */
    size_t index = SIZE_MAX;
    for (size_t i = 0; i < pm_state.device_count; i++) {
        if (pm_state.managed_devices[i] == dev) {
            index = i;
            break;
        }
    }
    
    if (index == SIZE_MAX) {
        return -ENOENT;
    }
    
    /* Remove from list */
    for (size_t i = index; i < pm_state.device_count - 1; i++) {
        pm_state.managed_devices[i] = pm_state.managed_devices[i + 1];
    }
    pm_state.device_count--;
    
    /* Disable runtime PM */
    pm_device_runtime_disable(dev);
    
    LOG_DBG("Unregistered device %s from power management", dev->name);
    return 0;
}

int power_manager_get_stats(struct power_stats *stats)
{
    if (!stats) {
        return -EINVAL;
    }
    
    /* Update current statistics */
    uint64_t current_time = k_uptime_get();
    uint64_t elapsed_us = (current_time - pm_state.last_update_time) * 1000;
    
    /* Update cumulative statistics */
    pm_state.stats.active_time_us += elapsed_us;
    pm_state.last_update_time = current_time;
    
    /* Copy statistics */
    *stats = pm_state.stats;
    
    return 0;
}

void power_manager_print_stats(void)
{
    struct power_stats stats;
    
    if (power_manager_get_stats(&stats) != 0) {
        LOG_ERR("Failed to get power statistics");
        return;
    }
    
    LOG_INF("=== Power Management Statistics ===");
    LOG_INF("Current mode: %d", stats.current_mode);
    LOG_INF("CPU frequency: %u Hz", stats.cpu_frequency_hz);
    LOG_INF("Active time: %llu us", stats.active_time_us);
    LOG_INF("Sleep count: %u", stats.sleep_count);
    LOG_INF("Wake-up count: %u", stats.wakeup_count);
    LOG_INF("Deep sleep count: %u", stats.deep_sleep_count);
    LOG_INF("Managed devices: %zu", pm_state.device_count);
    
    /* Print device states */
    for (size_t i = 0; i < pm_state.device_count; i++) {
        enum pm_device_state state;
        const struct device *dev = pm_state.managed_devices[i];
        
        if (pm_device_state_get(dev, &state) == 0) {
            LOG_INF("Device %s: state=%d", dev->name, state);
        }
    }
}

/* Custom power policy implementation */
const struct pm_state_info *pm_policy_next_state(uint8_t cpu, int32_t ticks)
{
    static const struct pm_state_info states[] = {
        {PM_STATE_RUNTIME_IDLE, 0, 0},
        {PM_STATE_SUSPEND_TO_IDLE, 0, 1000},    /* 1ms min residency */
        {PM_STATE_SUSPEND_TO_RAM, 0, 10000},    /* 10ms min residency */
    };
    
    uint32_t residency_us = k_ticks_to_us_floor32(ticks);
    uint32_t threshold = power_mode_config[pm_state.current_mode].idle_threshold_ms * 1000;
    
    /* Apply current power mode policy */
    if (residency_us < threshold) {
        return NULL; /* Stay active */
    }
    
    /* Select appropriate state */
    for (int i = ARRAY_SIZE(states) - 1; i >= 0; i--) {
        if (residency_us >= states[i].min_residency_us) {
            pm_state.stats.sleep_count++;
            return &states[i];
        }
    }
    
    return NULL;
}

/* Power management event callbacks */
void pm_state_exit_post_ops(enum pm_state state, uint8_t cpu)
{
    ARG_UNUSED(cpu);
    
    pm_state.stats.wakeup_count++;
    
    LOG_DBG("Resumed from power state %d", state);
}
```

## Part 3: Sensor Manager with Power Management

### 3.1 Sensor Manager Header

Create `include/sensor_manager.h`:
```c
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <zephyr/kernel.h>

/**
 * @brief Sensor data structure
 */
struct sensor_data {
    float temperature_c;
    float humidity_percent;
    uint64_t timestamp;
    uint32_t sequence_number;
};

/**
 * @brief Initialize sensor management system
 * 
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_init(void);

/**
 * @brief Read sensor data
 * 
 * @param data Pointer to sensor data structure
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_read(struct sensor_data *data);

/**
 * @brief Read battery voltage
 * 
 * @return Battery voltage in millivolts
 */
uint32_t sensor_manager_read_battery(void);

/**
 * @brief Put sensor into low power mode
 * 
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_sleep(void);

/**
 * @brief Wake up sensor from low power mode
 * 
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_wakeup(void);

#endif /* SENSOR_MANAGER_H */
```

### 3.2 Sensor Manager Implementation

Create `src/sensor_manager.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>

#include "sensor_manager.h"
#include "power_manager.h"

LOG_MODULE_REGISTER(sensor_manager, LOG_LEVEL_DBG);

/* Device tree references */
#define TEMP_SENSOR_NODE DT_NODELABEL(temp_sensor)
#define ADC_NODE DT_NODELABEL(adc)
#define BATTERY_CHANNEL 0

/* Sensor manager state */
static struct {
    const struct device *temp_sensor;
    const struct device *adc_dev;
    bool initialized;
    uint32_t sequence_number;
    
    /* ADC configuration for battery monitoring */
    struct adc_channel_cfg adc_cfg;
    struct adc_sequence adc_seq;
    uint16_t adc_buffer;
    
} sensor_mgr = {0};

/* Power management callbacks for temperature sensor */
static int temp_sensor_pm_action(const struct device *dev,
                                enum pm_device_action action)
{
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        LOG_DBG("Temperature sensor suspended");
        /* Put sensor in sleep mode */
        break;
        
    case PM_DEVICE_ACTION_RESUME:
        LOG_DBG("Temperature sensor resumed");
        /* Wake up sensor */
        break;
        
    case PM_DEVICE_ACTION_TURN_OFF:
        LOG_DBG("Temperature sensor powered off");
        break;
        
    case PM_DEVICE_ACTION_TURN_ON:
        LOG_DBG("Temperature sensor powered on");
        break;
        
    default:
        return -ENOTSUP;
    }
    
    return 0;
}

int sensor_manager_init(void)
{
    if (sensor_mgr.initialized) {
        return -EALREADY;
    }
    
    LOG_INF("Initializing sensor manager");
    
    /* Get temperature sensor device */
    sensor_mgr.temp_sensor = DEVICE_DT_GET_OR_NULL(TEMP_SENSOR_NODE);
    if (!sensor_mgr.temp_sensor) {
        LOG_WRN("Temperature sensor not found, using simulated values");
    } else if (!device_is_ready(sensor_mgr.temp_sensor)) {
        LOG_ERR("Temperature sensor not ready");
        return -ENODEV;
    }
    
    /* Get ADC device for battery monitoring */
    sensor_mgr.adc_dev = DEVICE_DT_GET_OR_NULL(ADC_NODE);
    if (!sensor_mgr.adc_dev) {
        LOG_WRN("ADC not found, using simulated battery values");
    } else if (!device_is_ready(sensor_mgr.adc_dev)) {
        LOG_ERR("ADC not ready");
        return -ENODEV;
    } else {
        /* Configure ADC for battery monitoring */
        sensor_mgr.adc_cfg = (struct adc_channel_cfg) {
            .gain = ADC_GAIN_1,
            .reference = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,
            .channel_id = BATTERY_CHANNEL,
        };
        
        int ret = adc_channel_setup(sensor_mgr.adc_dev, &sensor_mgr.adc_cfg);
        if (ret < 0) {
            LOG_ERR("Failed to setup ADC channel: %d", ret);
            return ret;
        }
        
        /* Configure ADC sequence */
        sensor_mgr.adc_seq = (struct adc_sequence) {
            .channels = BIT(BATTERY_CHANNEL),
            .buffer = &sensor_mgr.adc_buffer,
            .buffer_size = sizeof(sensor_mgr.adc_buffer),
            .resolution = 12,
        };
    }
    
    /* Register sensors with power manager */
    if (sensor_mgr.temp_sensor) {
        power_manager_register_device(sensor_mgr.temp_sensor);
    }
    if (sensor_mgr.adc_dev) {
        power_manager_register_device(sensor_mgr.adc_dev);
    }
    
    sensor_mgr.initialized = true;
    LOG_INF("Sensor manager initialized");
    
    return 0;
}

int sensor_manager_read(struct sensor_data *data)
{
    if (!data) {
        return -EINVAL;
    }
    
    if (!sensor_mgr.initialized) {
        return -ENODEV;
    }
    
    /* Get device for measurement */
    if (sensor_mgr.temp_sensor) {
        int ret = pm_device_runtime_get(sensor_mgr.temp_sensor);
        if (ret < 0) {
            LOG_ERR("Failed to get temperature sensor: %d", ret);
            return ret;
        }
    }
    
    /* Read temperature and humidity */
    if (sensor_mgr.temp_sensor) {
        struct sensor_value temp_val, humidity_val;
        
        int ret = sensor_sample_fetch(sensor_mgr.temp_sensor);
        if (ret == 0) {
            ret = sensor_channel_get(sensor_mgr.temp_sensor,
                                   SENSOR_CHAN_AMBIENT_TEMP,
                                   &temp_val);
            if (ret == 0) {
                data->temperature_c = temp_val.val1 + temp_val.val2 / 1000000.0;
            }
            
            ret = sensor_channel_get(sensor_mgr.temp_sensor,
                                   SENSOR_CHAN_HUMIDITY,
                                   &humidity_val);
            if (ret == 0) {
                data->humidity_percent = humidity_val.val1 + humidity_val.val2 / 1000000.0;
            }
        }
        
        /* Release device after measurement */
        pm_device_runtime_put_async(sensor_mgr.temp_sensor);
        
        if (ret < 0) {
            LOG_ERR("Failed to read sensor: %d", ret);
            return ret;
        }
    } else {
        /* Simulate sensor readings */
        static float base_temp = 22.5f;
        static float base_humidity = 45.0f;
        
        /* Add some variation */
        data->temperature_c = base_temp + ((float)(sys_rand32_get() % 100) - 50) / 100.0f;
        data->humidity_percent = base_humidity + ((float)(sys_rand32_get() % 200) - 100) / 10.0f;
        
        /* Clamp values */
        if (data->humidity_percent < 0) data->humidity_percent = 0;
        if (data->humidity_percent > 100) data->humidity_percent = 100;
    }
    
    /* Set metadata */
    data->timestamp = k_uptime_get();
    data->sequence_number = ++sensor_mgr.sequence_number;
    
    LOG_DBG("Sensor reading #%u: %.2f°C, %.1f%%",
           data->sequence_number,
           data->temperature_c,
           data->humidity_percent);
    
    return 0;
}

uint32_t sensor_manager_read_battery(void)
{
    if (!sensor_mgr.adc_dev) {
        /* Simulate battery voltage */
        static uint32_t battery_mv = 3800;
        
        /* Slowly decrease battery level */
        if (sys_rand32_get() % 100 == 0) {
            battery_mv = MAX(battery_mv - 1, 3000);
        }
        
        return battery_mv;
    }
    
    /* Get ADC device for measurement */
    int ret = pm_device_runtime_get(sensor_mgr.adc_dev);
    if (ret < 0) {
        LOG_ERR("Failed to get ADC: %d", ret);
        return 0;
    }
    
    /* Read ADC */
    ret = adc_read(sensor_mgr.adc_dev, &sensor_mgr.adc_seq);
    
    /* Release ADC device */
    pm_device_runtime_put_async(sensor_mgr.adc_dev);
    
    if (ret < 0) {
        LOG_ERR("Failed to read ADC: %d", ret);
        return 0;
    }
    
    /* Convert ADC reading to voltage (implementation depends on hardware) */
    uint32_t battery_mv = (sensor_mgr.adc_buffer * 3300) / 4095; /* Assuming 3.3V reference */
    battery_mv = (battery_mv * 2); /* Assuming voltage divider */
    
    LOG_DBG("Battery voltage: %u mV", battery_mv);
    
    return battery_mv;
}

int sensor_manager_sleep(void)
{
    if (!sensor_mgr.initialized) {
        return -ENODEV;
    }
    
    /* Suspend sensors */
    if (sensor_mgr.temp_sensor) {
        pm_device_action_run(sensor_mgr.temp_sensor, PM_DEVICE_ACTION_SUSPEND);
    }
    
    LOG_DBG("Sensors put to sleep");
    return 0;
}

int sensor_manager_wakeup(void)
{
    if (!sensor_mgr.initialized) {
        return -ENODEV;
    }
    
    /* Resume sensors */
    if (sensor_mgr.temp_sensor) {
        pm_device_action_run(sensor_mgr.temp_sensor, PM_DEVICE_ACTION_RESUME);
    }
    
    LOG_DBG("Sensors woken up");
    return 0;
}
```

## Part 4: Device Tree Configuration

### 4.1 Power States Configuration

Create `overlays/power_states.overlay`:
```dts
/ {
    cpus {
        power-states {
            idle: idle {
                compatible = "zephyr,power-state";
                power-state-name = "runtime-idle";
                min-residency-us = <100>;
                exit-latency-us = <10>;
            };
            
            suspend_idle: suspend-to-idle {
                compatible = "zephyr,power-state";
                power-state-name = "suspend-to-idle";
                min-residency-us = <5000>;
                exit-latency-us = <100>;
            };
            
            suspend_mem: suspend-to-ram {
                compatible = "zephyr,power-state";
                power-state-name = "suspend-to-ram";
                min-residency-us = <50000>;
                exit-latency-us = <1000>;
            };
        };
        
        cpu@0 {
            cpu-power-states = <&idle &suspend_idle &suspend_mem>;
        };
    };
    
    /* Virtual temperature/humidity sensor for simulation */
    temp_sensor: temp-sensor {
        compatible = "zephyr,dummy-sensor";
        label = "TEMP_HUMIDITY_SENSOR";
        friendly-name = "Temperature and Humidity Sensor";
        minimal-interval = <1000>; /* 1 second minimum interval */
    };
    
    /* Power domains */
    power-domains {
        sensor_pd: sensor-power-domain {
            compatible = "power-domain";
            #power-domain-cells = <0>;
        };
        
        comm_pd: communication-power-domain {
            compatible = "power-domain";
            #power-domain-cells = <0>;
        };
    };
    
    aliases {
        temp-sensor = &temp_sensor;
    };
};

/* Configure sensor power domain */
&temp_sensor {
    power-domains = <&sensor_pd>;
};

/* Configure ADC for battery monitoring */
&adc {
    #address-cells = <1>;
    #size-cells = <0>;
    
    channel@0 {
        reg = <0>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };
};

/* Configure button as wake-up source */
&gpio0 {
    wakeup-gpios = <&gpio0 11 GPIO_ACTIVE_LOW>; /* Button pin */
};
```

### 4.2 Board-specific Configuration

Create `boards/nrf52840dk_nrf52840.overlay`:
```dts
/* nRF52840 Development Kit specific configuration */

/ {
    chosen {
        zephyr,console = &uart0;
        zephyr,shell-uart = &uart0;
        zephyr,uart-mcumgr = &uart0;
    };
    
    /* LED indicators for power states */
    leds {
        compatible = "gpio-leds";
        
        power_led: led_0 {
            gpios = <&gpio0 13 GPIO_ACTIVE_LOW>;
            label = "Power State LED";
        };
        
        activity_led: led_1 {
            gpios = <&gpio0 14 GPIO_ACTIVE_LOW>;
            label = "Activity LED";
        };
    };
    
    /* Buttons for wake-up */
    buttons {
        compatible = "gpio-keys";
        
        wakeup_button: button_0 {
            gpios = <&gpio0 11 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
            label = "Wake-up Button";
        };
    };
    
    aliases {
        led-power = &power_led;
        led-activity = &activity_led;
        sw-wakeup = &wakeup_button;
    };
};

/* UART configuration for lower power */
&uart0 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart0_default>;
    pinctrl-1 = <&uart0_sleep>;
    pinctrl-names = "default", "sleep";
};

/* I2C for sensors */
&i2c0 {
    compatible = "nordic,nrf-twi";
    status = "okay";
    pinctrl-0 = <&i2c0_default>;
    pinctrl-1 = <&i2c0_sleep>;
    pinctrl-names = "default", "sleep";
    
    /* Example sensor (if available) */
    sht3xd@44 {
        compatible = "sensirion,sht3xd";
        reg = <0x44>;
        label = "SHT3XD";
    };
};

/* ADC configuration */
&adc {
    status = "okay";
};

/* Enable Bluetooth */
&radio {
    status = "okay";
};

/* Power management pin control states */
&pinctrl {
    uart0_default: uart0_default {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, 6)>,
                    <NRF_PSEL(UART_RX, 0, 8)>;
        };
    };
    
    uart0_sleep: uart0_sleep {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, 6)>,
                    <NRF_PSEL(UART_RX, 0, 8)>;
            low-power-enable;
        };
    };
    
    i2c0_default: i2c0_default {
        group1 {
            psels = <NRF_PSEL(TWIM_SDA, 0, 26)>,
                    <NRF_PSEL(TWIM_SCL, 0, 27)>;
        };
    };
    
    i2c0_sleep: i2c0_sleep {
        group1 {
            psels = <NRF_PSEL(TWIM_SDA, 0, 26)>,
                    <NRF_PSEL(TWIM_SCL, 0, 27)>;
            low-power-enable;
        };
    };
};
```

## Part 5: Configuration and Build Files

### 5.1 Main Configuration

Create `prj.conf`:
```kconfig
# Kernel Configuration
CONFIG_MAIN_THREAD_PRIORITY=7
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
CONFIG_IDLE_STACK_SIZE=1024

# Power Management
CONFIG_PM=y
CONFIG_PM_DEVICE=y
CONFIG_PM_DEVICE_RUNTIME=y
CONFIG_PM_POLICY_CUSTOM=y
CONFIG_PM_S2RAM=y

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_LOG_MODE_DEFERRED=y
CONFIG_LOG_BUFFER_SIZE=4096

# Console and Shell
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y

# GPIO
CONFIG_GPIO=y

# ADC
CONFIG_ADC=y

# Sensor
CONFIG_SENSOR=y

# Bluetooth
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="EnvMonitor"
CONFIG_BT_DEVICE_APPEARANCE=768
CONFIG_BT_MAX_CONN=1
CONFIG_BT_MAX_PAIRED=1

# Bluetooth Low Energy
CONFIG_BT_GATT_DM=y
CONFIG_BT_GATT_CLIENT=y

# Thread synchronization
CONFIG_EVENTS=y

# System calls
CONFIG_HEAP_MEM_POOL_SIZE=8192

# Debugging
CONFIG_DEBUG=y
CONFIG_DEBUG_OPTIMIZATIONS=y
CONFIG_STACK_SENTINEL=y

# Performance
CONFIG_NO_OPTIMIZATIONS=n
CONFIG_SIZE_OPTIMIZATIONS=y

# Power optimization
CONFIG_TICKLESS_KERNEL=y
CONFIG_SYS_POWER_MANAGEMENT=y
```

### 5.2 CMakeLists.txt

Create `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(power_management_lab)

# Source files
target_sources(app PRIVATE
    src/main.c
    src/power_manager.c
    src/sensor_manager.c
    src/bluetooth_manager.c
)

# Include directories
target_include_directories(app PRIVATE include)

# Board-specific overlays


# Power states overlay
set(DTC_OVERLAY_FILE "overlays/power_states.overlay")

# Add board overlay if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.overlay")
    list(APPEND DTC_OVERLAY_FILE "boards/${BOARD}.overlay")
endif()
```

### 5.3 West Manifest (Optional)

Create `west.yml` for dependencies:
```yaml
manifest:
  version: "0.13"

  defaults:
    remote: upstream

  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos

  projects:
    - name: zephyr
      remote: upstream
      revision: main
      import:
        # Import Zephyr's west configuration
        file: west.yml

  self:
    path: power-management-lab
```

## Part 6: Testing and Verification

### 6.1 Build the Project

```bash
# Navigate to project directory
cd ~/zephyr-workspace/power_management_lab

# Build for nRF52840 Development Kit
west build -b nrf52840dk_nrf52840

# Flash to device
west flash

# Monitor output
west build -t menuconfig  # Optional: configure settings
```

### 6.2 Expected Output

When running the application, you should see:
```
*** Booting Zephyr OS build v3.x.x ***
[00:00:00.000,000] <inf> main: Environmental Monitoring Station Starting
[00:00:00.001,000] <inf> main: Build time: Dec 17 2024 10:30:45
[00:00:00.005,000] <inf> power_manager: Initializing power management system
[00:00:00.010,000] <inf> power_manager: Power management initialized
[00:00:00.015,000] <inf> sensor_manager: Initializing sensor manager
[00:00:00.020,000] <inf> sensor_manager: Sensor manager initialized
[00:00:00.025,000] <inf> main: Wake-up sources initialized
[00:00:00.030,000] <inf> main: Sensor thread started
[00:00:00.035,000] <inf> main: Bluetooth thread started
[00:00:00.040,000] <inf> main: Power management thread started
[00:00:00.045,000] <inf> main: System initialized successfully
[00:00:00.050,000] <inf> power_manager: Switching power mode: 0 -> 2
[00:00:00.055,000] <inf> power_manager: Power mode changed: 0 -> 2 (CPU: 48000000 Hz)
[00:00:30.000,000] <inf> main: Measurement 1: Temp=22.73°C, Humidity=44.2%
[00:00:30.005,000] <dbg> sensor_manager: Sensor reading #1: 22.73°C, 44.2%
[00:00:30.010,000] <dbg> sensor_manager: Battery voltage: 3756 mV
[00:00:40.000,000] <inf> power_manager: === Power Management Statistics ===
[00:00:40.005,000] <inf> power_manager: Current mode: 2
[00:00:40.010,000] <inf> power_manager: CPU frequency: 48000000 Hz
[00:00:40.015,000] <inf> power_manager: Active time: 40050000 us
[00:00:40.020,000] <inf> power_manager: Sleep count: 15
[00:00:40.025,000] <inf> power_manager: Wake-up count: 15
[00:00:40.030,000] <inf> power_manager: Deep sleep count: 0
[00:00:40.035,000] <inf> power_manager: Managed devices: 2
```

### 6.3 Testing Power Management Features

#### Test 1: Basic Power States
```bash
# Monitor power consumption with multimeter or power profiler
# Observe different current levels in different power modes

# Test wake-up from button
# Press button while system is in deep sleep mode
# Verify system wakes up and resumes operation
```

#### Test 2: Bluetooth Connection Impact
```bash
# Use nRF Connect mobile app to connect to the device
# Observe power mode change to ACTIVE when connected
# Monitor measurement frequency increase
# Disconnect and verify return to ECO mode
```

#### Test 3: Low Battery Behavior
```bash
# Simulate low battery by modifying threshold or ADC reading
# Verify system enters ULTRA_LOW power mode
# Confirm measurement interval increases
# Check that non-essential features are disabled
```

### 6.4 Power Consumption Analysis

Create a simple monitoring script `scripts/power_monitor.py`:
```python
#!/usr/bin/env python3

import serial
import time
import re
import matplotlib.pyplot as plt
from collections import defaultdict

class PowerMonitor:
    def __init__(self, port='/dev/ttyACM0', baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=1)
        self.power_modes = []
        self.timestamps = []
        self.measurements = []
        
    def monitor(self, duration=300):  # 5 minutes
        start_time = time.time()
        
        while time.time() - start_time < duration:
            line = self.ser.readline().decode('utf-8').strip()
            
            if 'Power mode changed' in line:
                match = re.search(r'Power mode changed: (\d+) -> (\d+)', line)
                if match:
                    old_mode, new_mode = map(int, match.groups())
                    timestamp = time.time() - start_time
                    self.power_modes.append((timestamp, new_mode))
                    print(f"[{timestamp:.1f}s] Power mode: {old_mode} -> {new_mode}")
            
            elif 'Measurement' in line and 'Temp=' in line:
                match = re.search(r'Measurement (\d+): Temp=([\d.]+)°C, Humidity=([\d.]+)%', line)
                if match:
                    seq, temp, humidity = match.groups()
                    timestamp = time.time() - start_time
                    self.measurements.append((timestamp, float(temp), float(humidity)))
                    print(f"[{timestamp:.1f}s] T={temp}°C, H={humidity}%")
    
    def plot_results(self):
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
        
        # Plot power modes
        if self.power_modes:
            times, modes = zip(*self.power_modes)
            ax1.step(times, modes, where='post', linewidth=2)
            ax1.set_ylabel('Power Mode')
            ax1.set_title('Power Mode Changes Over Time')
            ax1.grid(True)
        
        # Plot measurements
        if self.measurements:
            times, temps, humidity = zip(*self.measurements)
            ax2.plot(times, temps, 'r-o', label='Temperature (°C)', markersize=3)
            ax2_twin = ax2.twinx()
            ax2_twin.plot(times, humidity, 'b-s', label='Humidity (%)', markersize=3)
            ax2.set_xlabel('Time (seconds)')
            ax2.set_ylabel('Temperature (°C)', color='r')
            ax2_twin.set_ylabel('Humidity (%)', color='b')
            ax2.set_title('Sensor Measurements')
            ax2.grid(True)
        
        plt.tight_layout()
        plt.savefig('power_analysis.png')
        plt.show()

if __name__ == "__main__":
    monitor = PowerMonitor()
    print("Starting power monitoring for 5 minutes...")
    monitor.monitor(300)
    monitor.plot_results()
    print("Analysis complete. Results saved to power_analysis.png")
```

## Part 7: Advanced Exercises

### Exercise 1: Adaptive Power Management

Implement adaptive power management that adjusts based on activity:

```c
/* Add to power_manager.c */
static void adaptive_power_management(void)
{
    static uint32_t last_measurement_time = 0;
    static uint32_t last_bluetooth_activity = 0;
    uint32_t current_time = k_uptime_get_32();
    
    /* Check activity patterns */
    bool recent_measurements = (current_time - last_measurement_time) < 60000;
    bool recent_bluetooth = (current_time - last_bluetooth_activity) < 30000;
    
    /* Adaptive mode selection */
    if (recent_bluetooth) {
        power_manager_set_performance_mode(POWER_MODE_ACTIVE);
    } else if (recent_measurements) {
        power_manager_set_performance_mode(POWER_MODE_BALANCED);
    } else {
        power_manager_set_performance_mode(POWER_MODE_ECO);
    }
}
```

### Exercise 2: Power Budget Management

Create a power budget system:

```c
struct power_budget {
    uint32_t total_budget_mw;
    uint32_t current_usage_mw;
    uint32_t sensor_allocation_mw;
    uint32_t bluetooth_allocation_mw;
    uint32_t cpu_allocation_mw;
};

static int manage_power_budget(struct power_budget *budget)
{
    /* Monitor and enforce power budget */
    if (budget->current_usage_mw > budget->total_budget_mw) {
        /* Reduce power consumption */
        return reduce_power_consumption(budget);
    }
    
    return 0;
}
```

### Exercise 3: Wake-up Optimization

Implement intelligent wake-up scheduling:

```c
static k_timeout_t calculate_optimal_wakeup_interval(void)
{
    /* Consider multiple factors */
    uint32_t base_interval = MEASUREMENT_INTERVAL_MS;
    
    /* Adjust based on battery level */
    if (sys_state.battery_voltage_mv < LOW_BATTERY_THRESHOLD_MV) {
        base_interval *= 4;  /* Reduce frequency when battery is low */
    }
    
    /* Adjust based on connectivity */
    if (sys_state.ble_connected) {
        base_interval /= 6;  /* Increase frequency when connected */
    }
    
    /* Adjust based on environmental conditions */
    if (detect_rapid_changes()) {
        base_interval /= 2;  /* Increase sampling during rapid changes */
    }
    
    return K_MSEC(base_interval);
}
```

## Summary

This comprehensive lab demonstrates professional-grade power management implementation in Zephyr RTOS. You've implemented:

1. **System-Level Power Management**: Multiple power states with intelligent policy
2. **Device Runtime PM**: Automatic device power control based on usage
3. **Power-Aware Application Design**: Efficient resource usage patterns
4. **Adaptive Power Control**: Dynamic adjustment based on system state
5. **Wake-up Source Management**: Multiple wake-up mechanisms
6. **Power Monitoring**: Statistics and debugging capabilities

The project showcases real-world power management techniques essential for battery-powered IoT devices, providing a solid foundation for developing energy-efficient embedded systems.

**Key Takeaways:**
- Power management is critical for modern embedded systems
- Zephyr provides comprehensive power management APIs
- Device tree configuration is essential for power states
- Runtime power management enables automatic optimization
- Monitoring and debugging tools are essential for validation
- Adaptive algorithms can significantly improve battery life

Continue experimenting with different power modes, measurement intervals, and optimization strategies to fully understand power management principles and their impact on system performance.
