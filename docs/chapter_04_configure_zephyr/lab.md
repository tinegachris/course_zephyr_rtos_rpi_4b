# Chapter 4: Configure Zephyr - Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

This lab provides hands-on experience with Zephyr configuration management. You'll create different configuration profiles, work with device tree overlays, and optimize settings for various scenarios using your Raspberry Pi 4B.

---

## Prerequisites and Setup

Before starting this lab, ensure you have:

- **Hardware:** Raspberry Pi 4B with SD card and power supply
- **Development Environment:** VS Code with Zephyr extension, West workspace at `~/zephyrproject`
- **Completed:** Chapters 1-3 of this course
- **Optional Hardware:** LED, button, breadboard, and jumper wires for advanced exercises

**Verify Your Environment:**

```bash
# Verify West installation and workspace
cd ~/zephyrproject
west --version
west list

# Verify Zephyr SDK
echo $ZEPHYR_SDK_INSTALL_DIR
ls $ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/

# Test basic build capability
cd ~/zephyrproject/zephyr/samples/hello_world
west build -b rpi_4b --pristine
```

---

## Lab Exercise 1: Basic Configuration Management

### Objective
Create and manage multiple configuration profiles for different development scenarios.

### Step 1: Create a Base Application

**Create the project structure:**

```bash
cd ~/zephyrproject
mkdir config_lab
cd config_lab

# Create basic application files
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(config_lab)

target_sources(app PRIVATE src/main.c)
EOF

mkdir src
cat > src/main.c << 'EOF'
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

/**
 * @brief Note on GPIO_DT_SPEC_GET_OR
 *
 * The use of GPIO_DT_SPEC_GET_OR() is a convenient way to provide a fallback
 * if a device tree alias (like 'led0') is not defined. This prevents a
 * build failure.
 *
 * For more robust code, you could use a compile-time check:
 *
 * #if DT_NODE_HAS_STATUS(LED_NODE, okay)
 * static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
 * #else
 * #error "Unsupported board: led0 devicetree alias is not defined"
 * #endif
 *
 * This note is for educational purposes; the lab code will use the more
 * flexible `_GET_OR` variant to allow the application to run even without
all devices present.
 */

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED_NODE, gpios, {0});
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(BUTTON_NODE, gpios, {0});

static struct gpio_callback button_cb_data;
static bool led_state = false;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    led_state = !led_state;
    gpio_pin_set_dt(&led, led_state);
    LOG_INF("Button pressed, LED %s", led_state ? "ON" : "OFF");
}

int main(void)
{
    int ret;
    
    LOG_INF("Configuration Lab Application Starting");
    LOG_INF("Zephyr Version: %s", KERNEL_VERSION_STRING);
    
    // Initialize LED
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED device not ready");
        return -1;
    }
    
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin: %d", ret);
        return -1;
    }
    
    // Initialize button
    if (!gpio_is_ready_dt(&button)) {
        LOG_WRN("Button device not ready - continuing without button support");
    } else {
        ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
        if (ret < 0) {
            LOG_ERR("Failed to configure button pin: %d", ret);
        } else {
            ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
            if (ret < 0) {
                LOG_ERR("Failed to configure button interrupt: %d", ret);
            } else {
                gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
                gpio_add_callback(button.dt_spec->port, &button_cb_data);
                LOG_INF("Button configured successfully");
            }
        }
    }
    
    // Main application loop
    while (1) {
        k_sleep(K_SECONDS(2));
        
        if (!gpio_is_ready_dt(&button)) {
            // Toggle LED automatically if no button
            led_state = !led_state;
            gpio_pin_set_dt(&led, led_state);
            LOG_INF("Auto-toggle LED %s", led_state ? "ON" : "OFF");
        }
        
        LOG_DBG("Application heartbeat");
    }
    
    return 0;
}
EOF
```

### Step 2: Create Base Configuration

**Create the base configuration:**

