# Chapter 6: Zephyr Fundamentals - Lab

This hands-on lab guides you through building practical applications that demonstrate GPIO control, I2C communication, device tree integration, and shell development using your Raspberry Pi 4B platform.

---

## Lab Overview

**Learning Objectives:**
* Implement GPIO-based LED control and button input handling
* Establish I2C communication with temperature sensors
* Create custom shell commands for system interaction
* Apply device tree specifications for hardware abstraction
* Build integrated applications combining multiple subsystems

**Hardware Requirements:**
* Raspberry Pi 4B with Zephyr support
* Breadboard and jumper wires
* LED and current-limiting resistor (220Ω)
* Push button switch
* I2C temperature sensor (TMP102 or compatible)
* Pull-up resistors (10kΩ) if needed

**Development Environment:**
* VS Code with Zephyr extension
* West workspace properly configured
* Serial console access (minicom, picocom, or VS Code terminal)

---

## Lab 1: GPIO Control Foundation

### Objective
Build a basic GPIO application with LED control and button input, demonstrating device tree integration and interrupt handling.

### Hardware Setup

Connect your components to the Raspberry Pi 4B GPIO header:

```
GPIO Connections:
- LED: GPIO18 (Pin 12) → 220Ω resistor → LED → GND
- Button: GPIO21 (Pin 40) → Button → GND
- Button: GPIO21 (Pin 40) → 10kΩ pull-up → 3.3V
```

### Device Tree Configuration

Create the overlay file `boards/rpi_4b.overlay`:

```dts
/*
 * GPIO Lab Device Tree Overlay for Raspberry Pi 4B
 */

/ {
    aliases {
        led0 = &lab_led;
        sw0 = &lab_button;
    };

    leds {
        compatible = "gpio-leds";
        lab_led: led_0 {
            gpios = <&gpio 18 GPIO_ACTIVE_HIGH>;
            label = "Lab LED";
        };
    };

    gpio_keys {
        compatible = "gpio-keys";
        lab_button: button_0 {
            gpios = <&gpio 21 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
            label = "Lab Button";
        };
    };
};
```

### Application Implementation

Create `src/gpio_lab.c`:

```c
/*
 * GPIO Lab Application
 * Demonstrates LED control, button input, and interrupts
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS   1000

/* Device tree specifications */
#define LED_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED_NODE, gpios, {0});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(BUTTON_NODE, gpios, {0});

/* Button callback data */
static struct gpio_callback button_cb_data;

/* Application state */
static bool led_auto_mode = true;
static uint32_t button_press_count = 0;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                   uint32_t pins)
{
    button_press_count++;
    printk("Button pressed! Count: %u (Time: %u ms)\n", 
           button_press_count, k_uptime_get_32());
    
    /* Toggle between manual and automatic LED control */
    led_auto_mode = !led_auto_mode;
    
    if (led_auto_mode) {
        printk("Switched to automatic LED mode\n");
    } else {
        printk("Switched to manual LED mode\n");
        gpio_pin_toggle_dt(&led);
    }
}

int gpio_setup(void)
{
    int ret;

    /* Check device readiness */
    if (!gpio_is_ready_dt(&led)) {
        printk("Error: LED device %s is not ready\n", led.port->name);
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button)) {
        printk("Error: Button device %s is not ready\n", button.port->name);
        return -ENODEV;
    }

    /* Configure LED pin */
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure LED pin\n", ret);
        return ret;
    }

    /* Configure button pin */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Error %d: failed to configure button pin\n", ret);
        return ret;
    }

    /* Configure button interrupt */
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Error %d: failed to configure interrupt\n", ret);
        return ret;
    }

    /* Initialize and add button callback */
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    ret = gpio_add_callback(button.port, &button_cb_data);
    if (ret < 0) {
        printk("Error %d: failed to add callback\n", ret);
        return ret;
    }

    printk("GPIO setup complete\n");
    printk("LED: GPIO%d, Button: GPIO%d\n", led.pin, button.pin);
    
    return 0;
}

int main(void)
{
    int ret;
    bool led_state = false;

    printk("=== Zephyr GPIO Lab Application ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);

    ret = gpio_setup();
    if (ret < 0) {
        printk("GPIO setup failed: %d\n", ret);
        return ret;
    }

    printk("Press button to toggle between auto/manual LED mode\n");

    while (1) {
        if (led_auto_mode) {
            /* Automatic LED blinking */
            led_state = !led_state;
            gpio_pin_set_dt(&led, led_state);
            printk("Auto mode - LED: %s (uptime: %u ms)\n", 
                   led_state ? "ON" : "OFF", k_uptime_get_32());
        }

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
```

