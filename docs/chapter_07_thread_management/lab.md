# Chapter 7: Thread Management - Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

This hands-on lab guides you through building multi-threaded applications that demonstrate thread creation, synchronization, timing control, and performance optimization using your Raspberry Pi 4B platform.

---

## Lab Overview

**Learning Objectives:**

* Master static and dynamic thread creation techniques
* Implement thread synchronization using mutexes, semaphores, and condition variables
* Apply precise timing control and timeout handling
* Build concurrent sensor monitoring systems
* Debug and optimize multi-threaded applications

**Hardware Requirements:**

* Raspberry Pi 4B with Zephyr support
* Breadboard and jumper wires
* 2x LEDs with current-limiting resistors (220Ω)
* Push button switch with pull-up resistor (10kΩ)
* I2C temperature sensor (TMP102 or compatible)
* Optional: Logic analyzer or oscilloscope for timing analysis

**Development Environment:**

* VS Code with Zephyr extension
* West workspace properly configured
* Serial console access for monitoring thread behavior
* Thread analyzer tools enabled

---

## Lab 1: Thread Creation Fundamentals

### Objective

Build a foundation in thread creation by implementing both static and dynamic threads, demonstrating thread lifecycle management and basic synchronization.

### Hardware Setup

Connect components to your Raspberry Pi 4B:

```text
GPIO Connections:
- Status LED: GPIO18 (Pin 12) → 220Ω resistor → LED → GND
- Activity LED: GPIO19 (Pin 35) → 220Ω resistor → LED → GND  
- Control Button: GPIO21 (Pin 40) → Button → GND (with internal pull-up)
- Temperature Sensor: I2C1 (SDA: GPIO2, SCL: GPIO3)
```

### Device Tree Configuration

Create `boards/rpi_4b.overlay`:

```dts
/*
 * Thread Management Lab Device Tree Overlay
 */

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
    clock-frequency = <I2C_BITRATE_FAST>;
    
    temp_sensor: temperature@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
        label = "Temperature Sensor";
    };
};
```

### Static Thread Implementation

Create `src/thread_basics.c`:

