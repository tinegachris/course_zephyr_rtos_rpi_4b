# Chapter 17 - Power Management Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

## 17.1 Understanding Power Management in Embedded Systems

Power management is critical for modern embedded systems, especially battery-powered devices. Effective power management extends battery life, reduces heat generation, and improves system reliability.

### Why Power Management Matters

* **Battery Life**: IoT sensors, wearables, and mobile devices require extended operation without charging
* **Thermal Management**: Reduced power consumption prevents overheating and improves reliability
* **Cost Efficiency**: Lower power consumption reduces operational costs in large deployments
* **Environmental Impact**: Energy-efficient systems contribute to sustainability goals

### Power Management Challenges

* **Performance vs. Power Trade-offs**: Balancing responsiveness with power savings
* **Complex Wake-up Scenarios**: Managing multiple wake-up sources and conditions
* **Hardware Dependencies**: Different processors and peripherals have varying power capabilities
* **Software Coordination**: Synchronizing power states across multiple system components

## 17.2 Zephyr Power Management Architecture

Zephyr provides a comprehensive power management framework with multiple layers:

### System Power Management

System-level power management controls the overall system state:

```c
/* System power states */
enum pm_state {
    PM_STATE_ACTIVE,           /* System fully active */
    PM_STATE_RUNTIME_IDLE,     /* CPU idle, peripherals active */
    PM_STATE_SUSPEND_TO_IDLE,  /* CPU and most peripherals suspended */
    PM_STATE_SUSPEND_TO_RAM,   /* System state preserved in RAM */
    PM_STATE_SUSPEND_TO_DISK,  /* System state saved to persistent storage */
    PM_STATE_SOFT_OFF,         /* System off, but can be awakened */
};
```

### Device Power Management

Device-level power management controls individual peripherals:

```c
/* Device power states */
enum pm_device_state {
    PM_DEVICE_STATE_ACTIVE,      /* Device fully functional */
    PM_DEVICE_STATE_LOW_POWER,   /* Device in low power mode */
    PM_DEVICE_STATE_SUSPEND,     /* Device suspended */
    PM_DEVICE_STATE_OFF,         /* Device powered off */
};

/* Device power actions */
enum pm_device_action {
    PM_DEVICE_ACTION_SUSPEND,    /* Suspend device */
    PM_DEVICE_ACTION_RESUME,     /* Resume device */
    PM_DEVICE_ACTION_TURN_ON,    /* Power on device */
    PM_DEVICE_ACTION_TURN_OFF,   /* Power off device */
    PM_DEVICE_ACTION_LOW_POWER,  /* Enter low power mode */
};
```

## 17.3 Device Tree Power Configuration

### Power States Definition

Define available power states in device tree:

```dts
/ {
    cpus {
        power-states {
            state0: state0 {
                compatible = "zephyr,power-state";
                power-state-name = "suspend-to-idle";
                min-residency-us = <10000>;
                exit-latency-us = <100>;
            };
            
            state1: state1 {
                compatible = "zephyr,power-state";
                power-state-name = "suspend-to-ram";
                min-residency-us = <100000>;
                exit-latency-us = <1000>;
            };
        };
        
        cpu@0 {
            device_type = "cpu";
            compatible = "arm,cortex-m4";
            cpu-power-states = <&state0 &state1>;
        };
    };
};
```

### Device Power Domains

Configure power domains for peripheral control:

```dts
/ {
    power-domains {
        pd_sensor: power-domain-sensor {
            compatible = "power-domain";
            #power-domain-cells = <0>;
        };
        
        pd_communication: power-domain-comm {
            compatible = "power-domain";
            #power-domain-cells = <0>;
        };
    };
    
    soc {
        sensor@40001000 {
            compatible = "vendor,sensor";
            reg = <0x40001000 0x1000>;
            power-domains = <&pd_sensor>;
        };
        
        uart@40002000 {
            compatible = "vendor,uart";
            reg = <0x40002000 0x1000>;
            power-domains = <&pd_communication>;
        };
    };
};
```

## 17.4 Runtime Power Management

### Device Runtime PM API

```c
#include <zephyr/pm/device.h>

/**
 * @brief Enable runtime power management for device
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int pm_device_runtime_enable(const struct device *dev);

/**
 * @brief Disable runtime power management for device
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int pm_device_runtime_disable(const struct device *dev);

/**
 * @brief Get device for use (increment reference count)
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int pm_device_runtime_get(const struct device *dev);

/**
 * @brief Put device after use (decrement reference count)
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int pm_device_runtime_put(const struct device *dev);

/**
 * @brief Put device asynchronously
 * 
 * @param dev Device instance
 * @return 0 on success, negative errno on failure
 */
int pm_device_runtime_put_async(const struct device *dev);
```

### Device PM Implementation Pattern

