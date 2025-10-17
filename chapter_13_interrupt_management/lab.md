# Chapter 13 - Interrupt Management Lab

## Lab Overview

This lab provides hands-on experience implementing interrupt handling, workqueues, and their interaction in Zephyr. You'll build a complete interrupt-driven system that demonstrates real-world embedded programming patterns.

## Learning Objectives

By completing this lab, you will:

* Implement basic interrupt handlers using `IRQ_CONNECT()`
* Create handler threads for processing interrupt data
* Use workqueues for deferred interrupt processing
* Apply interrupt-safe APIs correctly
* Debug and optimize interrupt-driven systems

## Lab Setup

### Required Hardware

* Zephyr-supported development board (Nordic nRF52840-DK, STM32 Nucleo, or similar)
* LED connected to GPIO pin
* Push button connected to GPIO pin (with pull-up resistor)
* Logic analyzer or oscilloscope (optional, for timing analysis)

### Project Structure

```
interrupt_lab/
├── CMakeLists.txt
├── prj.conf
├── src/
│   ├── main.c
│   ├── interrupt_handler.c
│   └── interrupt_handler.h
└── boards/
    └── [board_name].overlay
```

## Part 1: Basic Interrupt Setup (30 minutes)

### Step 1: Project Configuration

Create `prj.conf`:

```conf
CONFIG_GPIO=y
CONFIG_PRINTK=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_KERNEL_LOG_LEVEL=3
CONFIG_ASSERT=y
```

### Step 2: Device Tree Overlay

Create `boards/nrf52840dk_nrf52840.overlay` (adjust for your board):

```dts
/ {
    buttons {
        compatible = "gpio-keys";
        button0: button_0 {
            gpios = <&gpio0 11 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
            label = "Push button switch 0";
        };
    };
    
    leds {
        compatible = "gpio-leds";
        led0: led_0 {
            gpios = <&gpio0 13 GPIO_ACTIVE_LOW>;
            label = "Green LED 0";
        };
    };
};
```

### Step 3: Basic Interrupt Handler

Create `src/interrupt_handler.h`:

```c
#ifndef INTERRUPT_HANDLER_H
#define INTERRUPT_HANDLER_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Button and LED definitions */
#define BUTTON_NODE DT_ALIAS(sw0)
#define LED_NODE    DT_ALIAS(led0)

/* Function declarations */
int setup_gpio_interrupt(void);
void gpio_interrupt_handler(const struct device *dev, 
                           struct gpio_callback *cb, 
                           uint32_t pins);

/* Global semaphore for signaling */
extern struct k_sem button_pressed_sem;

#endif /* INTERRUPT_HANDLER_H */
```

Create `src/interrupt_handler.c`:

```c
#include "interrupt_handler.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(interrupt_handler, LOG_LEVEL_DBG);

/* Global semaphore */
K_SEM_DEFINE(button_pressed_sem, 0, 1);

/* GPIO callback structure */
static struct gpio_callback button_cb_data;

/* GPIO devices */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE);

void gpio_interrupt_handler(const struct device *dev, 
                           struct gpio_callback *cb, 
                           uint32_t pins)
{
    LOG_INF("Button interrupt triggered on pin %d", pins);
    
    /* Signal the main thread - interrupt-safe operation */
    k_sem_give(&button_pressed_sem);
}

int setup_gpio_interrupt(void)
{
    int ret;
    
    if (!device_is_ready(button.port) || !device_is_ready(led.port)) {
        LOG_ERR("GPIO devices not ready");
        return -ENODEV;
    }
    
    /* Configure button as input with interrupt */
    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure button pin: %d", ret);
        return ret;
    }
    
    /* Configure LED as output */
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin: %d", ret);
        return ret;
    }
    
    /* Configure interrupt */
    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure interrupt: %d", ret);
        return ret;
    }
    
    /* Initialize callback */
    gpio_init_callback(&button_cb_data, gpio_interrupt_handler, 
                      BIT(button.pin));
    
    /* Add callback */
    ret = gpio_add_callback(button.port, &button_cb_data);
    if (ret < 0) {
        LOG_ERR("Failed to add callback: %d", ret);
        return ret;
    }
    
    LOG_INF("GPIO interrupt setup complete");
    return 0;
}
```