```bash
cat > prj.conf << 'EOF'
# Base Configuration for Config Lab
# This configuration includes essential features for all builds

# System Configuration
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_IDLE_STACK_SIZE=512
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=1024

# GPIO Support
CONFIG_GPIO=y

# Basic Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Console Support
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_SERIAL=y
CONFIG_PRINTK=y

# Enable assertion checking
CONFIG_ASSERT=y
EOF
```

### Step 3: Create Configuration Fragments

**Create debugging configuration fragment:**

```bash
mkdir fragments
cat > fragments/debug.conf << 'EOF'
# Debug Configuration Fragment
# Enables comprehensive debugging features

# Enhanced Logging
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_LOG_BACKEND_UART=y
CONFIG_LOG_PROCESS_THREAD_STACK_SIZE=2048

# Debug Features
CONFIG_DEBUG=y
CONFIG_DEBUG_OPTIMIZATIONS=y
CONFIG_DEBUG_THREAD_INFO=y

# Thread Analysis
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_NAME=y
CONFIG_THREAD_STACK_INFO=y

# Stack Protection
CONFIG_STACK_SENTINEL=y

# Kernel Debug
CONFIG_KERNEL_DEBUG=y
CONFIG_INIT_STACKS=y

# Increase stack sizes for debug overhead
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
EOF
```

**Create production configuration fragment:**

```bash
cat > fragments/release.conf << 'EOF'
# Release Configuration Fragment
# Optimized for production deployment

# Minimal Logging
CONFIG_LOG_DEFAULT_LEVEL=2
CONFIG_LOG_MODE_MINIMAL=y

# Size Optimizations
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_LTO=y
CONFIG_COMPILER_OPT="-Os"

# Disable Debug Features
CONFIG_DEBUG=n
CONFIG_ASSERT=n
CONFIG_THREAD_NAME=n
CONFIG_THREAD_STACK_INFO=n
CONFIG_INIT_STACKS=n
CONFIG_BOOT_BANNER=n

# Minimal Stack Sizes
CONFIG_MAIN_STACK_SIZE=1536
CONFIG_IDLE_STACK_SIZE=320
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=768

# Disable Unused Features
CONFIG_CONSOLE_SUBSYS=n
CONFIG_SHELL=n
CONFIG_THREAD_ANALYZER=n
EOF
```

**Create networking configuration fragment:**

```bash
cat > fragments/networking.conf << 'EOF'
# Networking Configuration Fragment
# Enables basic networking stack

# Core Networking
CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y

# Network Buffer Configuration
CONFIG_NET_PKT_RX_COUNT=10
CONFIG_NET_PKT_TX_COUNT=10
CONFIG_NET_BUF_RX_COUNT=20
CONFIG_NET_BUF_TX_COUNT=20
CONFIG_NET_BUF_DATA_SIZE=128

# DHCP Client
CONFIG_NET_DHCPV4=y

# DNS Resolution
CONFIG_DNS_RESOLVER=y
CONFIG_DNS_RESOLVER_MAX_SERVERS=2

# Network Shell (for debugging)
CONFIG_NET_SHELL=y
CONFIG_SHELL=y

# Additional stack space for networking
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=4096
CONFIG_NET_TX_STACK_SIZE=1536
CONFIG_NET_RX_STACK_SIZE=1536
EOF
```

### Step 4: Test Configuration Profiles

**Build with debug configuration:**

```bash
# Debug build
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/debug.conf"

# Check the generated configuration
cat build/zephyr/.config | grep -E "CONFIG_LOG_DEFAULT_LEVEL|CONFIG_DEBUG|CONFIG_MAIN_STACK_SIZE"

# Check binary size
ls -la build/zephyr/zephyr.elf
```

**Build with release configuration:**

```bash
# Release build
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/release.conf"

# Compare configuration
cat build/zephyr/.config | grep -E "CONFIG_LOG_DEFAULT_LEVEL|CONFIG_DEBUG|CONFIG_MAIN_STACK_SIZE"

# Compare binary size
ls -la build/zephyr/zephyr.elf
```

**Build with networking enabled:**