```c
#include <zephyr/pm/device.h>

struct my_device_data {
    /* Device runtime data */
    uint32_t usage_count;
    bool suspended;
    
    /* Power management */
    struct k_work_delayable pm_work;
    k_timeout_t suspend_delay;
};

/* Power management action handler */
static int my_device_pm_action(const struct device *dev,
                               enum pm_device_action action)
{
    struct my_device_data *data = dev->data;
    int ret = 0;
    
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        if (!data->suspended) {
            /* Save device context */
            ret = my_device_save_context(dev);
            if (ret == 0) {
                /* Power down hardware */
                my_device_power_down(dev);
                data->suspended = true;
                LOG_DBG("Device suspended");
            }
        }
        break;
        
    case PM_DEVICE_ACTION_RESUME:
        if (data->suspended) {
            /* Power up hardware */
            ret = my_device_power_up(dev);
            if (ret == 0) {
                /* Restore device context */
                ret = my_device_restore_context(dev);
                data->suspended = false;
                LOG_DBG("Device resumed");
            }
        }
        break;
        
    case PM_DEVICE_ACTION_TURN_OFF:
        /* Complete power off */
        my_device_power_off(dev);
        data->suspended = true;
        break;
        
    case PM_DEVICE_ACTION_TURN_ON:
        /* Complete power on */
        ret = my_device_power_on(dev);
        data->suspended = false;
        break;
        
    default:
        ret = -ENOTSUP;
    }
    
    return ret;
}

/* Device usage tracking */
static int my_device_get(const struct device *dev)
{
    struct my_device_data *data = dev->data;
    int ret;
    
    /* Get device for runtime PM */
    ret = pm_device_runtime_get(dev);
    if (ret < 0) {
        return ret;
    }
    
    /* Cancel any pending suspend operation */
    k_work_cancel_delayable(&data->pm_work);
    
    return 0;
}

static int my_device_put(const struct device *dev)
{
    struct my_device_data *data = dev->data;
    
    /* Schedule delayed suspend */
    k_work_schedule(&data->pm_work, data->suspend_delay);
    
    /* Put device for runtime PM */
    return pm_device_runtime_put_async(dev);
}

/* PM work handler */
static void my_device_pm_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct my_device_data *data = 
        CONTAINER_OF(dwork, struct my_device_data, pm_work);
    const struct device *dev = data->dev;
    
    /* Check if device is still unused */
    if (pm_device_runtime_usage(dev) == 0) {
        /* Suspend device */
        pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
    }
}
```

## 17.5 System Power Management

### System PM Policy

```c
#include <zephyr/pm/policy.h>

/* Custom power policy */
const struct pm_state_info *pm_policy_next_state(uint8_t cpu, int32_t ticks)
{
    static const struct pm_state_info states[] = {
        {PM_STATE_RUNTIME_IDLE, 0, 0},
        {PM_STATE_SUSPEND_TO_IDLE, 0, 10000},  /* min 10ms residency */
        {PM_STATE_SUSPEND_TO_RAM, 0, 100000},  /* min 100ms residency */
    };
    
    /* Convert ticks to microseconds */
    int32_t residency_us = k_ticks_to_us_floor32(ticks);
    
    /* Select appropriate state based on expected idle time */
    for (int i = ARRAY_SIZE(states) - 1; i >= 0; i--) {
        if (residency_us >= states[i].min_residency_us) {
            return &states[i];
        }
    }
    
    return NULL; /* Stay active */
}
```

### Wake-up Sources

```c
#include <zephyr/pm/device.h>

/* Configure wake-up sources */
static int configure_wakeup_sources(void)
{
    const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rtc));
    
    /* Configure GPIO wake-up */
    pm_device_wakeup_enable(gpio_dev, true);
    
    /* Configure RTC wake-up */
    pm_device_wakeup_enable(rtc_dev, true);
    
    return 0;
}

/* Handle wake-up events */
static void handle_wakeup_event(const struct device *dev, 
                               enum pm_device_wakeup_reason reason)
{
    switch (reason) {
    case PM_DEVICE_WAKEUP_SOURCE_GPIO:
        LOG_INF("Woken up by GPIO");
        /* Handle GPIO wake-up */
        break;
        
    case PM_DEVICE_WAKEUP_SOURCE_TIMER:
        LOG_INF("Woken up by timer");
        /* Handle timer wake-up */
        break;
        
    default:
        LOG_INF("Unknown wake-up source");
        break;
    }
}
```

## 17.6 Advanced Power Management Features

### Power Domains

```c
#include <zephyr/pm/domain.h>

/* Power domain control */
struct power_domain {
    const struct device *regulator;
    bool enabled;
    uint32_t ref_count;
};

static int power_domain_on(struct power_domain *pd)
{
    if (pd->ref_count == 0) {
        /* Enable power supply */
        int ret = regulator_enable(pd->regulator);
        if (ret < 0) {
            return ret;
        }
        pd->enabled = true;
    }
    
    pd->ref_count++;
    return 0;
}

static int power_domain_off(struct power_domain *pd)
{
    if (pd->ref_count > 0) {
        pd->ref_count--;
        
        if (pd->ref_count == 0) {
            /* Disable power supply */
            regulator_disable(pd->regulator);
            pd->enabled = false;
        }
    }
    
    return 0;
}
```