### Step 4: Main Application

Create `src/main.c`:

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "interrupt_handler.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* LED toggle function */
void toggle_led(void)
{
    static bool led_state = false;
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE);
    
    led_state = !led_state;
    gpio_pin_set_dt(&led, led_state);
    
    LOG_INF("LED toggled: %s", led_state ? "ON" : "OFF");
}

int main(void)
{
    int ret;
    
    LOG_INF("Starting Interrupt Management Lab");
    
    /* Setup GPIO interrupt */
    ret = setup_gpio_interrupt();
    if (ret < 0) {
        LOG_ERR("Failed to setup GPIO interrupt: %d", ret);
        return ret;
    }
    
    LOG_INF("Press the button to trigger interrupt");
    
    /* Main loop - wait for interrupts */
    while (1) {
        /* Wait for button press interrupt */
        ret = k_sem_take(&button_pressed_sem, K_FOREVER);
        if (ret == 0) {
            LOG_INF("Button press detected in main thread");
            toggle_led();
            
            /* Add debounce delay */
            k_msleep(200);
        }
    }
    
    return 0;
}
```

### Verification for Part 1

1. Build and flash the application
2. Press the button and verify:
   * Console shows interrupt messages
   * LED toggles on each button press
   * No system crashes or unexpected behavior

## Part 2: Handler Thread Implementation (30 minutes)

### Step 5: Add Handler Thread

Add to `src/interrupt_handler.c`:

```c
/* Handler thread stack and priority */
#define HANDLER_STACK_SIZE 1024
#define HANDLER_PRIORITY 7

/* Handler thread function */
void handler_thread_func(void *arg1, void *arg2, void *arg3)
{
    int count = 0;
    
    LOG_INF("Handler thread started");
    
    while (1) {
        /* Wait for interrupt signal */
        k_sem_take(&button_pressed_sem, K_FOREVER);
        
        count++;
        LOG_INF("Handler thread processing interrupt #%d", count);
        
        /* Simulate complex processing */
        k_msleep(100);
        
        /* Perform LED operations */
        toggle_led();
        
        LOG_INF("Handler thread completed processing #%d", count);
    }
}

/* Define handler thread */
K_THREAD_DEFINE(handler_thread, HANDLER_STACK_SIZE, handler_thread_func,
                NULL, NULL, NULL, HANDLER_PRIORITY, 0, 0);
```

Update `main.c` to remove LED toggling (now handled by handler thread):

```c
int main(void)
{
    int ret;
    
    LOG_INF("Starting Interrupt Management Lab - Handler Thread Version");
    
    /* Setup GPIO interrupt */
    ret = setup_gpio_interrupt();
    if (ret < 0) {
        LOG_ERR("Failed to setup GPIO interrupt: %d", ret);
        return ret;
    }
    
    LOG_INF("Handler thread will process interrupts");
    LOG_INF("Press the button to trigger interrupt");
    
    /* Main thread can do other work */
    while (1) {
        LOG_INF("Main thread is running...");
        k_msleep(5000); /* 5 second heartbeat */
    }
    
    return 0;
}
```

### Verification for Part 2

1. Build and flash the updated application
2. Verify that:
   * Handler thread processes interrupts independently
   * Main thread continues running (heartbeat messages)
   * System responds promptly to button presses

## Part 3: Workqueue Implementation (30 minutes)

### Step 6: Add Workqueue Processing

Add to `src/interrupt_handler.c`:

```c
/* Work item for deferred processing */
static struct k_work button_work;
static struct k_work_delayable delayed_work;

/* Work handler function */
void button_work_handler(struct k_work *work)
{
    static int work_count = 0;
    work_count++;
    
    LOG_INF("Workqueue processing button press #%d", work_count);
    
    /* Perform LED toggle */
    toggle_led();
    
    /* Schedule delayed work */
    k_work_schedule(&delayed_work, K_MSEC(1000));
}

