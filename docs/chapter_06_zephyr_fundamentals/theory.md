# Chapter 6: Zephyr Fundamentals - Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

This theory section provides comprehensive understanding of Zephyr's core subsystems and development practices. You'll master GPIO operations, I2C communication, device tree integration, preprocessor techniques, and shell development for professional embedded applications.

---

## GPIO Subsystem: Digital Hardware Control

### Understanding GPIO in Zephyr

General Purpose Input/Output (GPIO) pins provide the fundamental interface between your software and the digital world. Zephyr's GPIO subsystem abstracts hardware differences while providing powerful, type-safe APIs for controlling digital signals.

**Core GPIO Concepts:**

* **Device Tree Integration:** GPIO pins are described in device tree files, enabling automatic driver configuration and compile-time validation
* **Port-Based Organization:** GPIO pins are grouped into ports, with each port handled by a specific driver instance
* **Direction Configuration:** Pins can be configured as inputs (reading external signals) or outputs (controlling external devices)
* **Interrupt Capabilities:** Input pins can generate interrupts on signal changes, enabling responsive applications

### GPIO Device Tree Specification

Zephyr uses device tree specifications (`dt_spec`) to provide type-safe, hardware-independent GPIO access:

```c
#include <zephyr/drivers/gpio.h>

// Define GPIO specifications from device tree
#define LED_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED_NODE, gpios, {0});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(BUTTON_NODE, gpios, {0});
```

**Key Benefits of dt_spec:**
- **Compile-time validation:** Invalid GPIO configurations are caught during build
- **Hardware abstraction:** Same code works across different board configurations
- **Type safety:** Prevents common GPIO programming errors

### Essential GPIO Operations

**Device Readiness Check:**

```c
int gpio_setup(void)
{
    if (!gpio_is_ready_dt(&led)) {
        printk("Error: LED device %s is not ready\n", led.port->name);
        return -ENODEV;
    }
    
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: Button device %s is not ready\n", button.port->name);
        return -ENODEV;
    }
    
    return 0;
}
```

**Output Configuration and Control:**

```c
int led_init(void)
{
    int ret;
    
    // Configure LED pin as output, initially inactive
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure LED pin\n", ret);
        return ret;
    }
    
    return 0;
}

void led_control(bool state)
{
    gpio_pin_set_dt(&led, state);
}

void led_toggle(void)
{
    gpio_pin_toggle_dt(&led);
}
```

**Input Configuration and Reading:**

```c
int button_init(void)
{
    int ret;
    
    // Configure button pin as input with pull-up
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Error %d: failed to configure button pin\n", ret);
        return ret;
    }
    
    return 0;
}

bool button_is_pressed(void)
{
    int val = gpio_pin_get_dt(&button);
    // Assuming active-low button (pressed = 0)
    return (val == 0);
}
```

### GPIO Interrupt Handling

Interrupts enable responsive applications without continuous polling:

```c
static struct gpio_callback button_cb_data;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                   uint32_t pins)
{
    printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
    
    // Handle button press (e.g., toggle LED)
    led_toggle();
}

int button_interrupt_init(void)
{
    int ret;
    
    // Configure interrupt on falling edge (button press)
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure interrupt on button pin\n", ret);
        return ret;
    }
    
    // Initialize callback
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    
    // Add callback
    ret = gpio_add_callback_dt(&button, &button_cb_data);
    if (ret < 0) {
        printk("Error %d: failed to add callback\n", ret);
        return ret;
    }
    
    return 0;
}
```

## I2C Subsystem: Serial Communication

### I2C Communication Fundamentals

Inter-Integrated Circuit (I2C) provides a simple, two-wire interface for communicating with sensors, memory devices, and other peripherals. Zephyr's I2C subsystem handles protocol complexities while providing straightforward APIs.

**I2C Key Concepts:**