### Dynamic Voltage and Frequency Scaling (DVFS)

```c
#include <zephyr/pm/cpu.h>

/* CPU frequency scaling */
static int scale_cpu_frequency(uint32_t target_freq_hz)
{
    const struct device *cpu_dev = DEVICE_DT_GET(DT_NODELABEL(cpu0));
    struct pm_cpu_info info;
    int ret;
    
    /* Get current CPU info */
    ret = pm_cpu_info_get(cpu_dev, &info);
    if (ret < 0) {
        return ret;
    }
    
    /* Scale frequency */
    if (target_freq_hz != info.current_frequency) {
        ret = pm_cpu_frequency_set(cpu_dev, target_freq_hz);
        if (ret < 0) {
            LOG_ERR("Failed to set CPU frequency: %d", ret);
            return ret;
        }
        
        LOG_INF("CPU frequency scaled from %u to %u Hz",
                info.current_frequency, target_freq_hz);
    }
    
    return 0;
}

/* Adaptive frequency scaling based on workload */
static void adaptive_frequency_scaling(void)
{
    static uint32_t last_idle_time = 0;
    uint32_t current_idle_time = k_thread_runtime_stats_get_idle();
    uint32_t idle_percentage;
    
    /* Calculate idle percentage */
    if (current_idle_time > last_idle_time) {
        uint32_t total_time = k_uptime_get_32();
        idle_percentage = ((current_idle_time - last_idle_time) * 100) / 
                         (total_time);
    } else {
        idle_percentage = 0;
    }
    
    /* Adjust frequency based on idle time */
    if (idle_percentage > 80) {
        /* System mostly idle - reduce frequency */
        scale_cpu_frequency(48000000); /* 48 MHz */
    } else if (idle_percentage < 20) {
        /* System busy - increase frequency */
        scale_cpu_frequency(168000000); /* 168 MHz */
    } else {
        /* Moderate load - balanced frequency */
        scale_cpu_frequency(84000000); /* 84 MHz */
    }
    
    last_idle_time = current_idle_time;
}
```

## 17.7 Power Management Best Practices

### Design Guidelines

1. **Minimize Active Time**: Complete tasks quickly and return to idle
2. **Use Appropriate Power States**: Match power state to expected idle duration
3. **Optimize Wake-up Latency**: Balance power savings with responsiveness requirements
4. **Coordinate Device Dependencies**: Manage power domains and device relationships
5. **Monitor Power Consumption**: Use power statistics to validate optimizations

### Implementation Patterns

```c
/* Efficient power-aware operation */
static int power_aware_operation(const struct device *dev)
{
    int ret;
    
    /* Get device for use */
    ret = pm_device_runtime_get(dev);
    if (ret < 0) {
        return ret;
    }
    
    /* Perform operation efficiently */
    ret = perform_device_operation(dev);
    
    /* Release device immediately after use */
    pm_device_runtime_put_async(dev);
    
    return ret;
}

/* Batch operations for efficiency */
static int batch_operations(const struct device *dev,
                           const void **data, size_t count)
{
    int ret;
    
    /* Get device once for multiple operations */
    ret = pm_device_runtime_get(dev);
    if (ret < 0) {
        return ret;
    }
    
    /* Perform all operations while device is active */
    for (size_t i = 0; i < count; i++) {
        ret = perform_single_operation(dev, data[i]);
        if (ret < 0) {
            break;
        }
    }
    
    /* Release device after batch completion */
    pm_device_runtime_put(dev);
    
    return ret;
}
```

### Debugging and Monitoring

```c
/* Power management statistics */
static void print_power_stats(void)
{
    struct pm_stats stats;
    
    pm_stats_get(&stats);
    
    LOG_INF("Power Management Statistics:");
    LOG_INF("  Active time: %u ms", stats.active_time_ms);
    LOG_INF("  Idle time: %u ms", stats.idle_time_ms);
    LOG_INF("  Suspend count: %u", stats.suspend_count);
    LOG_INF("  Resume count: %u", stats.resume_count);
    LOG_INF("  Average suspend time: %u ms", stats.avg_suspend_time_ms);
}

/* Device power state monitoring */
static void monitor_device_power_states(void)
{
    const struct device *dev;
    enum pm_device_state state;
    
    /* Iterate through all devices */
    STRUCT_SECTION_FOREACH(device, dev) {
        if (pm_device_state_get(dev, &state) == 0) {
            LOG_INF("Device %s: state = %d", dev->name, state);
        }
    }
}
```

This comprehensive theory foundation provides the knowledge needed to implement effective power management in Zephyr applications.

[Next: Power Management Lab](./lab.md)