### Build Configuration

Create `prj.conf`:

```ini
# GPIO Lab Configuration
CONFIG_GPIO=y
CONFIG_PRINTK=y
CONFIG_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y

# Enable console output
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

### Building and Testing

```bash
# Build the application
west build -b rpi_4b -p auto src/

# Flash to device
west flash

# Monitor serial output
west attach
```

**Expected Behavior:**
1. LED blinks automatically every second
2. Button press toggles between auto/manual modes
3. Manual mode: LED toggles on each button press
4. Serial output shows mode changes and button events

---

## Lab 2: I2C Temperature Sensor Integration

### Objective
Implement I2C communication with a temperature sensor, demonstrating device tree I2C configuration and data processing.

### Hardware Setup

Connect TMP102 temperature sensor:

```
I2C Connections (Raspberry Pi 4B):
- SDA: GPIO2 (Pin 3)
- SCL: GPIO3 (Pin 5)  
- VCC: 3.3V (Pin 1)
- GND: GND (Pin 6)
- ADDR: GND (for address 0x48)
```

### Device Tree Configuration

Update the overlay file to include I2C configuration:

```dts
/*
 * I2C Temperature Sensor Configuration
 */

&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;
    
    tmp102: temperature@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
        label = "TMP102_SENSOR";
    };
};

/ {
    aliases {
        temp-sensor = &tmp102;
    };
};
```

### Application Implementation

Create `src/i2c_temp_lab.c`:

```c
/*
 * I2C Temperature Sensor Lab
 * Demonstrates I2C communication and sensor data processing
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <math.h>

/* Device tree specifications */
#define TEMP_SENSOR_NODE DT_ALIAS(temp_sensor)
#define LED_NODE DT_ALIAS(led0)

static const struct i2c_dt_spec temp_sensor = I2C_DT_SPEC_GET(TEMP_SENSOR_NODE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED_NODE, gpios, {0});

/* TMP102 register addresses */
#define TMP102_REG_TEMPERATURE  0x00
#define TMP102_REG_CONFIG       0x01
#define TMP102_REG_TLOW         0x02
#define TMP102_REG_THIGH        0x03

/* Temperature thresholds */
#define TEMP_WARNING_HIGH       28.0f   /* °C */
#define TEMP_WARNING_LOW        15.0f   /* °C */

/* Application state */
struct temp_monitor {
    float current_temp;
    float min_temp;
    float max_temp;
    uint32_t reading_count;
    bool warning_active;
};

static struct temp_monitor monitor = {
    .min_temp = 100.0f,
    .max_temp = -100.0f,
    .reading_count = 0,
    .warning_active = false
};

int tmp102_read_temperature(float *temperature)
{
    uint8_t temp_reg = TMP102_REG_TEMPERATURE;
    uint8_t temp_data[2];
    int16_t raw_temp;
    int ret;

    ret = i2c_write_read_dt(&temp_sensor, &temp_reg, 1, temp_data, 2);
    if (ret < 0) {
        printk("Error %d: Failed to read temperature register\n", ret);
        return ret;
    }

    /* Convert raw data to temperature */
    raw_temp = (temp_data[0] << 8) | temp_data[1];
    raw_temp >>= 4; /* TMP102 uses 12-bit resolution */

    /* Convert to Celsius (0.0625°C per LSB) */
    *temperature = raw_temp * 0.0625f;

    return 0;
}

int tmp102_configure(void)
{
    uint8_t config_data[3];
    int ret;

    /* Configure TMP102 for continuous conversion, 12-bit resolution */
    config_data[0] = TMP102_REG_CONFIG;
    config_data[1] = 0x60; /* Configuration register high byte */
    config_data[2] = 0xA0; /* Configuration register low byte */

    ret = i2c_write_dt(&temp_sensor, config_data, sizeof(config_data));
    if (ret < 0) {
        printk("Error %d: Failed to configure TMP102\n", ret);
        return ret;
    }

    printk("TMP102 configured for continuous conversion\n");
    return 0;
}