```bash
# Networking build
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/debug.conf fragments/networking.conf"

# Check networking configuration
cat build/zephyr/.config | grep -E "CONFIG_NETWORKING|CONFIG_NET_IPV4"

# Check binary size impact
ls -la build/zephyr/zephyr.elf
```

---

## Lab Exercise 2: Device Tree Customization

### Objective
Create device tree overlays to customize hardware configurations for the Raspberry Pi 4B.

### Step 1: Examine Base Device Tree

**Explore the Raspberry Pi 4B device tree:**

```bash
# View the base board device tree
cd ~/zephyrproject/zephyr
find . -name "rpi_4b*" -type f | grep -E "\.(dts|dtsi)$"

# Examine the main board file
cat boards/raspberrypi/rpi_4b/rpi_4b.dts

# Look at GPIO definitions
grep -A 10 -B 5 "gpio" boards/raspberrypi/rpi_4b/rpi_4b.dts

# Check available aliases
grep -A 20 "aliases" boards/raspberrypi/rpi_4b/rpi_4b.dts
```

### Step 2: Create Hardware Overlay

**Create an overlay for custom GPIO assignments:**

```bash
cd ~/zephyrproject/config_lab

cat > boards/rpi_4b.overlay << 'EOF'
/*
 * Custom hardware configuration for Raspberry Pi 4B
 * Adds LED and button support for our application
 */

/ {
    aliases {
        led0 = &status_led;
        sw0 = &user_button;
    };

    leds {
        compatible = "gpio-leds";
        
        status_led: led_0 {
            gpios = <&gpio0 18 GPIO_ACTIVE_HIGH>; // GPIO18 (Pin 12)
            label = "Status LED";
        };
    };

    buttons {
        compatible = "gpio-keys";
        
        user_button: button_0 {
            gpios = <&gpio0 21 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>; // GPIO21 (Pin 40)
            label = "User Button";
        };
    };
};

// Enable additional GPIO features
&gpio0 {
    status = "okay";
};

// Configure UART for console output
&uart1 {
    status = "okay";
    current-speed = <115200>;
};
EOF
```

### Step 3: Create Sensor Overlay

**Add I2C sensor support:**

```bash
cat > app.overlay << 'EOF'
/*
 * Application-specific device tree overlay
 * Adds sensor support for advanced configurations
 */

/ {
    aliases {
        temp-sensor = &bme280;
        ambient-light = &tsl2561;
    };
};

&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;
    
    // BME280 Temperature/Humidity/Pressure sensor
    bme280: bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
        label = "BME280";
    };
    
    // TSL2561 Light sensor
    tsl2561: tsl2561@39 {
        compatible = "taos,tsl2561";
        reg = <0x39>;
        label = "TSL2561";
    };
};

// Enable SPI for additional peripherals
&spi0 {
    status = "okay";
    cs-gpios = <&gpio0 8 GPIO_ACTIVE_LOW>,
               <&gpio0 7 GPIO_ACTIVE_LOW>;
    
    // Generic SPI device placeholder
    spi_device: spi-device@0 {
        compatible = "zephyr,spi-device";
        reg = <0>;
        spi-max-frequency = <1000000>;
        label = "SPI_DEV_0";
    };
};
EOF
```

### Step 4: Test Device Tree Modifications

**Build with custom overlay:**

```bash
# Build with our custom hardware overlay
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/debug.conf"

# Check generated device tree
ls build/zephyr/zephyr.dts

# Examine LED and button definitions in generated DTS
grep -A 5 -B 5 "status_led\|user_button" build/zephyr/zephyr.dts

# Check GPIO configuration
grep -A 10 "gpio@" build/zephyr/zephyr.dts
```

**Build with sensor overlay:**

```bash
# Build with sensor support
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/debug.conf" -DDTC_OVERLAY_FILE="app.overlay"

# Check for I2C and sensor nodes
grep -A 5 -B 5 "bme280\|tsl2561\|i2c1" build/zephyr/zephyr.dts
```

---

