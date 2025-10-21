# Chapter 2: Introduction to Zephyr - Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

Now that you understand Zephyr's concepts and architecture, let's get hands-on experience with setting up a development environment and building your first applications.

---

## Lab Exercise 1: Development Environment Setup

### Objective

Set up a complete Zephyr development environment and verify it works by building a sample application.

### Prerequisites

* Computer running Linux, macOS, or Windows
* Internet connection for downloading dependencies
* At least 2GB free disk space

### Step 1: Install VS Code and Zephyr Extension

1. **Download and install VS Code** from [code.visualstudio.com](https://code.visualstudio.com/)

2. **Install the Zephyr extension:**
   * Open VS Code
   * Go to Extensions (Ctrl+Shift+X)
   * Search for "Zephyr"
   * Install the official Zephyr extension by Zephyr Project

### Step 2: Install Zephyr SDK

   * Open Command Palette (Ctrl+Shift+P)
   * Type "Zephyr: Install SDK" and select it
   * Follow the guided installation process

#### Manual SDK Installation (Alternative)

If you prefer to install the SDK manually instead of using the VS Code extension's guided process, you can follow these steps:

1.  **Navigate to the official [Zephyr SDK Releases page](https://github.com/zephyrproject-rtos/sdk-ng/releases).**
2.  Look for the SDK version that matches the version used in this course, which is **0.17.4**.
3.  Download the `zephyr-sdk-*.tar.xz` archive for your operating system.
4.  Extract the archive to a suitable location, for example, `~/zephyr-sdk`.
5.  Run the SDK's setup script:

```bash
cd ~/zephyr-sdk
./setup.sh
```

### Step 3: Verify Installation

Test your setup by building a sample application:

```bash
cd ~/zephyrproject
west build -b qemu_x86 zephyr/samples/hello_world
west build -t run
```

**Expected Output:**

```console
*** Booting Zephyr OS build v4.2.99 ***
Hello World! qemu_x86
```

**Note on Versioning:** The version number `v4.2.99` indicates a development version of Zephyr. A `.99` patch level is often used for builds from the main development branch rather than a stable release.

If you see this output, your development environment is working correctly!

---

## Lab Exercise 2: Hello World for Raspberry Pi 4B

### Objective

Build and deploy your first application specifically for Raspberry Pi 4B hardware.

### Prerequisites

* Completed Lab Exercise 1
* Raspberry Pi 4B board
* MicroSD card (8GB or larger)
* USB-to-UART adapter for console output

### Step 1: Build for Raspberry Pi 4B

```bash
cd ~/zephyrproject
west build -b rpi_4b zephyr/samples/hello_world --pristine
```

The `--pristine` flag ensures a clean build.

### Step 2: Prepare SD Card

1. **Format SD card** with FAT32 file system
2. **Copy the kernel image:**

   ```bash
   cp build/zephyr/zephyr.bin /path/to/sdcard/kernel8.img
   ```

3. **Add minimal config.txt** to SD card:

   ```bash
   arm_64bit=1
   kernel=kernel8.img
   ```

### Step 3: Connect Serial Console

1. **Connect USB-to-UART adapter:**
   * GND → Pin 6 (Ground)
   * RX → Pin 8 (GPIO 14, UART TX)
   * TX → Pin 10 (GPIO 15, UART RX)

2. **Open terminal emulator:**

   ```bash
   # Linux/macOS
   screen /dev/ttyUSB0 115200

   # Or use minicom
   minicom -D /dev/ttyUSB0 -b 115200
   ```

### Step 4: Boot and Verify

1. Insert SD card into Raspberry Pi 4B
2. Power on the board
3. You should see output in your terminal:

```console
*** Booting Zephyr OS build v4.2.99 ***
Hello World! rpi_4b
```

**Troubleshooting:**

* **No output:** Check UART connections and baud rate
* **Boot failure:** Verify SD card formatting and file names
* **Build errors:** Ensure you have the latest Zephyr SDK

---

## Lab Exercise 3: Blinking LED

### Objective
Create an interactive application that controls hardware (LED) and demonstrates Zephyr's GPIO API.

### Step 1: Examine the Blinky Sample

```bash
cd ~/zephyrproject
cat zephyr/samples/basic/blinky/src/main.c
```

**Key concepts in the code:**
* Device tree integration with `DT_ALIAS(led0)`
* GPIO driver API usage
* Zephyr kernel services (`k_msleep`)

### Step 2: Build and Test Blinky

**For Raspberry Pi 4B:**
```bash
west build -b rpi_4b zephyr/samples/basic/blinky --pristine
cp build/zephyr/zephyr.bin /path/to/sdcard/kernel8.img
```

**For QEMU (testing without hardware):**
```bash
west build -b qemu_x86 zephyr/samples/basic/blinky --pristine
west build -t run
```

### Step 3: Customize the Blink Rate

1. **Create a custom application directory:**
   ```bash
   mkdir ~/zephyrproject/my_blinky
   cd ~/zephyrproject/my_blinky
   ```

2. **Create CMakeLists.txt:**
   ```cmake
   cmake_minimum_required(VERSION 3.20.0)
   find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
   project(my_blinky)
   
   target_sources(app PRIVATE src/main.c)
   ```

3. **Create prj.conf:**
   ```
   CONFIG_GPIO=y
   ```

4. **Create src/main.c with custom timing:**
   ```c
   #include <stdio.h>
   #include <zephyr/kernel.h>
   #include <zephyr/drivers/gpio.h>
   #include <zephyr/logging/log.h>

   LOG_MODULE_REGISTER(main);

   #define FAST_BLINK_MS   250    // Fast blink mode
   #define SLOW_BLINK_MS   1500   // Slow blink mode
   #define LED0_NODE DT_ALIAS(led0)
   
   static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
   
   int main(void)
   {
       int ret;
       bool led_state = true;
       bool fast_mode = true;
       int blink_count = 0;
   
       LOG_INF("Custom Blinky Application Starting");
   
       if (!gpio_is_ready_dt(&led)) {
           LOG_ERR("Error: LED device not ready");
           return 0;
       }
   
       ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
       if (ret < 0) {
           LOG_ERR("Error: Failed to configure LED pin");
           return 0;
       }
   
       while (1) {
           // Toggle LED
           ret = gpio_pin_toggle_dt(&led);
           if (ret < 0) {
               LOG_ERR("Error: Failed to toggle LED");
               return 0;
           }
   
           led_state = !led_state;
           LOG_INF("LED %s (blink #%d)", led_state ? "ON " : "OFF", ++blink_count);
   
           // Switch between fast and slow blinking every 10 blinks
           if (blink_count % 10 == 0) {
               fast_mode = !fast_mode;
               LOG_INF("Switching to %s mode", fast_mode ? "FAST" : "SLOW");
           }
   
           k_msleep(fast_mode ? FAST_BLINK_MS : SLOW_BLINK_MS);
       }
       return 0;
   }
   ```

5. **Build and test:**
   ```bash
   west build -b rpi_4b . --pristine
   ```

### Step 4: Add Button Control (Challenge)

**For boards with buttons (like nRF52840 DK):**

1. **Update prj.conf:**
   ```
   CONFIG_GPIO=y
   ```

2. **Modify main.c to include button handling:**
   ```c
   #define BUTTON0_NODE DT_ALIAS(sw0)
   static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
   static struct gpio_callback button_cb_data;

   void button_pressed(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins)
   {
       printf("Button pressed! Toggling LED mode\n");
       // Add your button handling logic here
   }
   ```

**Note:** This challenge requires an understanding of interrupts and callbacks, which will be covered in detail in later chapters. The `DT_ALIAS(sw0)` macro also requires a `sw0` alias to be defined in your board's device tree file. For example:

```dts
/ {
    aliases {
        sw0 = &button0;
    };
};
```

---

## Lab Exercise 4: Exploring Configuration Options

### Objective
Learn to configure Zephyr applications using Kconfig and understand the impact of different settings.

### Step 1: Interactive Configuration

```bash
cd ~/zephyrproject/my_blinky
west build -t menuconfig
```

This opens an interactive menu where you can:

* Browse available configuration options
* Enable/disable features
* See help text for each option
* Save your configuration

### Step 2: Common Configuration Experiments

**Enable Logging:**
Add to prj.conf:

```
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
```

Update your code to use logging:
```c
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

// In main function:
LOG_INF("Application starting");
LOG_DBG("LED state changed to %s", led_state ? "ON" : "OFF");
```

**Adjust Stack Sizes:**

```
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=1024
```

**Enable Shell Interface:**

```
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
```

### Step 3: Memory Usage Analysis

**Check memory usage:**

```bash
west build -t ram_report
west build -t rom_report
```

The memory usage report is automatically generated every time you build your application. You can find this information in the build output, typically near the end. Look for a section that shows the memory usage for different regions like Flash and RAM.

**Optimize for size:**

```
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_DEBUG=n
CONFIG_ASSERT=n
```

---

## Lab Summary

### What You've Accomplished

1. **Development Environment:** Set up VS Code with Zephyr extension or command-line tools
2. **First Application:** Built and ran Hello World on both QEMU and real hardware
3. **Hardware Interaction:** Controlled GPIO pins and LEDs using Zephyr's device driver API
4. **Custom Application:** Created your own application with custom timing and logging
5. **Configuration:** Learned to use Kconfig to customize Zephyr features

### Key Takeaways

* **Zephyr's Power:** Even simple applications demonstrate real-time capabilities
* **Hardware Abstraction:** Same code works across different boards with device tree
* **Configuration Flexibility:** Kconfig allows fine-tuning for your specific needs
* **Development Workflow:** West provides a streamlined build and deployment process

### Next Steps

You now have hands-on experience with Zephyr development. In Chapter 3, we'll dive deeper into the build system, explore advanced West features, and learn about project structure and organization.

**Continue practicing by:**

* Trying different boards and comparing the differences
* Exploring other samples in the zephyr/samples directory
* Experimenting with different Kconfig options
* Adding more complex GPIO interactions (multiple LEDs, buttons, etc.)

[Next: Chapter 3: Zephyr Build System](../chapter_03_zephyr_build_system/README.md)