```c
/*
 * Thread Creation Fundamentals Lab
 * Demonstrates static and dynamic thread creation
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

/* Device tree specifications */
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(activity_led), gpios);
static const struct gpio_dt_spec control_btn = GPIO_DT_SPEC_GET(DT_ALIAS(control_btn), gpios);
static const struct i2c_dt_spec temp_sensor = I2C_DT_SPEC_GET(DT_ALIAS(temp_sensor));

/* Thread parameters */
#define STATUS_THREAD_STACK_SIZE    1024
#define STATUS_THREAD_PRIORITY      5

#define SENSOR_THREAD_STACK_SIZE    1024  
#define SENSOR_THREAD_PRIORITY      4

#define ACTIVITY_THREAD_STACK_SIZE  512
#define ACTIVITY_THREAD_PRIORITY    6

/* Global state */
static volatile bool system_active = true;
static volatile uint32_t sensor_reading_count = 0;
static volatile float last_temperature = 0.0f;
static bool activity_thread_running = false;

/* Status thread - static thread showing system heartbeat */
void status_thread_entry(void *p1, void *p2, void *p3)
{
    bool led_state = false;
    uint32_t heartbeat_count = 0;
    
    printk("Status thread started (TID: %p)\n", k_current_get());
    
    while (system_active) {
        /* Toggle status LED */
        led_state = !led_state;
        gpio_pin_set_dt(&status_led, led_state);
        
        heartbeat_count++;
        
        /* Print status every 10 heartbeats */
        if ((heartbeat_count % 10) == 0) {
            printk("System Status: Active | Heartbeat: %u | Temp: %.1f°C | Readings: %u\n",
                   heartbeat_count, last_temperature, sensor_reading_count);
        }
        
        k_sleep(K_MSEC(500));
    }
    
    gpio_pin_set_dt(&status_led, 0);
    printk("Status thread terminating\n");
}

/* Sensor monitoring thread - static thread for temperature readings */
void sensor_thread_entry(void *p1, void *p2, void *p3)
{
    float temperature;
    int ret;
    
    printk("Sensor thread started (TID: %p)\n", k_current_get());
    
    while (system_active) {
        /* Read temperature sensor */
        uint8_t temp_reg = 0x00;
        uint8_t temp_data[2];
        
        ret = i2c_read_dt(&temp_sensor, temp_data, 2);
        if (ret == 0) {
            /* Convert TMP102 data to temperature */
            int16_t raw_temp = (temp_data[0] << 8) | temp_data[1];
            raw_temp >>= 4;  /* 12-bit resolution */
            temperature = raw_temp * 0.0625f;
            
            last_temperature = temperature;
            sensor_reading_count++;
            
            printk("Sensor Reading %u: %.2f°C\n", sensor_reading_count, temperature);
            
            /* Check for temperature alerts */
            if (temperature > 30.0f) {
                printk("🌡️  High temperature alert: %.1f°C\n", temperature);
            } else if (temperature < 15.0f) {
                printk("❄️  Low temperature alert: %.1f°C\n", temperature);  
            }
        } else {
            printk("Sensor read error: %d\n", ret);
        }
        
        k_sleep(K_SECONDS(2));
    }
    
    printk("Sensor thread terminating\n");
}

/* Activity simulation thread - dynamic thread creation example */
void activity_thread_entry(void *param1, void *param2, void *param3)
{
    const char *activity_name = (const char *)param1;
    uint32_t *activity_duration = (uint32_t *)param2;
    uint32_t blink_count = 0;
    
    printk("Activity thread '%s' started (TID: %p, Duration: %ums)\n", 
           activity_name, k_current_get(), *activity_duration);
    
    uint32_t start_time = k_uptime_get_32();
    uint32_t end_time = start_time + *activity_duration;
    
    while (k_uptime_get_32() < end_time && system_active) {
        /* Blink activity LED */
        gpio_pin_set_dt(&activity_led, 1);
        k_sleep(K_MSEC(50));
        gpio_pin_set_dt(&activity_led, 0);
        k_sleep(K_MSEC(100));
        
        blink_count++;
        
        if ((blink_count % 20) == 0) {
            printk("Activity '%s': %u blinks completed\n", activity_name, blink_count);
        }
    }
    
    gpio_pin_set_dt(&activity_led, 0);
    printk("Activity thread '%s' completed %u blinks in %ums\n", 
           activity_name, blink_count, k_uptime_get_32() - start_time);

    activity_thread_running = false;
}

/* Button callback for dynamic thread creation */
static struct gpio_callback button_cb_data;

void button_pressed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static struct k_thread activity_thread_data;
    static k_thread_stack_t activity_thread_stack[ACTIVITY_THREAD_STACK_SIZE];
    static uint32_t activity_duration;
    static uint32_t activity_counter = 0;
    static char activity_name[32];

    if (activity_thread_running) {
        printk("Activity thread already running.\n");
        return;
    }
    activity_thread_running = true;
    static bool activity_thread_running = false;

    if (activity_thread_running) {
        printk("Activity thread already running.\n");
        return;
    }

    activity_thread_running = true;
    
    activity_counter++;
    
    /* Generate random activity duration between 3-8 seconds */
    activity_duration = 3000 + (sys_rand32_get() % 5000);
    snprintf(activity_name, sizeof(activity_name), "Activity_%u", activity_counter);
    
    printk("Button pressed! Creating dynamic thread: %s\n", activity_name);
    
    /* Create dynamic thread */
    k_tid_t thread_id = k_thread_create(&activity_thread_data,
                                       activity_thread_stack,
                                       K_THREAD_STACK_SIZEOF(activity_thread_stack),
                                       activity_thread_entry,
                                       activity_name, &activity_duration, NULL,
                                       ACTIVITY_THREAD_PRIORITY,
                                       0,
                                       K_NO_WAIT);
    
    if (thread_id != NULL) {
        printk("Dynamic thread created successfully (TID: %p)\n", thread_id);
    } else {
        printk("Failed to create dynamic thread\n");
    }
}

/* Hardware initialization */
int init_hardware(void)
{
    int ret;
    
    /* Initialize LEDs */
    if (!gpio_is_ready_dt(&status_led)) {
        printk("Status LED device not ready\n");
        return -ENODEV;
    }
    
    if (!gpio_is_ready_dt(&activity_led)) {
        printk("Activity LED device not ready\n");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Failed to configure status LED: %d\n", ret);
        return ret;
    }
    
    ret = gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Failed to configure activity LED: %d\n", ret);
        return ret;
    }
    
    /* Initialize button */
    if (!gpio_is_ready_dt(&control_btn)) {
        printk("Control button device not ready\n");
        return -ENODEV;
    }
    
    ret = gpio_pin_configure_dt(&control_btn, GPIO_INPUT);
    if (ret < 0) {
        printk("Failed to configure control button: %d\n", ret);
        return ret;
    }
    
    ret = gpio_pin_interrupt_configure_dt(&control_btn, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Failed to configure button interrupt: %d\n", ret);
        return ret;
    }
    
    gpio_init_callback(&button_cb_data, button_pressed_callback, BIT(control_btn.pin));
    ret = gpio_add_callback_dt(&control_btn, &button_cb_data);
    if (ret < 0) {
        printk("Failed to add button callback: %d\n", ret);
        return ret;
    }
    
    /* Initialize I2C sensor */
    if (!i2c_is_ready_dt(&temp_sensor)) {
        printk("Temperature sensor device not ready\n");
        return -ENODEV;
    }
    
    printk("Hardware initialization complete\n");
    return 0;
}

/* Static thread definitions */
K_THREAD_DEFINE(status_thread_tid, STATUS_THREAD_STACK_SIZE,
               status_thread_entry, NULL, NULL, NULL,
               STATUS_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(sensor_thread_tid, SENSOR_THREAD_STACK_SIZE,
               sensor_thread_entry, NULL, NULL, NULL,
               SENSOR_THREAD_PRIORITY, 0, K_MSEC(500));

/* Main function */
int main(void)
{
    int ret;
    
    printk("\n=== Thread Creation Fundamentals Lab ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);
    printk("Kernel: Zephyr %s\n", KERNEL_VERSION_STRING);
    
    ret = init_hardware();
    if (ret < 0) {
        printk("Hardware initialization failed: %d\n", ret);
        return ret;
    }
    
    printk("Static threads started automatically\n");
    printk("Press button to create dynamic activity threads\n");
    printk("System running... (Press Ctrl+C to stop)\n\n");
    
    /* Main thread becomes monitoring thread */
    uint32_t runtime_seconds = 0;
    
    while (system_active) {
        k_sleep(K_SECONDS(1));
        runtime_seconds++;
        
        /* Print periodic system summary */
        if ((runtime_seconds % 30) == 0) {
            printk("\n--- System Summary (Runtime: %us) ---\n", runtime_seconds);
            printk("Status Thread: Running (TID: %p)\n", &status_thread_tid);
            printk("Sensor Thread: Running (TID: %p)\n", &sensor_thread_tid);
            printk("Temperature Readings: %u\n", sensor_reading_count);
            printk("Last Temperature: %.1f°C\n", last_temperature);
            printk("System Active: %s\n\n", system_active ? "Yes" : "No");
        }
        
        /* Auto-shutdown after 5 minutes for demo */
        if (runtime_seconds >= 300) {
            printk("Demo time limit reached, shutting down gracefully\n");
            system_active = false;
        }
    }
    
    printk("Main thread terminating\n");
    return 0;
}
```