## Lab Exercise 3: Advanced Configuration Scenarios

### Objective
Implement real-world configuration scenarios using conditional compilation and advanced Kconfig features.

### Step 1: Environment-Based Configuration

**Create CMakeLists.txt with conditional configuration:**

```bash
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(config_lab)

target_sources(app PRIVATE src/main.c)

# Conditional configuration based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - enabling debugging features")
    list(APPEND CONF_FILE "prj.conf" "fragments/debug.conf")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build - enabling optimizations")
    list(APPEND CONF_FILE "prj.conf" "fragments/release.conf")
    
elseif(CMAKE_BUILD_TYPE STREQUAL "Testing")
    message(STATUS "Testing build - enabling test features")
    list(APPEND CONF_FILE "prj.conf" "fragments/debug.conf" "fragments/networking.conf")
    
else()
    message(STATUS "Default build configuration")
    set(CONF_FILE "prj.conf")
endif()

# Feature-based overlay selection
if(ENABLE_SENSORS)
    message(STATUS "Sensors enabled - adding sensor overlay")
    list(APPEND DTC_OVERLAY_FILE "app.overlay")
endif()

# Environment-specific settings
if(DEFINED ENV{CONFIG_PROFILE})
    message(STATUS "Using configuration profile: $ENV{CONFIG_PROFILE}")
    list(APPEND CONF_FILE "profiles/$ENV{CONFIG_PROFILE}.conf")
endif()
EOF
```

### Step 2: Create Configuration Profiles

**Create profile directory:**

```bash
mkdir profiles

cat > profiles/development.conf << 'EOF'
# Development Profile
# Maximum debugging and development convenience

CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_SHELL=y
CONFIG_KERNEL_SHELL=y
CONFIG_DEVICE_SHELL=y
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_AUTO=y
CONFIG_THREAD_ANALYZER_AUTO_INTERVAL=5

# Enable all debugging
CONFIG_DEBUG_OPTIMIZATIONS=y
CONFIG_DEBUG_THREAD_INFO=y
CONFIG_STACK_SENTINEL=y
CONFIG_INIT_STACKS=y

# Generous memory allocation
CONFIG_MAIN_STACK_SIZE=8192
CONFIG_SHELL_STACK_SIZE=4096
CONFIG_HEAP_MEM_POOL_SIZE=32768
EOF

cat > profiles/production.conf << 'EOF'
# Production Profile  
# Minimal footprint, maximum reliability

CONFIG_LOG_DEFAULT_LEVEL=1
CONFIG_LOG_MODE_MINIMAL=y

# Disable all debugging
CONFIG_DEBUG=n
CONFIG_ASSERT=n
CONFIG_SHELL=n
CONFIG_THREAD_ANALYZER=n
CONFIG_PRINTK=n
CONFIG_BOOT_BANNER=n

# Optimize for size and speed
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_SPEED_OPTIMIZATIONS=y
CONFIG_LTO=y

# Minimal memory usage
CONFIG_MAIN_STACK_SIZE=1024
CONFIG_IDLE_STACK_SIZE=256
CONFIG_HEAP_MEM_POOL_SIZE=4096
EOF

cat > profiles/testing.conf << 'EOF'
# Testing Profile
# Features needed for automated testing

CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_ZTEST=y
CONFIG_ZTEST_NEW_API=y

# Unity testing framework
CONFIG_ZTEST_UNITY_BACKEND=y

# Shell for test control
CONFIG_SHELL=y
CONFIG_KERNEL_SHELL=y

# Moderate debugging
CONFIG_ASSERT=y
CONFIG_THREAD_NAME=y
CONFIG_STACK_SENTINEL=y

# Test-friendly memory settings
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_ZTEST_STACKSIZE=2048
CONFIG_HEAP_MEM_POOL_SIZE=16384
EOF
```

### Step 3: Create Build Scripts

**Create convenient build scripts:**