* **Controller/Target Architecture:** One controller manages communication with multiple target devices
* **Address-Based Communication:** Each target device has a unique 7-bit or 10-bit address
* **Transaction-Based Operations:** Data is transferred in discrete transactions with automatic bus management
* **Error Handling:** Built-in error detection and recovery mechanisms

### I2C Device Tree Configuration

I2C devices are described in device tree with bus and address information:

```c
#include <zephyr/drivers/i2c.h>

// I2C device specification from device tree
#define SENSOR_NODE DT_NODELABEL(temp_sensor)

static const struct i2c_dt_spec sensor = I2C_DT_SPEC_GET(SENSOR_NODE);
```

**Corresponding Device Tree Entry:**

```dts
&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;
    
    temp_sensor: sensor@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
        label = "TMP102";
    };
};
```

### I2C Communication Operations

**Device Initialization:**

```c
int sensor_init(void)
{
    if (!i2c_is_ready_dt(&sensor)) {
        printk("Error: I2C device %s is not ready\n", sensor.bus->name);
        return -ENODEV;
    }
    
    printk("I2C device %s at address 0x%02x is ready\n", 
           sensor.bus->name, sensor.addr);
    
    return 0;
}
```

**Register-Based Communication:**

```c
// Read single register
int sensor_read_register(uint8_t reg_addr, uint8_t *value)
{
    int ret;
    
    ret = i2c_write_read_dt(&sensor, &reg_addr, 1, value, 1);
    if (ret < 0) {
        printk("Error %d: failed to read register 0x%02x\n", ret, reg_addr);
        return ret;
    }
    
    return 0;
}

// Write single register
int sensor_write_register(uint8_t reg_addr, uint8_t value)
{
    uint8_t data[2] = {reg_addr, value};
    int ret;
    
    ret = i2c_write_dt(&sensor, data, sizeof(data));
    if (ret < 0) {
        printk("Error %d: failed to write register 0x%02x\n", ret, reg_addr);
        return ret;
    }
    
    return 0;
}
```

**Multi-Byte Data Transfer:**

```c
// Read temperature data (16-bit value)
int sensor_read_temperature(int16_t *temperature)
{
    uint8_t temp_reg = 0x00;  // Temperature register address
    uint8_t temp_data[2];
    int ret;
    
    ret = i2c_write_read_dt(&sensor, &temp_reg, 1, temp_data, 2);
    if (ret < 0) {
        printk("Error %d: failed to read temperature\n", ret);
        return ret;
    }
    
    // Convert to temperature (TMP102 format: 12-bit, 0.0625°C per LSB)
    *temperature = ((int16_t)(temp_data[0] << 8 | temp_data[1])) >> 4;
    
    return 0;
}

// Get human-readable temperature
float sensor_get_temperature_celsius(void)
{
    int16_t raw_temp;
    int ret;
    
    ret = sensor_read_temperature(&raw_temp);
    if (ret < 0) {
        return NAN;  // Return NaN on error
    }
    
    return raw_temp * 0.0625f;  // Convert to Celsius
}
```

### I2C Error Handling and Recovery

```c
int i2c_transaction_with_retry(const struct i2c_dt_spec *spec,
                              struct i2c_msg *msgs, uint8_t num_msgs,
                              int max_retries)
{
    int ret;
    int retry_count = 0;
    
    do {
        ret = i2c_transfer_dt(spec, msgs, num_msgs);
        if (ret == 0) {
            return 0;  // Success
        }
        
        retry_count++;
        printk("I2C transaction failed (attempt %d/%d): %d\n", 
               retry_count, max_retries, ret);
        
        // Brief delay before retry
        k_msleep(10);
        
    } while (retry_count < max_retries);
    
    printk("I2C transaction failed after %d attempts\n", max_retries);
    return ret;
}
```

## Device Tree Integration and Preprocessor Techniques

### Understanding Device Tree Macros