### Build Configuration

Create `prj.conf`:

```ini
# Thread Creation Lab Configuration
CONFIG_GPIO=y
CONFIG_I2C=y
CONFIG_PRINTK=y
CONFIG_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y

# Thread monitoring and debugging
CONFIG_THREAD_MONITOR=y
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y

# Random number generation
CONFIG_ENTROPY_GENERATOR=y
CONFIG_TEST_RANDOM_GENERATOR=y

# Floating point support
CONFIG_NEWLIB_LIBC=y
CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3

# System
CONFIG_HEAP_MEM_POOL_SIZE=4096
CONFIG_MAIN_STACK_SIZE=2048
```

### Testing and Expected Behavior

1. **Build and flash:**

   ```bash
   west build -b rpi_4b -p auto
   west flash
   west attach
   ```

2. **Expected output:**
   * Status LED blinks every 500ms (system heartbeat)
   * Temperature readings every 2 seconds
   * Button press creates dynamic activity threads
   * Activity LED blinks rapidly during dynamic thread execution
   * System summary every 30 seconds

3. **Thread behavior verification:**
   * Multiple threads run concurrently
   * Each thread maintains independent timing
   * Dynamic threads are created and terminate properly
   * Hardware resources are shared without conflicts

---

## Lab 2: Thread Synchronization

### Learning Goals

Implement thread synchronization using mutexes, semaphores, and condition variables to coordinate access to shared resources and manage complex thread interactions.

### Synchronization Implementation

Create `src/thread_sync.c`:

```c
/*
 * Thread Synchronization Lab
 * Demonstrates mutexes, semaphores, and condition variables
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>
#include <string.h>

/* Device specifications */
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(activity_led), gpios);

/* Synchronization primitives */
static struct k_mutex data_mutex;
static struct k_sem buffer_available_sem;
static struct k_sem data_ready_sem;
static struct k_condvar processing_condition;
static struct k_mutex condition_mutex;

/* Shared data structures */
#define BUFFER_POOL_SIZE 5
#define DATA_BUFFER_SIZE 64

struct data_buffer {
    uint8_t data[DATA_BUFFER_SIZE];
    uint32_t timestamp;
    uint32_t sequence_number;
    bool valid;
};

static struct data_buffer buffer_pool[BUFFER_POOL_SIZE];
static uint32_t next_sequence_number = 1;
static uint32_t processed_count = 0;
static bool processing_enabled = true;

/* Thread parameters */
#define PRODUCER_STACK_SIZE     1024
#define CONSUMER_STACK_SIZE     1024
#define PROCESSOR_STACK_SIZE    1024
#define CONTROLLER_STACK_SIZE   512

#define PRODUCER_PRIORITY       5
#define CONSUMER_PRIORITY       6
#define PROCESSOR_PRIORITY      7
#define CONTROLLER_PRIORITY     4

/* Producer thread - generates data with mutex protection */
void data_producer_thread(void *p1, void *p2, void *p3)
{
    uint32_t producer_id = *(uint32_t *)p1;
    uint32_t production_count = 0;
    
    printk("Producer %u thread started (TID: %p)\n", producer_id, k_current_get());
    
    while (1) {
        /* Wait for available buffer */
        if (k_sem_take(&buffer_available_sem, K_SECONDS(1)) != 0) {
            printk("Producer %u: No buffer available, dropping data\n", producer_id);
            continue;
        }
        
        /* Find available buffer with mutex protection */
        k_mutex_lock(&data_mutex, K_FOREVER);
        
        struct data_buffer *buffer = NULL;
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            if (!buffer_pool[i].valid) {
                buffer = &buffer_pool[i];
                break;
            }
        }
        
        if (buffer != NULL) {
            /* Fill buffer with simulated data */
            buffer->sequence_number = next_sequence_number++;
            buffer->timestamp = k_uptime_get_32();
            
            /* Generate random data */
            for (int i = 0; i < DATA_BUFFER_SIZE; i++) {
                buffer->data[i] = sys_rand32_get() & 0xFF;
            }
            
            buffer->valid = true;
            production_count++;
            
            printk("Producer %u: Created data packet %u (seq: %u)\n", 
                   producer_id, production_count, buffer->sequence_number);
        }
        
        k_mutex_unlock(&data_mutex);
        
        /* Signal data ready */
        k_sem_give(&data_ready_sem);
        
        /* Blink status LED to show activity */
        gpio_pin_toggle_dt(&status_led);
        
        /* Variable production rate */
        uint32_t delay = 200 + (sys_rand32_get() % 800);
        k_sleep(K_MSEC(delay));
    }
}

/* Consumer thread - processes data with semaphore coordination */
void data_consumer_thread(void *p1, void *p2, void *p3)
{
    uint32_t consumer_id = *(uint32_t *)p1;
    uint32_t consumption_count = 0;
    
    printk("Consumer %u thread started (TID: %p)\n", consumer_id, k_current_get());
    
    while (1) {
        /* Wait for data to be available */
        if (k_sem_take(&data_ready_sem, K_SECONDS(2)) != 0) {
            printk("Consumer %u: No data available for processing\n", consumer_id);
            continue;
        }
        
        /* Get data buffer with mutex protection */
        k_mutex_lock(&data_mutex, K_FOREVER);
        
        struct data_buffer *buffer = NULL;
        for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
            if (buffer_pool[i].valid) {
                buffer = &buffer_pool[i];
                break;
            }
        }
        
        if (buffer != NULL) {
            /* Process data */
            uint32_t checksum = 0;
            for (int i = 0; i < DATA_BUFFER_SIZE; i++) {
                checksum += buffer->data[i];
            }
            
            uint32_t processing_time = k_uptime_get_32() - buffer->timestamp;
            consumption_count++;
            
            printk("Consumer %u: Processed packet %u (seq: %u, checksum: 0x%08x, latency: %ums)\n",
                   consumer_id, consumption_count, buffer->sequence_number, 
                   checksum, processing_time);
            
            /* Clear buffer */
            memset(buffer, 0, sizeof(struct data_buffer));
            buffer->valid = false;
        }
        
        k_mutex_unlock(&data_mutex);
        
        /* Signal buffer available */
        k_sem_give(&buffer_available_sem);
        
        /* Blink activity LED */
        gpio_pin_toggle_dt(&activity_led);
        
        /* Simulate processing time */
        k_sleep(K_MSEC(100 + (sys_rand32_get() % 200)));
    }
}

/* Data processor thread - uses condition variables for complex coordination */
void data_processor_thread(void *p1, void *p2, void *p3)
{
    uint32_t batch_size = 0;
    
    printk("Data processor thread started (TID: %p)\n", k_current_get());
    
    while (1) {
        k_mutex_lock(&condition_mutex, K_FOREVER);
        
        /* Wait for processing to be enabled */
        while (!processing_enabled) {
            printk("Data processor: Waiting for processing to be enabled\n");
            k_condvar_wait(&processing_condition, &condition_mutex, K_FOREVER);
        }
        
        /* Wait for sufficient data to process */
        while (k_sem_count_get(&data_ready_sem) < 3 && processing_enabled) {
            printk("Data processor: Waiting for batch of data (current: %u)\n", 
                   k_sem_count_get(&data_ready_sem));
            k_condvar_wait(&processing_condition, &condition_mutex, K_SECONDS(5));
        }
        
        if (!processing_enabled) {
            k_mutex_unlock(&condition_mutex);
            continue;
        }
        
        /* Process batch */
        batch_size = MIN(k_sem_count_get(&data_ready_sem), 3);
        processed_count += batch_size;
        
        printk("Data processor: Processing batch of %u items (total processed: %u)\n",
               batch_size, processed_count);
        
        k_mutex_unlock(&condition_mutex);
        
        /* Simulate batch processing */
        k_sleep(K_MSEC(500));
        
        /* Signal processing complete */
        k_condvar_broadcast(&processing_condition);
        
        k_sleep(K_SECONDS(2));
    }
}

/* Controller thread - manages system state with condition variables */
void system_controller_thread(void *p1, void *p2, void *p3)
{
    bool last_processing_state = processing_enabled;
    uint32_t state_changes = 0;
    
    printk("System controller thread started (TID: %p)\n", k_current_get());
    
    while (1) {
        /* Monitor system load */
        uint32_t pending_data = k_sem_count_get(&data_ready_sem);
        uint32_t available_buffers = k_sem_count_get(&buffer_available_sem);
        
        k_mutex_lock(&condition_mutex, K_FOREVER);
        
        /* Control processing based on system state */
        if (pending_data > 4) {
            /* Enable aggressive processing */
            if (!processing_enabled) {
                processing_enabled = true;
                state_changes++;
                printk("Controller: Enabling processing (pending: %u, available: %u)\n",
                       pending_data, available_buffers);
                k_condvar_broadcast(&processing_condition);
            }
        } else if (pending_data == 0) {
            /* Disable processing when idle */
            if (processing_enabled) {
                processing_enabled = false;
                state_changes++;
                printk("Controller: Disabling processing (system idle)\n");
                k_condvar_broadcast(&processing_condition);
            }
        }
        
        if (processing_enabled != last_processing_state) {
            last_processing_state = processing_enabled;
        }
        
        k_mutex_unlock(&condition_mutex);
        
        /* Print system status */
        if ((state_changes % 5) == 0 && state_changes > 0) {
            printk("--- System Status ---\n");
            printk("Pending Data: %u\n", pending_data);
            printk("Available Buffers: %u\n", available_buffers);
            printk("Processing Enabled: %s\n", processing_enabled ? "Yes" : "No");
            printk("Total Processed: %u\n", processed_count);
            printk("State Changes: %u\n\n", state_changes);
        }
        
        k_sleep(K_SECONDS(1));
    }
}

/* Initialize synchronization system */
int init_synchronization_system(void)
{
    /* Initialize synchronization primitives */
    k_mutex_init(&data_mutex);
    k_sem_init(&buffer_available_sem, BUFFER_POOL_SIZE, BUFFER_POOL_SIZE);
    k_sem_init(&data_ready_sem, 0, BUFFER_POOL_SIZE);
    k_condvar_init(&processing_condition);
    k_mutex_init(&condition_mutex);
    
    /* Initialize buffer pool */
    memset(buffer_pool, 0, sizeof(buffer_pool));
    
    printk("Synchronization system initialized\n");
    return 0;
}

/* Thread creation with different parameters */
int create_sync_threads(void)
{
    static struct k_thread producer1_thread, producer2_thread;
    static struct k_thread consumer1_thread, consumer2_thread;
    static struct k_thread processor_thread, controller_thread;
    
    static k_thread_stack_t producer1_stack[PRODUCER_STACK_SIZE];
    static k_thread_stack_t producer2_stack[PRODUCER_STACK_SIZE];
    static k_thread_stack_t consumer1_stack[CONSUMER_STACK_SIZE];
    static k_thread_stack_t consumer2_stack[CONSUMER_STACK_SIZE];
    static k_thread_stack_t processor_stack[PROCESSOR_STACK_SIZE];
    static k_thread_stack_t controller_stack[CONTROLLER_STACK_SIZE];
    
    static uint32_t producer1_id = 1, producer2_id = 2;
    static uint32_t consumer1_id = 1, consumer2_id = 2;
    
    /* Create producer threads */
    k_thread_create(&producer1_thread, producer1_stack,
                   K_THREAD_STACK_SIZEOF(producer1_stack),
                   data_producer_thread, &producer1_id, NULL, NULL,
                   PRODUCER_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_create(&producer2_thread, producer2_stack,
                   K_THREAD_STACK_SIZEOF(producer2_stack),
                   data_producer_thread, &producer2_id, NULL, NULL,
                   PRODUCER_PRIORITY, 0, K_MSEC(250));
    
    /* Create consumer threads */
    k_thread_create(&consumer1_thread, consumer1_stack,
                   K_THREAD_STACK_SIZEOF(consumer1_stack),
                   data_consumer_thread, &consumer1_id, NULL, NULL,
                   CONSUMER_PRIORITY, 0, K_MSEC(100));
    
    k_thread_create(&consumer2_thread, consumer2_stack,
                   K_THREAD_STACK_SIZEOF(consumer2_stack),
                   data_consumer_thread, &consumer2_id, NULL, NULL,
                   CONSUMER_PRIORITY, 0, K_MSEC(350));
    
    /* Create processor thread */
    k_thread_create(&processor_thread, processor_stack,
                   K_THREAD_STACK_SIZEOF(processor_stack),
                   data_processor_thread, NULL, NULL, NULL,
                   PROCESSOR_PRIORITY, 0, K_MSEC(500));
    
    /* Create controller thread */
    k_thread_create(&controller_thread, controller_stack,
                   K_THREAD_STACK_SIZEOF(controller_stack),
                   system_controller_thread, NULL, NULL, NULL,
                   CONTROLLER_PRIORITY, 0, K_MSEC(750));
    
    printk("All synchronization threads created\n");
    return 0;
}

int main(void)
{
    int ret;
    
    printk("\n=== Thread Synchronization Lab ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);
    
    /* Initialize hardware */
    if (!gpio_is_ready_dt(&status_led) || !gpio_is_ready_dt(&activity_led)) {
        printk("LED devices not ready\n");
        return -ENODEV;
    }
    
    gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);
    
    /* Initialize synchronization system */
    ret = init_synchronization_system();
    if (ret < 0) {
        printk("Synchronization system initialization failed: %d\n", ret);
        return ret;
    }
    
    /* Create and start threads */
    ret = create_sync_threads();
    if (ret < 0) {
        printk("Thread creation failed: %d\n", ret);
        return ret;
    }
    
    printk("System running with multiple producers, consumers, and processors\n");
    printk("Observe LED patterns and console output for synchronization behavior\n\n");
    
    /* Main thread monitors overall system health */
    uint32_t runtime_minutes = 0;
    
    while (1) {
        k_sleep(K_SECONDS(60));
        runtime_minutes++;
        
        printk("\n=== System Health Check (Runtime: %u minutes) ===\n", runtime_minutes);
        printk("Data Ready Semaphore Count: %u\n", k_sem_count_get(&data_ready_sem));
        printk("Buffer Available Semaphore Count: %u\n", k_sem_count_get(&buffer_available_sem));
        printk("Total Processed Batches: %u\n", processed_count);
        printk("Processing State: %s\n\n", processing_enabled ? "Enabled" : "Disabled");
        
        /* Auto-shutdown after demo period */
        if (runtime_minutes >= 10) {
            printk("Demo period complete, system demonstrated successfully\n");
            break;
        }
    }
    
    return 0;
}
```