```bash
cat > build_debug.sh << 'EOF'
#!/bin/bash
echo "Building debug configuration..."
west build -b rpi_4b --pristine -- -DCMAKE_BUILD_TYPE=Debug
echo "Debug build complete. Size:"
ls -la build/zephyr/zephyr.elf
EOF

cat > build_release.sh << 'EOF'
#!/bin/bash
echo "Building release configuration..."
west build -b rpi_4b --pristine -- -DCMAKE_BUILD_TYPE=Release
echo "Release build complete. Size:"
ls -la build/zephyr/zephyr.elf
EOF

cat > build_with_sensors.sh << 'EOF'
#!/bin/bash
echo "Building with sensor support..."
west build -b rpi_4b --pristine -- -DCMAKE_BUILD_TYPE=Debug -DENABLE_SENSORS=ON
echo "Sensor build complete. Size:"
ls -la build/zephyr/zephyr.elf
EOF

cat > build_profile.sh << 'EOF'
#!/bin/bash
if [ -z "$1" ]; then
    echo "Usage: $0 <profile>"
    echo "Available profiles: development, production, testing"
    exit 1
fi

echo "Building with profile: $1"
CONFIG_PROFILE=$1 west build -b rpi_4b --pristine
echo "Profile build complete. Size:"
ls -la build/zephyr/zephyr.elf
EOF

chmod +x *.sh
```

### Step 4: Test Advanced Configurations

**Test different build configurations:**

```bash
# Test debug build
./build_debug.sh

# Test release build  
./build_release.sh

# Compare sizes
echo "Debug vs Release size comparison:"
ls -la build/zephyr/zephyr.elf

# Test profile builds
./build_profile.sh development
./build_profile.sh production
./build_profile.sh testing

# Test with sensors
./build_with_sensors.sh
```

---

## Lab Exercise 4: Configuration Validation and Optimization

### Objective
Learn to validate configurations and optimize for specific requirements.

### Step 1: Configuration Analysis Tools

**Analyze configuration dependencies:**

```bash
# Build and analyze configuration
west build -b rpi_4b --pristine -- -DCONF_FILE="prj.conf fragments/debug.conf"

# Generate Kconfig documentation
west build -t kconfig-html

# View generated documentation
ls build/kconfig-html/
# Open index.html in browser to explore configuration hierarchy

# Show final configuration values
west build -t kconfig-print | grep -E "CONFIG_(LOG|DEBUG|MAIN_STACK)"

# Find configuration conflicts
west build -t kconfig-print | grep -i "warning\|error"
```

### Step 2: Memory Usage Analysis

**Create memory analysis configuration:**

```bash
cat > fragments/memory_analysis.conf << 'EOF'
# Memory Analysis Configuration
# Enables detailed memory usage reporting

CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_AUTO=y
CONFIG_THREAD_ANALYZER_AUTO_INTERVAL=10

CONFIG_INIT_STACKS=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_KERNEL_DEBUG=y

# Runtime statistics
CONFIG_STATS=y
CONFIG_STATS_NAMES=y

# Memory pool debugging
CONFIG_HEAP_MEM_POOL_SIZE=16384
CONFIG_SYS_HEAP_RUNTIME_STATS=y

# Shell for runtime inspection
CONFIG_SHELL=y
CONFIG_KERNEL_SHELL=y
CONFIG_THREAD_ANALYZER_USE_PRINTK=y
EOF
```

### Step 3: Performance Optimization

**Create performance-optimized configuration:**

```bash
cat > fragments/performance.conf << 'EOF'
# Performance Optimization Configuration
# Maximizes runtime performance

# High-performance scheduling
CONFIG_TIMESLICING=n
CONFIG_PREEMPT_ENABLED=y
CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000

# Compiler optimizations
CONFIG_SPEED_OPTIMIZATIONS=y
CONFIG_COMPILER_OPT="-O3 -march=armv8-a"

# Disable debugging overhead
CONFIG_ASSERT=n
CONFIG_STACK_SENTINEL=n
CONFIG_INIT_STACKS=n
CONFIG_THREAD_ANALYZER=n

# Optimize interrupt handling
CONFIG_IRQ_OFFLOAD=y
CONFIG_DYNAMIC_INTERRUPTS=n

# Memory optimizations
CONFIG_KERNEL_MEM_POOL=y
CONFIG_HEAP_MEM_POOL_SIZE=32768

# Reduce context switch overhead
CONFIG_THREAD_CUSTOM_DATA=n
CONFIG_ERRNO=n
EOF
```