Zephyr's preprocessor system automatically generates code from device tree descriptions, enabling type-safe hardware abstraction:

```c
// Device tree macro examples
#define LED_NODE        DT_ALIAS(led0)
#define LED_GPIO_LABEL  DT_GPIO_LABEL(LED_NODE, gpios)
#define LED_GPIO_PIN    DT_GPIO_PIN(LED_NODE, gpios)
#define LED_GPIO_FLAGS  DT_GPIO_FLAGS(LED_NODE, gpios)

// Check if device exists at compile time
#if DT_NODE_HAS_STATUS(LED_NODE, okay)
    // LED is available
    static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
#else
    #warning "LED not available on this board"
#endif
```

**Advanced Device Tree Processing:**

```c
// Iterate over all I2C devices
#define SENSOR_INIT(node_id)                                          \
    do {                                                               \
        if (DT_NODE_HAS_STATUS(node_id, okay)) {                     \
            const struct i2c_dt_spec spec = I2C_DT_SPEC_GET(node_id); \
            sensor_init_instance(&spec, DT_PROP(node_id, label));     \
        }                                                              \
    } while (0)

// Apply to all temperature sensors
DT_FOREACH_STATUS_OKAY_VARGS(ti_tmp102, SENSOR_INIT);
```

### Compile-Time Configuration

Use preprocessor techniques for efficient, configurable code:

```c
// Conditional compilation based on device tree
#if DT_NODE_EXISTS(DT_ALIAS(led0))
    #define HAS_STATUS_LED 1
    static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#else
    #define HAS_STATUS_LED 0
#endif

void status_led_set(bool state)
{
#if HAS_STATUS_LED
    gpio_pin_set_dt(&status_led, state);
#else
    printk("Status LED: %s\n", state ? "ON" : "OFF");
#endif
}

// Configuration-based feature inclusion
#ifdef CONFIG_APP_ENABLE_SENSORS
    static int init_sensors(void)
    {
        // Sensor initialization code
        return 0;
    }
#else
    static inline int init_sensors(void) { return 0; }
#endif
```

## Zephyr Shell: Interactive Development

### Shell Architecture and Integration

The Zephyr Shell provides a powerful command-line interface for embedded applications, enabling real-time interaction, debugging, and system administration:

```c
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

// Enable shell in configuration
// CONFIG_SHELL=y
// CONFIG_SHELL_BACKEND_SERIAL=y
```

### Custom Shell Commands

**Basic Command Implementation:**

```c
static int cmd_led_control(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: led <on|off|toggle>");
        return -EINVAL;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        led_control(true);
        shell_print(sh, "LED turned on");
    } else if (strcmp(argv[1], "off") == 0) {
        led_control(false);
        shell_print(sh, "LED turned off");
    } else if (strcmp(argv[1], "toggle") == 0) {
        led_toggle();
        shell_print(sh, "LED toggled");
    } else {
        shell_error(sh, "Invalid argument: %s", argv[1]);
        return -EINVAL;
    }
    
    return 0;
}

// Register command
SHELL_CMD_REGISTER(led, NULL, "Control LED (on/off/toggle)", cmd_led_control);
```

**Hierarchical Command Structure:**

```c
// Sensor reading commands
static int cmd_sensor_temp(const struct shell *sh, size_t argc, char **argv)
{
    float temperature = sensor_get_temperature_celsius();
    
    if (isnan(temperature)) {
        shell_error(sh, "Failed to read temperature");
        return -EIO;
    }
    
    shell_print(sh, "Temperature: %.2f°C", temperature);
    return 0;
}

static int cmd_sensor_status(const struct shell *sh, size_t argc, char **argv)
{
    if (i2c_is_ready_dt(&sensor)) {
        shell_print(sh, "Sensor: Ready at address 0x%02x", sensor.addr);
    } else {
        shell_error(sh, "Sensor: Not ready");
    }
    
    return 0;
}

// Create sensor command group
SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensor,
    SHELL_CMD(temp, NULL, "Read temperature", cmd_sensor_temp),
    SHELL_CMD(status, NULL, "Show sensor status", cmd_sensor_status),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sensor, &sub_sensor, "Sensor operations", NULL);
```