### Testing Synchronization

1. **Build and run:**

   ```bash
   west build -b rpi_4b
   west flash
   west attach
   ```

2. **Observe synchronization behavior:**
   * Multiple producers create data at different rates
   * Consumers process data cooperatively using semaphores
   * Processor waits for batch conditions using condition variables
   * Controller manages system state dynamically
   * LEDs indicate thread activity patterns

3. **Key synchronization concepts demonstrated:**
   * **Mutual exclusion** with mutexes protecting shared data
   * **Resource counting** with semaphores managing buffer pool
   * **Complex conditions** with condition variables coordinating batch processing
   * **Priority inheritance** preventing priority inversion issues

---

## Lab 3: Timing and Performance Optimization

### Performance Goals

Implement precise timing control, analyze thread performance, and optimize multi-threaded applications for real-time requirements.

### Performance Analysis Implementation

Create `src/thread_performance.c`:

```c
/*
 * Thread Performance and Timing Lab
 * Demonstrates timing precision, performance monitoring, and optimization
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/timing/timing.h>
#include <zephyr/random/random.h>

/* Performance monitoring structures */
struct thread_performance_stats {
    uint32_t execution_count;
    uint32_t min_execution_time_us;
    uint32_t max_execution_time_us;
    uint32_t total_execution_time_us;
    uint32_t deadline_misses;
    uint32_t context_switches;
};

/* Thread parameters for timing tests */
#define RT_THREAD_STACK_SIZE        1024
#define MONITOR_THREAD_STACK_SIZE   1024
#define LOAD_THREAD_STACK_SIZE      512

#define RT_THREAD_PRIORITY          2   /* High priority real-time */
#define MONITOR_THREAD_PRIORITY     5   /* Normal monitoring */
#define LOAD_THREAD_PRIORITY        8   /* Background load */

/* Global performance tracking */
static struct thread_performance_stats rt_stats = {0};
static struct thread_performance_stats monitor_stats = {0};
static volatile bool performance_test_active = true;

/* Timing precision test thread */
void realtime_thread_entry(void *p1, void *p2, void *p3)
{
    uint32_t period_us = *(uint32_t *)p1;
    k_timeout_t thread_period = K_USEC(period_us);
    
    timing_t start_time, end_time;
    uint64_t execution_cycles, execution_us;
    
    printk("Real-time thread started (Period: %uµs, TID: %p)\n", 
           period_us, k_current_get());
    
    /* Initialize timing system */
    timing_init();
    timing_start();
    
    rt_stats.min_execution_time_us = UINT32_MAX;
    
    while (performance_test_active) {
        start_time = timing_counter_get();
        
        /* Simulate real-time task work */
        perform_realtime_task();
        
        end_time = timing_counter_get();
        
        /* Calculate execution time */
        execution_cycles = timing_cycles_get(&start_time, &end_time);
        execution_us = timing_cycles_to_ns(execution_cycles) / 1000;
        
        /* Update performance statistics */
        rt_stats.execution_count++;
        rt_stats.total_execution_time_us += execution_us;
        
        if (execution_us < rt_stats.min_execution_time_us) {
            rt_stats.min_execution_time_us = execution_us;
        }
        if (execution_us > rt_stats.max_execution_time_us) {
            rt_stats.max_execution_time_us = execution_us;
        }
        
        /* Check for deadline miss */
        if (execution_us > (period_us * 0.8)) {  /* 80% deadline threshold */
            rt_stats.deadline_misses++;
            printk("⚠️  Real-time thread deadline miss: %lluµs (limit: %u µs)\n", 
                   execution_us, (uint32_t)(period_us * 0.8));
        }
        
        k_sleep(thread_period);
    }
    
    timing_stop();
    printk("Real-time thread terminated\n");
}

/* Performance monitoring thread */
void performance_monitor_thread(void *p1, void *p2, void *p3)
{
    uint32_t report_interval_s = *(uint32_t *)p1;
    uint32_t report_count = 0;
    
    printk("Performance monitor thread started (Interval: %us, TID: %p)\n", 
           report_interval_s, k_current_get());
    
    while (performance_test_active) {
        timing_t start_time = timing_counter_get();
        
        /* Collect system performance data */
        collect_system_metrics();
        
        timing_t end_time = timing_counter_get();
        uint64_t monitor_cycles = timing_cycles_get(&start_time, &end_time);
        uint32_t monitor_us = timing_cycles_to_ns(monitor_cycles) / 1000;
        
        /* Update monitoring statistics */
        monitor_stats.execution_count++;
        monitor_stats.total_execution_time_us += monitor_us;
        
        if (monitor_us < monitor_stats.min_execution_time_us || 
            monitor_stats.min_execution_time_us == 0) {
            monitor_stats.min_execution_time_us = monitor_us;
        }
        if (monitor_us > monitor_stats.max_execution_time_us) {
            monitor_stats.max_execution_time_us = monitor_us;
        }
        
        report_count++;
        
        /* Generate performance report */
        if ((report_count % report_interval_s) == 0) {
            print_performance_report();
        }
        
        k_sleep(K_SECONDS(1));
    }
    
    printk("Performance monitor thread terminated\n");
}

/* System load generation thread */
void load_generation_thread(void *p1, void *p2, void *p3)
{
    uint32_t load_pattern = 0;
    
    printk("Load generation thread started (TID: %p)\n", k_current_get());
    
    while (performance_test_active) {
        load_pattern = (load_pattern + 1) % 4;
        
        switch (load_pattern) {
        case 0:
            /* Light load */
            generate_computational_load(10);
            k_sleep(K_MSEC(900));
            break;
        case 1:
            /* Medium load */
            generate_computational_load(50);
            k_sleep(K_MSEC(500));
            break;
        case 2:
            /* Heavy load */
            generate_computational_load(100);
            k_sleep(K_MSEC(200));
            break;
        case 3:
            /* Burst load */
            for (int i = 0; i < 5; i++) {
                generate_computational_load(20);
                k_sleep(K_MSEC(50));
            }
            k_sleep(K_MSEC(750));
            break;
        }
    }
    
    printk("Load generation thread terminated\n");
}

/* Real-time task simulation */
void perform_realtime_task(void)
{
    /* Simulate sensor reading and processing */
    volatile uint32_t sensor_value = sys_rand32_get();
    volatile uint32_t processed_value = 0;
    
    /* Simple processing algorithm */
    for (int i = 0; i < 100; i++) {
        processed_value += (sensor_value >> (i % 8)) & 0xFF;
    }
    
    /* Simulate control output */
    bool control_output = (processed_value > 12500);
    
    /* Update GPIO based on control decision */
    static const struct gpio_dt_spec control_led = 
        GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
    gpio_pin_set_dt(&control_led, control_output ? 1 : 0);
}

/* System metrics collection */
void collect_system_metrics(void)
{
    /* Collect various system performance metrics */
    uint32_t uptime_ms = k_uptime_get_32();
    
    /* Simulate additional metric collection */
    volatile uint32_t cpu_usage = calculate_cpu_usage();
    volatile uint32_t memory_usage = calculate_memory_usage();
    
    /* Store metrics for reporting */
    static uint32_t last_free_heap = 0;
    if (free_heap != last_free_heap) {
        last_free_heap = free_heap;
    }
}

/* Computational load generation */
void generate_computational_load(uint32_t intensity)
{
    volatile uint32_t result = 0;
    uint32_t iterations = intensity * 1000;
    
    for (uint32_t i = 0; i < iterations; i++) {
        result += i * (i % 7) + sys_rand32_get() % 100;
    }
}

/* CPU usage calculation (simplified) */
uint32_t calculate_cpu_usage(void)
{
    static uint32_t last_idle_time = 0;
    static uint32_t last_total_time = 0;
    
    uint32_t current_time = k_uptime_get_32();
    uint32_t idle_time = current_time;  /* Simplified calculation */
    
    uint32_t total_delta = current_time - last_total_time;
    uint32_t idle_delta = idle_time - last_idle_time;
    
    last_total_time = current_time;
    last_idle_time = idle_time;
    
    if (total_delta == 0) return 0;
    
    return ((total_delta - idle_delta) * 100) / total_delta;
}

/* Memory usage calculation */
uint32_t calculate_memory_usage(void)
{
    size_t free_bytes = k_mem_free_get();
    return (4096 - free_bytes) * 100 / 4096;  /* Assuming 4KB heap */
}

/* Performance report generation */
void print_performance_report(void)
{
    printk("\n=== Performance Report ===\n");
    
    /* Real-time thread statistics */
    if (rt_stats.execution_count > 0) {
        uint32_t avg_rt_time = rt_stats.total_execution_time_us / rt_stats.execution_count;
        printk("Real-time Thread:\n");
        printk("  Executions: %u\n", rt_stats.execution_count);
        printk("  Avg Time: %u µs\n", avg_rt_time);
        printk("  Min Time: %u µs\n", rt_stats.min_execution_time_us);
        printk("  Max Time: %u µs\n", rt_stats.max_execution_time_us);
        printk("  Deadline Misses: %u (%.1f%%)\n", 
               rt_stats.deadline_misses,
               (float)rt_stats.deadline_misses * 100.0f / rt_stats.execution_count);
    }
    
    /* Monitor thread statistics */
    if (monitor_stats.execution_count > 0) {
        uint32_t avg_monitor_time = monitor_stats.total_execution_time_us / monitor_stats.execution_count;
        printk("Monitor Thread:\n");
        printk("  Executions: %u\n", monitor_stats.execution_count);
        printk("  Avg Time: %u µs\n", avg_monitor_time);
        printk("  Min Time: %u µs\n", monitor_stats.min_execution_time_us);
        printk("  Max Time: %u µs\n", monitor_stats.max_execution_time_us);
    }
    
    /* System statistics */
    printk("System:\n");
    printk("  Uptime: %u seconds\n", k_uptime_get_32() / 1000);
    printk("  Est. CPU Usage: %u%%\n", calculate_cpu_usage());
    printk("  Est. Memory Usage: %u%%\n", calculate_memory_usage());
    
    printk("========================\n\n");
}

/* Thread stack monitoring */
void monitor_thread_stacks(void)
{
    printk("=== Thread Stack Usage ===\n");
    
    /* Monitor main thread stack */
    size_t main_unused = k_thread_stack_space_get(k_current_get());
    printk("Main Thread: %zu bytes unused\n", main_unused);
    
    printk("===========================\n");
}

/* Performance test threads */
static struct k_thread rt_thread_data, monitor_thread_data, load_thread_data;
static k_thread_stack_t rt_thread_stack[RT_THREAD_STACK_SIZE];
static k_thread_stack_t monitor_thread_stack[MONITOR_THREAD_STACK_SIZE];
static k_thread_stack_t load_thread_stack[LOAD_THREAD_STACK_SIZE];

int main(void)
{
    static uint32_t rt_period_us = 5000;      /* 5ms period */
    static uint32_t monitor_interval_s = 10;   /* 10s reports */
    
    printk("\n=== Thread Performance and Timing Lab ===\n");
    printk("Platform: %s\n", CONFIG_BOARD);
    printk("Testing timing precision and performance monitoring\n\n");
    
    /* Initialize GPIO for control output */
    static const struct gpio_dt_spec status_led = 
        GPIO_DT_SPEC_GET(DT_ALIAS(status_led), gpios);
    if (gpio_is_ready_dt(&status_led)) {
        gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
    }
    
    /* Create performance test threads */
    k_thread_create(&rt_thread_data, rt_thread_stack,
                   K_THREAD_STACK_SIZEOF(rt_thread_stack),
                   realtime_thread_entry, &rt_period_us, NULL, NULL,
                   RT_THREAD_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_create(&monitor_thread_data, monitor_thread_stack,
                   K_THREAD_STACK_SIZEOF(monitor_thread_stack),
                   performance_monitor_thread, &monitor_interval_s, NULL, NULL,
                   MONITOR_THREAD_PRIORITY, 0, K_MSEC(500));
    
    k_thread_create(&load_thread_data, load_thread_stack,
                   K_THREAD_STACK_SIZEOF(load_thread_stack),
                   load_generation_thread, NULL, NULL, NULL,
                   LOAD_THREAD_PRIORITY, 0, K_MSEC(1000));
    
    printk("Performance test threads created and started\n");
    printk("Monitor console output for timing and performance data\n\n");
    
    /* Main thread supervises test execution */
    uint32_t test_duration_minutes = 5;
    uint32_t elapsed_minutes = 0;
    
    while (elapsed_minutes < test_duration_minutes) {
        k_sleep(K_SECONDS(60));
        elapsed_minutes++;
        
        printk("--- Test Progress: %u/%u minutes ---\n", 
               elapsed_minutes, test_duration_minutes);
        
        /* Monitor stack usage */
        monitor_thread_stacks();
        
        /* Print intermediate summary */
        print_performance_report();
    }
    
    printk("Performance test completed, stopping threads...\n");
    performance_test_active = false;
    
    /* Wait for threads to terminate gracefully */
    k_sleep(K_SECONDS(2));
    
    /* Final performance report */
    printk("\n=== FINAL PERFORMANCE REPORT ===\n");
    print_performance_report();
    
    printk("Thread performance lab completed successfully\n");
    return 0;
}
```

