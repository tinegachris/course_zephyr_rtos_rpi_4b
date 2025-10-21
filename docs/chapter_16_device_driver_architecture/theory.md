# Chapter 16 - Device Driver Architecture Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

## 16.1 Understanding Zephyr Device Driver Model

The Zephyr device driver model provides a standardized framework for interfacing with hardware peripherals. This architecture ensures consistency, maintainability, and portability across different hardware platforms.

### Core Components

Every Zephyr device driver consists of four fundamental components:

1. **Device Structure**: Contains device metadata and runtime information
2. **Configuration Data**: Static configuration parameters (ROM)
3. **Runtime Data**: Dynamic state information (RAM)
4. **API Structure**: Function pointers for device operations

### The Device Structure

```c
struct device {
    const char *name;           /* Device name */
    const void *config;         /* Configuration data (ROM) */
    const void *api;           /* API function pointers */
    void * const data;         /* Runtime data (RAM) */
    pm_device_t *pm;           /* Power management */
    uint8_t state;             /* Device state */
};
```

## 16.2 Device Definition and Registration

### The DEVICE_DEFINE Macro

The `DEVICE_DEFINE` macro is the primary mechanism for registering devices:

```c
DEVICE_DEFINE(dev_name, drv_name,
              init_fn, pm_fn,
              data_ptr, config_ptr,
              level, prio, api_ptr);
```

**Parameters Explained:**
* `dev_name`: Unique device identifier
* `drv_name`: Human-readable driver name
* `init_fn`: Initialization function pointer
* `pm_fn`: Power management function pointer
* `data_ptr`: Pointer to runtime data structure
* `config_ptr`: Pointer to configuration data structure
* `level`: Initialization level (PRE_KERNEL_1, PRE_KERNEL_2, POST_KERNEL, APPLICATION)
* `prio`: Priority within the initialization level
* `api_ptr`: Pointer to API structure

### Modern Device Tree Integration

For device tree-based drivers, use `DEVICE_DT_INST_DEFINE`:

```c
DEVICE_DT_INST_DEFINE(inst, init_fn, pm_fn,
                      data_ptr, config_ptr,
                      level, prio, api_ptr);
```

## 16.3 Device Tree Integration

### Device Tree Bindings

Device tree bindings define the interface between hardware description and driver code. They are specified in YAML files:

```yaml
# dts/bindings/vendor,device-name.yaml
description: Custom device driver binding

compatible: "vendor,device-name"

properties:
  reg:
    type: array
    description: Register space
    required: true

  interrupts:
    type: array
    description: Interrupt configuration
    required: false

  clock-frequency:
    type: int
    description: Device clock frequency in Hz
    required: false

  vendor,custom-property:
    type: int
    description: Vendor-specific configuration
    required: false
```

### Device Tree Node

```dts
/ {
    soc {
        my_device: my_device@40001000 {
            compatible = "vendor,device-name";
            reg = <0x40001000 0x1000>;
            interrupts = <15 0>;
            clock-frequency = <24000000>;
            vendor,custom-property = <42>;
            status = "okay";
        };
    };
};
```

## 16.4 Complete Driver Implementation Pattern

### Configuration Structure

```c
struct my_device_config {
    /* Device tree derived configuration */
    uintptr_t base_addr;
    uint32_t clock_freq;
    uint32_t custom_property;
    
    /* Interrupt configuration */
    void (*irq_config_func)(void);
    uint32_t irq_num;
    uint32_t irq_priority;
};
```

### Runtime Data Structure

```c
struct my_device_data {
    /* Runtime state */
    bool initialized;
    bool enabled;
    uint32_t error_count;
    
    /* Synchronization */
    struct k_mutex lock;
    struct k_sem ready_sem;
    
    /* Callback support */
    my_device_callback_t callback;
    void *user_data;
};
```

### API Structure