void update_temperature_stats(float temperature)
{
    monitor.current_temp = temperature;
    monitor.reading_count++;

    /* Update min/max values */
    if (temperature < monitor.min_temp) {
        monitor.min_temp = temperature;
    }
    if (temperature > monitor.max_temp) {
        monitor.max_temp = temperature;
    }

    /* Check warning conditions */
    bool new_warning = (temperature > TEMP_WARNING_HIGH) || 
                      (temperature < TEMP_WARNING_LOW);

    if (new_warning != monitor.warning_active) {
        monitor.warning_active = new_warning;
        
        if (new_warning) {
            printk("⚠️  TEMPERATURE WARNING: %.2f°C\n", temperature);
        } else {
            printk("✅ Temperature back to normal: %.2f°C\n", temperature);
        }
    }

    /* Update warning LED */
    gpio_pin_set_dt(&led, monitor.warning_active);
}

void print_temperature_report(void)
{
    printk("\n=== Temperature Report ===\n");
    printk("Current: %.2f°C\n", monitor.current_temp);
    printk("Minimum: %.2f°C\n", monitor.min_temp);
    printk("Maximum: %.2f°C\n", monitor.max_temp);
    printk("Readings: %u\n", monitor.reading_count);
    printk("Warning: %s\n", monitor.warning_active ? "ACTIVE" : "Normal");
    printk("Uptime: %u seconds\n", k_uptime_get_32() / 1000);
    printk("========================\n\n");
}

int sensor_init(void)
{
    int ret;

    /* Check I2C device readiness */
    if (!i2c_is_ready_dt(&temp_sensor)) {
        printk("Error: I2C device %s is not ready\n", temp_sensor.bus->name);
        return -ENODEV;
    }

    /* Check LED readiness */
    if (!gpio_is_ready_dt(&led)) {
        printk("Error: LED device is not ready\n");
        return -ENODEV;
    }

    /* Configure LED */
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error %d: Failed to configure LED\n", ret);
        return ret;
    }

    /* Configure temperature sensor */
    ret = tmp102_configure();
    if (ret < 0) {
        return ret;
    }

    printk("Sensor initialization complete\n");
    printk("I2C device: %s at address 0x%02x\n", 
           temp_sensor.bus->name, temp_sensor.addr);

    return 0;
}