### Advanced Performance Configuration

Update `prj.conf` for performance testing:

```ini
# Thread Performance Lab Configuration
CONFIG_GPIO=y
CONFIG_PRINTK=y
CONFIG_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y

# Thread monitoring and debugging
CONFIG_THREAD_MONITOR=y
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_RUNTIME_STATS=y

# Timing and performance
CONFIG_TIMING_FUNCTIONS=y
CONFIG_TIMING_FUNCTIONS_NEED_AT_BOOT=y
CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000

# High precision timing
CONFIG_COUNTER=y
CONFIG_SYSTEM_CLOCK_SLOPPY_IDLE=n

# Memory management
CONFIG_HEAP_MEM_POOL_SIZE=8192
CONFIG_MAIN_STACK_SIZE=4096

# Random number generation
CONFIG_ENTROPY_GENERATOR=y
CONFIG_TEST_RANDOM_GENERATOR=y

# Floating point support
CONFIG_NEWLIB_LIBC=y
CONFIG_NEWLIB_LIBC_FLOAT_PRINTF=y

# Optimization
CONFIG_SPEED_OPTIMIZATIONS=y
CONFIG_COMPILER_OPT="-O2"

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

### Performance Analysis Results

Expected performance characteristics when running the lab:

1. **Real-time Thread Performance:**

   ```text
   Real-time Thread:
     Executions: 1200
     Avg Time: 245 µs
     Min Time: 180 µs
     Max Time: 890 µs
     Deadline Misses: 3 (0.3%)
   ```

2. **System Load Impact:**
   * Light load: Consistent timing, minimal jitter
   * Medium load: Slight increase in execution time
   * Heavy load: Increased jitter, potential deadline misses
   * Burst load: Periodic timing variations

3. **Thread Synchronization Overhead:**
   * Mutex operations: ~5-10µs overhead
   * Semaphore operations: ~3-7µs overhead
   * Condition variables: ~8-15µs overhead

---

## Lab Summary

Through these comprehensive labs, you've mastered:

**Thread Creation and Management:**

* Static thread definition using K_THREAD_DEFINE
* Dynamic thread creation with k_thread_create
* Thread lifecycle management and graceful termination
* Parameter passing and thread communication

**Synchronization Mechanisms:**

* Mutual exclusion with mutexes for resource protection
* Resource counting and signaling with semaphores
* Complex coordination using condition variables
* Priority inheritance and deadlock prevention

**Timing and Performance:**

* Precise timing control with k_sleep, k_msleep, and k_usleep
* High-precision timing using the timing API
* Performance monitoring and optimization techniques
* Real-time constraint handling and deadline management

**Professional Practices:**

* Stack size optimization and monitoring
* Priority assignment strategies
* Error handling and recovery patterns
* Performance analysis and system tuning

These skills provide the foundation for building sophisticated multi-threaded embedded applications that meet professional real-time system requirements.

[Next: Chapter 8 - Tracing and Logging](../chapter_08_tracing_and_logging/README.md)