```c
struct my_device_api {
    int (*configure)(const struct device *dev, uint32_t config);
    int (*enable)(const struct device *dev);
    int (*disable)(const struct device *dev);
    int (*read)(const struct device *dev, void *buf, size_t len);
    int (*write)(const struct device *dev, const void *buf, size_t len);
    int (*ioctl)(const struct device *dev, uint32_t cmd, void *arg);
};
```

## 16.5 Driver Implementation

### Initialization Function

```c
static int my_device_init(const struct device *dev)
{
    const struct my_device_config *config = dev->config;
    struct my_device_data *data = dev->data;
    int ret;
    
    /* Validate configuration */
    if (!config || !data) {
        return -EINVAL;
    }
    
    /* Initialize synchronization primitives */
    k_mutex_init(&data->lock);
    k_sem_init(&data->ready_sem, 0, 1);
    
    /* Hardware initialization */
    ret = hardware_init(config->base_addr, config->clock_freq);
    if (ret < 0) {
        LOG_ERR("Hardware initialization failed: %d", ret);
        return ret;
    }
    
    /* Configure interrupts if needed */
    if (config->irq_config_func) {
        config->irq_config_func();
        irq_enable(config->irq_num);
    }
    
    /* Set initial state */
    data->initialized = true;
    data->error_count = 0;
    
    LOG_INF("Device %s initialized", dev->name);
    return 0;
}
```

### API Implementation

```c
static int my_device_configure(const struct device *dev, uint32_t config_flags)
{
    const struct my_device_config *config = dev->config;
    struct my_device_data *data = dev->data;
    int ret;
    
    if (!data->initialized) {
        return -ENODEV;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    /* Apply configuration */
    ret = apply_hardware_config(config->base_addr, config_flags);
    if (ret < 0) {
        data->error_count++;
    }
    
    k_mutex_unlock(&data->lock);
    return ret;
}

static int my_device_read(const struct device *dev, void *buf, size_t len)
{
    struct my_device_data *data = dev->data;
    const struct my_device_config *config = dev->config;
    int ret;
    
    if (!buf || len == 0) {
        return -EINVAL;
    }
    
    if (!data->initialized || !data->enabled) {
        return -ENODEV;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    /* Perform hardware read */
    ret = hardware_read(config->base_addr, buf, len);
    if (ret < 0) {
        data->error_count++;
    }
    
    k_mutex_unlock(&data->lock);
    return ret;
}
```

### Interrupt Handling

```c
static void my_device_isr(const struct device *dev)
{
    const struct my_device_config *config = dev->config;
    struct my_device_data *data = dev->data;
    uint32_t status;
    
    /* Read interrupt status */
    status = read_interrupt_status(config->base_addr);
    
    /* Handle different interrupt sources */
    if (status & IRQ_DATA_READY) {
        /* Signal data ready */
        k_sem_give(&data->ready_sem);
        
        /* Call user callback if registered */
        if (data->callback) {
            data->callback(dev, MY_DEVICE_EVENT_DATA_READY, data->user_data);
        }
    }
    
    if (status & IRQ_ERROR) {
        data->error_count++;
        LOG_WRN("Device error interrupt");
    }
    
    /* Clear interrupt status */
    clear_interrupt_status(config->base_addr, status);
}
```

## 16.6 Power Management Integration

### Power Management Actions

```c
#ifdef CONFIG_PM_DEVICE
static int my_device_pm_action(const struct device *dev,
                               enum pm_device_action action)
{
    const struct my_device_config *config = dev->config;
    struct my_device_data *data = dev->data;
    int ret = 0;
    
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        /* Save device state */
        ret = save_device_state(config->base_addr);
        if (ret == 0) {
            /* Power down hardware */
            power_down_hardware(config->base_addr);
            data->enabled = false;
        }
        break;
        
    case PM_DEVICE_ACTION_RESUME:
        /* Power up hardware */
        ret = power_up_hardware(config->base_addr);
        if (ret == 0) {
            /* Restore device state */
            ret = restore_device_state(config->base_addr);
            data->enabled = true;
        }
        break;
        
    case PM_DEVICE_ACTION_TURN_ON:
        ret = my_device_enable(dev);
        break;
        
    case PM_DEVICE_ACTION_TURN_OFF:
        ret = my_device_disable(dev);
        break;
        
    default:
        ret = -ENOTSUP;
        break;
    }
    
    return ret;
}
#endif /* CONFIG_PM_DEVICE */
```