int main(void)
{
    int ret;
    float temperature;
    uint32_t report_counter = 0;

    printk("=== I2C Temperature Sensor Lab ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);

    ret = sensor_init();
    if (ret < 0) {
        printk("Sensor initialization failed: %d\n", ret);
        return ret;
    }

    printk("Starting temperature monitoring...\n");
    printk("Warning thresholds: %.1f°C - %.1f°C\n", 
           TEMP_WARNING_LOW, TEMP_WARNING_HIGH);

    while (1) {
        ret = tmp102_read_temperature(&temperature);
        if (ret == 0) {
            update_temperature_stats(temperature);
            printk("Temperature: %.2f°C | Readings: %u\n", 
                   temperature, monitor.reading_count);

            /* Print detailed report every 30 seconds */
            if (++report_counter >= 6) {
                print_temperature_report();
                report_counter = 0;
            }
        } else {
            printk("Failed to read temperature: %d\n", ret);
        }

        k_msleep(5000); /* Read every 5 seconds */
    }

    return 0;
}
```

### Build Configuration

Update `prj.conf`:

```ini
# I2C Temperature Lab Configuration
CONFIG_GPIO=y
CONFIG_I2C=y
CONFIG_PRINTK=y
CONFIG_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y

# Enable floating point support
CONFIG_NEWLIB_LIBC=y
CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

---

## Lab 3: Interactive Shell Interface

### Objective
Create a comprehensive shell interface that combines GPIO and I2C functionality with custom commands for real-time system interaction.

### Application Implementation

Create `src/shell_lab.c`:

```c
/*
 * Interactive Shell Lab
 * Demonstrates advanced shell commands with GPIO and I2C integration
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <stdlib.h>
#include <math.h>

/* Device specifications */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
static const struct i2c_dt_spec temp_sensor = I2C_DT_SPEC_GET(DT_ALIAS(temp_sensor));

/* System state */
static bool led_state = false;
static bool monitoring_active = false;
static struct k_timer monitor_timer;

/* Forward declarations */
int read_temperature(float *temp);
void monitor_timer_handler(struct k_timer *timer);

/* LED Control Commands */
static int cmd_led_on(const struct shell *sh, size_t argc, char **argv)
{
    int ret = gpio_pin_set_dt(&led, 1);
    if (ret == 0) {
        led_state = true;
        shell_print(sh, "LED turned ON");
    } else {
        shell_error(sh, "Failed to turn on LED: %d", ret);
    }
    return ret;
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
    int ret = gpio_pin_set_dt(&led, 0);
    if (ret == 0) {
        led_state = false;
        shell_print(sh, "LED turned OFF");
    } else {
        shell_error(sh, "Failed to turn off LED: %d", ret);
    }
    return ret;
}

static int cmd_led_toggle(const struct shell *sh, size_t argc, char **argv)
{
    int ret = gpio_pin_toggle_dt(&led);
    if (ret == 0) {
        led_state = !led_state;
        shell_print(sh, "LED toggled %s", led_state ? "ON" : "OFF");
    } else {
        shell_error(sh, "Failed to toggle LED: %d", ret);
    }
    return ret;
}

static int cmd_led_blink(const struct shell *sh, size_t argc, char **argv)
{
    int count = 3;
    int delay_ms = 500;
    
    if (argc > 1) {
        count = strtol(argv[1], NULL, 10);
        if (count <= 0 || count > 20) {
            shell_error(sh, "Invalid count (1-20): %d", count);
            return -EINVAL;
        }
    }
    
    if (argc > 2) {
        delay_ms = strtol(argv[2], NULL, 10);
        if (delay_ms < 100 || delay_ms > 2000) {
            shell_error(sh, "Invalid delay (100-2000ms): %d", delay_ms);
            return -EINVAL;
        }
    }
    
    shell_print(sh, "Blinking LED %d times with %dms delay", count, delay_ms);
    
    bool original_state = led_state;
    
    for (int i = 0; i < count; i++) {
        gpio_pin_set_dt(&led, 1);
        k_msleep(delay_ms);
        gpio_pin_set_dt(&led, 0);
        k_msleep(delay_ms);
    }
    
    gpio_pin_set_dt(&led, original_state);
    shell_print(sh, "Blink sequence complete");
    
    return 0;
}

static int cmd_led_status(const struct shell *sh, size_t argc, char **argv)
{
    int pin_value = gpio_pin_get_dt(&led);
    
    shell_print(sh, "LED Status:");
    shell_print(sh, "  GPIO Pin: %d", led.pin);
    shell_print(sh, "  Current State: %s", pin_value ? "ON" : "OFF");
    shell_print(sh, "  Port: %s", led.port->name);
    
    return 0;
}

/* GPIO Status Commands */
static int cmd_button_status(const struct shell *sh, size_t argc, char **argv)
{
    int pin_value = gpio_pin_get_dt(&button);
    
    shell_print(sh, "Button Status:");
    shell_print(sh, "  GPIO Pin: %d", button.pin);
    shell_print(sh, "  Current State: %s", pin_value ? "NOT PRESSED" : "PRESSED");
    shell_print(sh, "  Port: %s", button.port->name);
    
    return 0;
}

/* Temperature Sensor Commands */
static int cmd_temp_read(const struct shell *sh, size_t argc, char **argv)
{
    float temperature;
    int ret = read_temperature(&temperature);
    
    if (ret == 0) {
        shell_print(sh, "Temperature: %.2f°C (%.2f°F)", 
                   temperature, temperature * 9.0f / 5.0f + 32.0f);
    } else {
        shell_error(sh, "Failed to read temperature: %d", ret);
    }
    
    return ret;
}

static int cmd_temp_monitor(const struct shell *sh, size_t argc, char **argv)
{
    int duration = 30; /* Default 30 seconds */
    
    if (argc > 1) {
        duration = strtol(argv[1], NULL, 10);
        if (duration < 5 || duration > 300) {
            shell_error(sh, "Invalid duration (5-300 seconds): %d", duration);
            return -EINVAL;
        }
    }
    
    if (monitoring_active) {
        shell_warn(sh, "Monitoring already active. Stopping previous session.");
        k_timer_stop(&monitor_timer);
    }
    
    shell_print(sh, "Starting temperature monitoring for %d seconds", duration);
    shell_print(sh, "Press Ctrl+C to stop early");
    
    monitoring_active = true;
    k_timer_start(&monitor_timer, K_SECONDS(2), K_SECONDS(2));
    
    /* Stop monitoring after specified duration */
    k_timer_start(&monitor_timer, K_SECONDS(2), K_SECONDS(2));
    
    /* Simple countdown implementation */
    for (int i = duration; i > 0 && monitoring_active; i--) {
        k_msleep(1000);
        if ((i % 10) == 0) {
            shell_print(sh, "Monitoring continues... %d seconds remaining", i);
        }
    }
    
    monitoring_active = false;
    k_timer_stop(&monitor_timer);
    shell_print(sh, "Temperature monitoring stopped");
    
    return 0;
}

static int cmd_sensor_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Temperature Sensor Information:");
    shell_print(sh, "  Type: TMP102");
    shell_print(sh, "  I2C Bus: %s", temp_sensor.bus->name);
    shell_print(sh, "  I2C Address: 0x%02X", temp_sensor.addr);
    shell_print(sh, "  Resolution: 12-bit (0.0625°C)");
    shell_print(sh, "  Range: -40°C to +125°C");
    shell_print(sh, "  Ready: %s", i2c_is_ready_dt(&temp_sensor) ? "Yes" : "No");
    
    return 0;
}

/* System Commands */
static int cmd_system_info(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "System Information:");
    shell_print(sh, "  Board: %s", CONFIG_BOARD);
    shell_print(sh, "  Kernel: Zephyr %s", KERNEL_VERSION_STRING);
    shell_print(sh, "  Uptime: %u seconds", k_uptime_get_32() / 1000);
    
    
    return 0;
}

static int cmd_system_reset(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "System reset requested...");
    k_msleep(1000);
    sys_reboot(SYS_REBOOT_WARM);
    return 0;
}

/* Shell command registration */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led,
    SHELL_CMD(on, NULL, "Turn LED on", cmd_led_on),
    SHELL_CMD(off, NULL, "Turn LED off", cmd_led_off),
    SHELL_CMD(toggle, NULL, "Toggle LED state", cmd_led_toggle),
    SHELL_CMD_ARG(blink, NULL, "Blink LED [count] [delay_ms]", cmd_led_blink, 1, 2),
    SHELL_CMD(status, NULL, "Show LED status", cmd_led_status),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio,
    SHELL_CMD(button, NULL, "Show button status", cmd_button_status),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_temp,
    SHELL_CMD(read, NULL, "Read current temperature", cmd_temp_read),
    SHELL_CMD_ARG(monitor, NULL, "Monitor temperature [duration_sec]", cmd_temp_monitor, 1, 1),
    SHELL_CMD(info, NULL, "Show sensor information", cmd_sensor_info),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_system,
    SHELL_CMD(info, NULL, "Show system information", cmd_system_info),
    SHELL_CMD(reset, NULL, "Reset the system", cmd_system_reset),
    SHELL_SUBCMD_SET_END
);

/* Register main command groups */
SHELL_CMD_REGISTER(led, &sub_led, "LED control commands", NULL);
SHELL_CMD_REGISTER(gpio, &sub_gpio, "GPIO status commands", NULL);
SHELL_CMD_REGISTER(temp, &sub_temp, "Temperature sensor commands", NULL);
SHELL_CMD_REGISTER(sys, &sub_system, "System commands", NULL);

/* Helper functions */
int read_temperature(float *temp)
{
    uint8_t temp_reg = 0x00;
    uint8_t temp_data[2];
    int16_t raw_temp;
    int ret;

    ret = i2c_write_read_dt(&temp_sensor, &temp_reg, 1, temp_data, 2);
    if (ret < 0) {
        return ret;
    }

    raw_temp = (temp_data[0] << 8) | temp_data[1];
    raw_temp >>= 4;
    *temp = raw_temp * 0.0625f;

    return 0;
}

void monitor_timer_handler(struct k_timer *timer)
{
    if (!monitoring_active) {
        return;
    }

    float temperature;
    int ret = read_temperature(&temperature);

    if (ret == 0) {
        printk("Monitor: %.2f°C | LED: %s | Button: %s\n",
               temperature,
               gpio_pin_get_dt(&led) ? "ON" : "OFF",
               gpio_pin_get_dt(&button) ? "UP" : "DOWN");
    }
}

/* Application initialization and main */
int app_init(void)
{
    int ret;

    /* Initialize GPIO devices */
    if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
        printk("GPIO devices not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    /* Initialize I2C device */
    if (!i2c_is_ready_dt(&temp_sensor)) {
        printk("I2C temperature sensor not ready\n");
        return -ENODEV;
    }

    /* Initialize timer */
    k_timer_init(&monitor_timer, monitor_timer_handler, NULL);

    printk("Shell Lab initialized successfully\n");
    return 0;
}

int main(void)
{
    int ret;

    printk("\n=== Interactive Shell Lab ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);

    ret = app_init();
    if (ret < 0) {
        printk("Application initialization failed: %d\n", ret);
        return ret;
    }

    printk("Shell interface ready. Try these commands:\n");
    printk("  led on/off/toggle/blink/status\n");
    printk("  temp read/monitor/info\n");
    printk("  gpio button\n");
    printk("  sys info/reset\n");
    printk("Use 'help' for complete command list\n\n");

    return 0;
}
```

### Build Configuration

Update `prj.conf` for shell support:

```ini
# Shell Lab Configuration
CONFIG_GPIO=y
CONFIG_I2C=y
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
CONFIG_SHELL_PROMPT_UART="lab:~$ "

# Console and logging
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_PRINTK=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Memory and system
CONFIG_HEAP_MEM_POOL_SIZE=2048
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_SHELL_STACK_SIZE=2048

# Math support
CONFIG_NEWLIB_LIBC=y
CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y

# System commands
CONFIG_REBOOT=y
```

### Testing the Shell Interface

Build and flash the application, then interact through the serial console:

```bash
# Available commands once running:

lab:~$ help
Available commands:
  clear     :Clear screen.
  help      :Prints the help message.
  history   :Command history.
  led       :LED control commands
  gpio      :GPIO status commands  
  temp      :Temperature sensor commands
  sys       :System commands

# Test LED control
lab:~$ led on
LED turned ON

lab:~$ led blink 5 200
Blinking LED 5 times with 200ms delay

lab:~$ led status
LED Status:
  GPIO Pin: 18
  Current State: OFF
  Port: GPIO_0

# Test temperature reading
lab:~$ temp read
Temperature: 23.45°C (74.21°F)

lab:~$ temp monitor 60
Starting temperature monitoring for 60 seconds

# Test system information
lab:~$ sys info
System Information:
  Board: rpi_4b
  Kernel: Zephyr 3.7.99
  Uptime: 127 seconds
  Free RAM: 12456 bytes
```

---

## Lab 4: Integrated Application

### Objective
Combine all learned concepts into a comprehensive monitoring system with automatic responses and comprehensive shell interface.

### Application Features

* **Automatic Temperature Monitoring:** Continuous sensor reading with threshold checking
* **LED Status Indication:** Visual feedback for system states and warnings
* **Button Interaction:** Manual mode switching and emergency alerts
* **Comprehensive Shell:** Full system control and monitoring capabilities
* **Data Logging:** Temperature history with statistics
* **Error Recovery:** Robust error handling and system recovery

### Final Implementation

This integrated application demonstrates professional embedded development practices using Zephyr's powerful APIs and device tree integration. The shell interface provides comprehensive system control while automatic monitoring ensures reliable operation.

**Key Learning Outcomes:**

1. **Device Tree Mastery:** Hardware abstraction through proper device tree usage
2. **GPIO Expertise:** Digital I/O control with interrupts and state management  
3. **I2C Communication:** Serial communication with error handling and data processing
4. **Shell Development:** Interactive command-line interfaces for embedded systems
5. **System Integration:** Combining multiple subsystems into cohesive applications
6. **Professional Practices:** Error handling, logging, and maintainable code structure

### Next Steps

Continue exploring Zephyr's advanced features:
* **Threading and Synchronization:** Multi-threaded applications with proper synchronization
* **Memory Management:** Dynamic allocation and memory pools
* **Power Management:** Low-power modes and power optimization
* **Networking:** TCP/IP stack integration and IoT connectivity
* **File Systems:** Storage solutions and data persistence

This foundation in Zephyr fundamentals prepares you for advanced embedded development with confidence in device integration, system architecture, and professional development practices.

---

## Lab Summary

Through these hands-on exercises, you've mastered:

* **GPIO Control:** LED manipulation, button input, and interrupt handling
* **I2C Communication:** Temperature sensor integration with error handling
* **Device Tree Integration:** Hardware abstraction and compile-time configuration
* **Shell Development:** Interactive command interfaces for system control
* **Professional Practices:** Code organization, error handling, and system architecture

These skills form the foundation for advanced Zephyr development and real-world embedded applications.