### Step 4: Comprehensive Testing

**Create test validation script:**

```bash
cat > validate_configs.sh << 'EOF'
#!/bin/bash

echo "=== Configuration Validation Script ==="

configs=(
    "prj.conf"
    "prj.conf fragments/debug.conf"
    "prj.conf fragments/release.conf"
    "prj.conf fragments/networking.conf"
    "prj.conf fragments/memory_analysis.conf"
    "prj.conf fragments/performance.conf"
)

overlays=(
    ""
    "app.overlay"
)

results_file="build_results.txt"
echo "Build Results - $(date)" > $results_file
echo "==============================" >> $results_file

for config in "${configs[@]}"; do
    for overlay in "${overlays[@]}"; do
        echo "Testing configuration: $config"
        if [ -n "$overlay" ]; then
            echo "  with overlay: $overlay"
            build_args="-- -DCONF_FILE=\"$config\" -DDTC_OVERLAY_FILE=\"$overlay\""
        else
            build_args="-- -DCONF_FILE=\"$config\""
        fi
        
        if eval "west build -b rpi_4b --pristine $build_args" > build.log 2>&1; then
            size=$(stat -c%s build/zephyr/zephyr.elf)
            echo "  ✓ SUCCESS - Size: $size bytes"
            echo "SUCCESS: $config $overlay - Size: $size" >> $results_file
        else
            echo "  ✗ FAILED"
            echo "FAILED: $config $overlay" >> $results_file
            tail -10 build.log >> $results_file
        fi
        echo "---" >> $results_file
    done
done

echo "Validation complete. Results saved to $results_file"
EOF

chmod +x validate_configs.sh
./validate_configs.sh
```

---

## Lab Summary and Analysis

### Configuration Comparison

**Analyze the results from your builds:**

```bash
cat build_results.txt

# Compare binary sizes
echo "Binary Size Comparison:"
echo "Configuration                           Size (bytes)"
echo "=================================================="
grep "SUCCESS:" build_results.txt | sort -k4 -n
```

### Key Learning Points

1. **Configuration Layers:** Base configuration + fragments provide flexible, maintainable configuration management
2. **Device Tree Overlays:** Enable hardware customization without modifying base board definitions
3. **Build Profiles:** Environment-specific optimizations significantly impact binary size and performance
4. **Validation:** Automated testing prevents configuration errors and regressions

### Best Practices Demonstrated

- **Modular Configuration:** Separate concerns with configuration fragments
- **Version Control:** All configurations should be tracked and documented
- **Testing:** Validate all configuration combinations before deployment
- **Documentation:** Use clear naming and comments in configuration files

### Real-World Applications

This lab's techniques apply directly to:
- **Product Development:** Different builds for development, testing, and production
- **Hardware Variants:** Supporting multiple board revisions with overlays
- **Feature Management:** Enabling/disabling features based on requirements
- **Resource Optimization:** Tailoring configurations for memory and performance constraints

---

## Challenge Exercise (Optional)

Create a configuration system for an IoT sensor node that supports:

1. **Low-power mode:** Minimal features, maximum battery life
2. **Development mode:** Full debugging and shell access
3. **Field testing mode:** Networking enabled with remote logging
4. **Sensor variants:** Different overlays for various sensor configurations

**Requirements:**
- Use configuration fragments for each mode
- Create device tree overlays for at least two sensor types
- Implement automated build validation
- Document memory usage for each configuration

This challenge reinforces all concepts covered in this lab while preparing you for real-world Zephyr development scenarios.

[Next Chapter 5: West](../chapter_05_west/README.md)