## 16.7 Device Tree Macros and Helpers

### Extracting Configuration from Device Tree

```c
#define MY_DEVICE_INIT(inst)                                    \
    static void my_device_irq_config_##inst(void)              \
    {                                                           \
        IRQ_CONNECT(DT_INST_IRQN(inst),                        \
                    DT_INST_IRQ(inst, priority),               \
                    my_device_isr,                              \
                    DEVICE_DT_INST_GET(inst), 0);              \
        irq_enable(DT_INST_IRQN(inst));                        \
    }                                                           \
                                                                \
    static const struct my_device_config my_device_config_##inst = { \
        .base_addr = DT_INST_REG_ADDR(inst),                   \
        .clock_freq = DT_INST_PROP_OR(inst, clock_frequency, 0), \
        .custom_property = DT_INST_PROP_OR(inst, vendor_custom_property, 0), \
        .irq_config_func = my_device_irq_config_##inst,        \
        .irq_num = DT_INST_IRQN(inst),                         \
        .irq_priority = DT_INST_IRQ(inst, priority),           \
    };                                                          \
                                                                \
    static struct my_device_data my_device_data_##inst;        \
                                                                \
    DEVICE_DT_INST_DEFINE(inst, my_device_init,                \
                          IF_ENABLED(CONFIG_PM_DEVICE,         \
                                   (my_device_pm_action)),      \
                          &my_device_data_##inst,               \
                          &my_device_config_##inst,             \
                          POST_KERNEL,                          \
                          CONFIG_MY_DEVICE_INIT_PRIORITY,       \
                          &my_device_api);

/* Create device instances for all enabled nodes */
DT_INST_FOREACH_STATUS_OKAY(MY_DEVICE_INIT)
```

## 16.8 Best Practices

### Error Handling

```c
static int my_device_operation(const struct device *dev)
{
    int ret;
    
    /* Parameter validation */
    if (!dev || !dev->data) {
        return -EINVAL;
    }
    
    /* State validation */
    struct my_device_data *data = dev->data;
    if (!data->initialized) {
        return -ENODEV;
    }
    
    /* Operation with timeout */
    ret = k_sem_take(&data->ready_sem, K_MSEC(1000));
    if (ret != 0) {
        LOG_WRN("Operation timeout");
        return -ETIMEDOUT;
    }
    
    /* Hardware operation with error checking */
    ret = perform_hardware_operation();
    if (ret < 0) {
        LOG_ERR("Hardware operation failed: %d", ret);
        data->error_count++;
        return ret;
    }
    
    return 0;
}
```

### Thread Safety

```c
static int my_device_thread_safe_operation(const struct device *dev)
{
    struct my_device_data *data = dev->data;
    int ret;
    
    /* Acquire mutex with timeout */
    ret = k_mutex_lock(&data->lock, K_MSEC(100));
    if (ret != 0) {
        return -EBUSY;
    }
    
    /* Critical section */
    ret = perform_critical_operation();
    
    /* Always release mutex */
    k_mutex_unlock(&data->lock);
    
    return ret;
}
```

### Resource Management

```c
static int my_device_cleanup(const struct device *dev)
{
    struct my_device_data *data = dev->data;
    const struct my_device_config *config = dev->config;
    
    /* Disable interrupts */
    irq_disable(config->irq_num);
    
    /* Reset hardware */
    reset_hardware(config->base_addr);
    
    /* Free resources */
    if (data->buffer) {
        k_free(data->buffer);
        data->buffer = NULL;
    }
    
    /* Update state */
    data->initialized = false;
    data->enabled = false;
    
    return 0;
}
```

This theory foundation provides comprehensive knowledge for implementing professional device drivers in Zephyr RTOS.

[Next: Device Driver Lab](./lab.md)