/* Delayed work handler */
void delayed_work_handler(struct k_work *work)
{
    LOG_INF("Delayed work executed - turning off LED");
    
    /* Turn off LED */
    const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE);
    gpio_pin_set_dt(&led, 0);
}

/* Updated interrupt handler */
void gpio_interrupt_handler(const struct device *dev, 
                           struct gpio_callback *cb, 
                           uint32_t pins)
{
    LOG_INF("Button interrupt - submitting to workqueue");
    
    /* Submit work to system workqueue - interrupt-safe */
    k_work_submit(&button_work);
}

/* Initialize work items */
void init_workqueue(void)
{
    k_work_init(&button_work, button_work_handler);
    k_work_init_delayable(&delayed_work, delayed_work_handler);
    
    LOG_INF("Workqueue initialized");
}
```

Update header file and main function to call `init_workqueue()`.

### Verification for Part 3

1. Build and flash the workqueue version
2. Press button and verify:
   * Immediate LED toggle
   * LED turns off after 1 second delay
   * Multiple rapid button presses are handled correctly

## Part 4: Performance Analysis and Optimization (20 minutes)

### Step 7: Add Timing Measurements

Add timing measurement to interrupt handler:

```c
#include <zephyr/sys/time_units.h>

void gpio_interrupt_handler(const struct device *dev, 
                           struct gpio_callback *cb, 
                           uint32_t pins)
{
    static uint32_t last_time = 0;
    uint32_t current_time = k_uptime_get_32();
    
    /* Calculate time since last interrupt */
    if (last_time != 0) {
        uint32_t delta = current_time - last_time;
        LOG_INF("Time since last interrupt: %u ms", delta);
    }
    
    last_time = current_time;
    
    /* Submit work to workqueue */
    k_work_submit(&button_work);
}
```

### Step 8: Stress Testing

Add stress test functionality:

```c
/* Stress test work item */
static struct k_work stress_work;

void stress_work_handler(struct k_work *work)
{
    /* Simulate heavy processing */
    for (int i = 0; i < 1000; i++) {
        k_busy_wait(100); /* 100 microseconds */
    }
    
    LOG_INF("Stress work completed");
}

/* Submit stress work periodically */
void submit_stress_work(void)
{
    k_work_submit(&stress_work);
}
```

## Lab Exercises

### Exercise 1: Multiple Interrupt Sources
Add a second button and handle multiple interrupt sources.

### Exercise 2: Custom Workqueue
Implement a custom workqueue with different priority than system workqueue.

### Exercise 3: Interrupt Statistics
Track interrupt frequency and processing times.

### Exercise 4: Error Handling
Add comprehensive error handling for all interrupt operations.

## Troubleshooting Guide

| Issue | Possible Cause | Solution |
|-------|----------------|----------|
| No interrupt triggered | GPIO not configured correctly | Check device tree and pin configuration |
| System crashes in ISR | Using blocking API in ISR | Use only interrupt-safe APIs |
| Missed interrupts | ISR taking too long | Move processing to workqueue |
| LED not toggling | GPIO output not configured | Verify LED GPIO configuration |
| Build errors | Missing dependencies | Check prj.conf configuration |

## Expected Outcomes

After completing this lab, you should have:

1. A working interrupt-driven system
2. Understanding of ISR vs. handler thread trade-offs
3. Experience with workqueue-based deferred processing
4. Knowledge of interrupt-safe API usage
5. Skills in debugging interrupt-driven systems

## Advanced Challenges

1. **Multi-level Interrupts**: Implement nested interrupt handling
2. **Real-time Constraints**: Add timing constraints and verify they're met
3. **Power Management**: Integrate interrupt handling with low-power modes
4. **Hardware Integration**: Connect to real sensors and actuators

This comprehensive lab provides practical experience with all aspects of interrupt management in Zephyr, preparing you for real-world embedded system development.