### Advanced Shell Features

**Command with Arguments and Validation:**

```c
static int cmd_gpio_set(const struct shell *sh, size_t argc, char **argv)
{
    int pin, value;
    char *endptr;
    
    if (argc != 3) {
        shell_error(sh, "Usage: gpio set <pin> <0|1>");
        return -EINVAL;
    }
    
    // Parse pin number
    pin = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || pin < 0 || pin > 31) {
        shell_error(sh, "Invalid pin number: %s", argv[1]);
        return -EINVAL;
    }
    
    // Parse value
    value = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || (value != 0 && value != 1)) {
        shell_error(sh, "Invalid value: %s (use 0 or 1)", argv[2]);
        return -EINVAL;
    }
    
    // Set GPIO pin (simplified example)
    shell_print(sh, "Setting GPIO pin %d to %d", pin, value);
    
    return 0;
}

// Command with tab completion
static void gpio_pin_get(size_t idx, struct shell_static_entry *entry)
{
    static const char *pins[] = {"0", "1", "2", "18", "21", NULL};
    
    if (idx < ARRAY_SIZE(pins) - 1) {
        entry->syntax = pins[idx];
        entry->handler = NULL;
        entry->help = "GPIO pin number";
    } else {
        entry->syntax = NULL;
    }
}

SHELL_DYNAMIC_CMD_CREATE(gpio_pins, gpio_pin_get);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio,
    SHELL_CMD_ARG(set, &gpio_pins, "Set GPIO pin value", cmd_gpio_set, 3, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gpio, &sub_gpio, "GPIO operations", NULL);
```

**Real-Time Monitoring Commands:**

```c
static int cmd_monitor_start(const struct shell *sh, size_t argc, char **argv)
{
    static struct k_timer monitor_timer;
    static bool monitoring = false;
    
    if (monitoring) {
        shell_warn(sh, "Monitoring already active");
        return 0;
    }
    
    shell_print(sh, "Starting system monitoring...");
    shell_print(sh, "Press any key to stop");
    
    monitoring = true;
    
    // Start periodic monitoring
    k_timer_start(&monitor_timer, K_SECONDS(1), K_SECONDS(1));
    
    while (monitoring) {
        float temp = sensor_get_temperature_celsius();
        bool button_state = button_is_pressed();
        
        shell_print(sh, "Temp: %.1f°C | Button: %s", 
                   temp, button_state ? "PRESSED" : "RELEASED");
        
        // Check for user input to stop
        if (shell_readkey(sh) != SHELL_KEY_NOT_AVAILABLE) {
            monitoring = false;
        }
        
        k_msleep(1000);
    }
    
    k_timer_stop(&monitor_timer);
    shell_print(sh, "Monitoring stopped");
    
    return 0;
}

SHELL_CMD_REGISTER(monitor, NULL, "Start real-time monitoring", cmd_monitor_start);
```

## Data Structures and Memory Management

### Efficient Data Structures for Embedded Systems

Zephyr provides optimized data structures designed for resource-constrained environments:

```c
#include <zephyr/sys/slist.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/sys/ring_buffer.h>

// Sensor data structure
struct sensor_reading {
    sys_snode_t node;           // Single-linked list node
    uint32_t timestamp;         // Reading timestamp
    float temperature;          // Temperature value
    uint8_t sensor_id;         // Sensor identifier
};

// Reading history using single-linked list
static sys_slist_t reading_history;

void add_sensor_reading(float temp, uint8_t id)
{
    struct sensor_reading *reading;
    
    reading = k_malloc(sizeof(struct sensor_reading));
    if (!reading) {
        printk("Failed to allocate memory for reading\n");
        return;
    }
    
    reading->timestamp = k_uptime_get_32();
    reading->temperature = temp;
    reading->sensor_id = id;
    
    sys_slist_append(&reading_history, &reading->node);
    
    printk("Added reading: %.2f°C from sensor %d at %u\n",
           temp, id, reading->timestamp);
}
```

**Ring Buffer for Continuous Data:**

```c
// Ring buffer for continuous sensor data
#define SENSOR_BUFFER_SIZE 1024
static uint8_t sensor_buffer_data[SENSOR_BUFFER_SIZE];
static struct ring_buf sensor_buffer;

int sensor_buffer_init(void)
{
    ring_buf_init(&sensor_buffer, sizeof(sensor_buffer_data), sensor_buffer_data);
    return 0;
}

int sensor_data_put(const void *data, size_t size)
{
    size_t written;
    
    written = ring_buf_put(&sensor_buffer, data, size);
    if (written != size) {
        printk("Buffer full, dropped %zu bytes\n", size - written);
        return -ENOMEM;
    }
    
    return 0;
}

int sensor_data_get(void *data, size_t size)
{
    size_t read;
    
    read = ring_buf_get(&sensor_buffer, data, size);
    return read;
}
```

### Memory Pool Management

```c
// Define memory pool for sensor readings
K_MEM_POOL_DEFINE(sensor_pool, 16, 64, 8, 4);

struct sensor_reading *alloc_sensor_reading(void)
{
    struct sensor_reading *reading;
    
    reading = k_mem_pool_alloc(&sensor_pool, sizeof(struct sensor_reading), K_NO_WAIT);
    if (!reading) {
        printk("Memory pool exhausted\n");
        return NULL;
    }
    
    memset(reading, 0, sizeof(struct sensor_reading));
    return reading;
}

void free_sensor_reading(struct sensor_reading *reading)
{
    k_mem_pool_free(&sensor_pool, reading);
}
```

## Integration Best Practices

### Combining Multiple Subsystems

Professional embedded applications integrate multiple subsystems seamlessly:

```c
// Application state structure
struct app_context {
    struct gpio_dt_spec status_led;
    struct gpio_dt_spec user_button;
    struct i2c_dt_spec temperature_sensor;
    struct k_timer periodic_timer;
    sys_slist_t sensor_readings;
    bool monitoring_active;
};

static struct app_context app;

// Integrated initialization
int app_init(void)
{
    int ret;
    
    // Initialize GPIO
    ret = gpio_pin_configure_dt(&app.status_led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret;
    
    ret = gpio_pin_configure_dt(&app.user_button, GPIO_INPUT);
    if (ret < 0) return ret;
    
    // Initialize I2C
    if (!i2c_is_ready_dt(&app.temperature_sensor)) {
        return -ENODEV;
    }
    
    // Initialize data structures
    sys_slist_init(&app.sensor_readings);
    
    // Start periodic monitoring
    k_timer_init(&app.periodic_timer, sensor_timer_handler, NULL);
    k_timer_start(&app.periodic_timer, K_SECONDS(5), K_SECONDS(5));
    
    printk("Application initialized successfully\n");
    return 0;
}

void sensor_timer_handler(struct k_timer *timer)
{
    float temperature = sensor_get_temperature_celsius();
    
    if (!isnan(temperature)) {
        add_sensor_reading(temperature, 1);
        
        // Update status LED based on temperature
        if (temperature > 25.0f) {
            gpio_pin_set_dt(&app.status_led, 1);  // High temperature warning
        } else {
            gpio_pin_set_dt(&app.status_led, 0);  // Normal temperature
        }
    }
}
```

---

This theoretical foundation prepares you for hands-on implementation in the Lab section, where you'll build applications that demonstrate these concepts using your Raspberry Pi 4B platform.

[Next: Zephyr Fundamentals Lab](./lab.md)
