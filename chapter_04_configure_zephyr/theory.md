# Chapter 4: Configure Zephyr - Theory

Building on your understanding of the Zephyr build system from Chapter 3, this theory section explores how to precisely configure Zephyr for your application's needs. You'll master both Kconfig for feature selection and device tree for hardware description.

---

## Understanding Kconfig in Zephyr

### The Kconfig Philosophy

Kconfig originated in the Linux kernel and provides a hierarchical configuration system that manages thousands of interdependent options. In Zephyr, Kconfig determines which code gets compiled into your firmware, directly impacting binary size, feature availability, and system behavior.

**Key Principles:**

* **Dependency Management:** Options can depend on other options, ensuring consistent configurations
* **Default Values:** Sensible defaults reduce configuration burden while allowing customization
* **Conditional Compilation:** Only selected features are compiled, keeping binaries lean
* **Validation:** The system prevents invalid configuration combinations

### Kconfig File Structure

**Main Configuration Files:**

```bash
my_application/
├── prj.conf                    # Primary configuration file
├── boards/rpi_4b.conf          # Board-specific overrides
├── debug.conf                  # Configuration fragment for debugging
├── release.conf                # Configuration fragment for production
└── CMakeLists.txt
```

### Essential Kconfig Options for Embedded Development

**Core System Configuration:**

```bash
# Kernel features
CONFIG_MULTITHREADING=y         # Enable multi-threading (usually default)
CONFIG_MAIN_STACK_SIZE=4096     # Main thread stack size
CONFIG_IDLE_STACK_SIZE=1024     # Idle thread stack size
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Memory management
CONFIG_HEAP_MEM_POOL_SIZE=16384 # Heap size for dynamic allocation
CONFIG_MAIN_STACK_SIZE=4096     # Application main stack

# Timing and scheduling
CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000  # System tick frequency
CONFIG_TIMESLICING=y            # Enable time-slicing for equal-priority threads
```

**Communication and Networking:**

```bash
# Serial communication
CONFIG_SERIAL=y                 # Enable UART drivers
CONFIG_UART_CONSOLE=y           # Enable console over UART
CONFIG_PRINTK=y                 # Enable printk() for debugging

# I2C for sensors
CONFIG_I2C=y                    # Enable I2C subsystem
CONFIG_I2C_DMA=y               # Enable DMA for I2C transfers

# SPI communication
CONFIG_SPI=y                    # Enable SPI subsystem
CONFIG_SPI_DMA=y               # Enable DMA for SPI transfers

# GPIO
CONFIG_GPIO=y                   # Enable GPIO drivers (often default)
```

**Networking Stack (when needed):**

```bash
# Basic networking
CONFIG_NETWORKING=y             # Enable networking subsystem
CONFIG_NET_IPV4=y              # Enable IPv4 support
CONFIG_NET_IPV6=y              # Enable IPv6 support
CONFIG_NET_TCP=y               # Enable TCP protocol
CONFIG_NET_UDP=y               # Enable UDP protocol

# WiFi support
CONFIG_WIFI=y                  # Enable WiFi subsystem
CONFIG_NET_L2_WIFI_MGMT=y      # WiFi management layer

# Bluetooth
CONFIG_BT=y                    # Enable Bluetooth
CONFIG_BT_PERIPHERAL=y         # Enable peripheral role
CONFIG_BT_CENTRAL=y            # Enable central role
```

### Advanced Kconfig Techniques

**Conditional Configuration:**

```bash
# Enable debugging features only in debug builds
CONFIG_DEBUG=y
CONFIG_DEBUG_OPTIMIZATIONS=y
CONFIG_ASSERT=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=4

# Size optimizations for production
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_LTO=y                   # Link-time optimization
CONFIG_COMPILER_OPT="-Os"      # Optimize for size
```

**Memory Optimization:**

```bash
# Disable unused features to save memory
CONFIG_CONSOLE_SUBSYS=n        # Disable console subsystem
CONFIG_SHELL=n                 # Disable shell
CONFIG_BOOT_BANNER=n           # Disable boot banner
CONFIG_THREAD_NAME=n           # Disable thread names
CONFIG_THREAD_STACK_INFO=n     # Disable stack info
```

### Configuration Fragments

Configuration fragments allow modular, reusable configuration management. They're especially useful for different build variants (debug, release, testing).

**Creating Configuration Fragments:**

```bash
# debug.conf - Debug configuration fragment
CONFIG_DEBUG=y
CONFIG_DEBUG_OPTIMIZATIONS=y
CONFIG_ASSERT=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_NAME=y
CONFIG_STACK_SENTINEL=y
```

```bash
# release.conf - Production configuration fragment
CONFIG_DEBUG=n
CONFIG_ASSERT=n
CONFIG_LOG_DEFAULT_LEVEL=1
CONFIG_SIZE_OPTIMIZATIONS=y
CONFIG_LTO=y
CONFIG_BOOT_BANNER=n
CONFIG_PRINTK=n
```

```bash
# networking.conf - Networking feature fragment
CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y
CONFIG_NET_DHCPV4=y
CONFIG_DNS_RESOLVER=y
```

**Using Fragments in CMakeLists.txt:**

```cmake
# Conditional fragment inclusion based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONF_FILE "prj.conf debug.conf")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CONF_FILE "prj.conf release.conf")
endif()

# Multiple fragments for complex configurations
set(CONF_FILE "prj.conf networking.conf sensors.conf")
```

## Device Tree Fundamentals

### Device Tree Concepts

Device Tree (DT) describes hardware in a standardized, hierarchical format. Unlike Kconfig which controls software features, device tree describes hardware topology, addresses, and properties.

**Key Concepts:**

* **Nodes:** Represent hardware components (CPU, memory, peripherals)
* **Properties:** Describe characteristics of nodes (addresses, interrupts, GPIO pins)
* **Compatible Strings:** Link nodes to appropriate device drivers
* **Aliases:** Provide convenient names for commonly referenced nodes

### Device Tree Syntax Overview

**Basic Node Structure:**

```dts
/ {
    model = "Raspberry Pi 4 Model B";
    compatible = "raspberrypi,4-model-b", "brcm,bcm2838";
    
    memory@0 {
        device_type = "memory";
        reg = <0x00000000 0x40000000>; // 1GB RAM
    };
    
    cpus {
        #address-cells = <1>;
        #size-cells = <0>;
        
        cpu@0 {
            device_type = "cpu";
            compatible = "arm,cortex-a72";
            reg = <0>;
        };
    };
};
```

**GPIO and Peripheral Definitions:**

```dts
gpio: gpio@7e200000 {
    compatible = "brcm,bcm2835-gpio";
    reg = <0x7e200000 0xb4>;
    gpio-controller;
    #gpio-cells = <2>;
    
    uart1_pins: uart1 {
        brcm,pins = <14 15>;
        brcm,function = <2>; // ALT5
    };
};

uart1: uart@7e215040 {
    compatible = "brcm,bcm2835-uart";
    reg = <0x7e215040 0x40>;
    interrupts = <2 25>;
    pinctrl-names = "default";
    pinctrl-0 = <&uart1_pins>;
    status = "okay";
};
```

### Device Tree Overlays

Overlays modify existing device trees without changing the base board definition. This is crucial for adding sensors, changing pin configurations, or enabling additional peripherals.

**Creating Application Overlays:**

```dts
// app.overlay - Add I2C sensor to Raspberry Pi 4B
/ {
    aliases {
        temp-sensor = &bme280;
    };
};

&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;
    
    bme280: bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
        label = "BME280_TEMP_HUMIDITY";
    };
};
```

**Board-Specific Overlays:**

```dts
// boards/rpi_4b.overlay - Raspberry Pi 4B specific modifications
/ {
    leds {
        compatible = "gpio-leds";
        
        status_led: led_0 {
            gpios = <&gpio1 14 GPIO_ACTIVE_HIGH>; // GPIO 42
            label = "Status LED";
        };
    };
    
    buttons {
        compatible = "gpio-keys";
        
        user_button: button_0 {
            gpios = <&gpio0 21 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>; // GPIO 21
            label = "User Button";
        };
    };
};

// Enable SPI for external peripherals
&spi0 {
    status = "okay";
    cs-gpios = <&gpio0 8 GPIO_ACTIVE_LOW>;
    
    spi_device: spi-device@0 {
        compatible = "zephyr,spi-device";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

### Advanced Device Tree Techniques

**Custom Device Bindings:**

```yaml
# dts/bindings/custom-sensor.yaml
description: Custom sensor device

compatible: "custom,sensor-v1"

properties:
  reg:
    type: int
    required: true
    description: I2C address
    
  sampling-rate:
    type: int
    default: 100
    description: Sampling rate in Hz
    
  enable-gpio:
    type: phandle-array
    description: GPIO for enabling sensor
```

**Using Custom Bindings:**

```dts
&i2c1 {
    custom_sensor: sensor@48 {
        compatible = "custom,sensor-v1";
        reg = <0x48>;
        sampling-rate = <200>;
        enable-gpio = <&gpio0 12 GPIO_ACTIVE_HIGH>;
    };
};
```

## Configuration Best Practices

### Organization Strategies

**Layered Configuration Approach:**

1. **Base Configuration (prj.conf):** Essential features every build needs
2. **Board Configuration:** Hardware-specific settings for rpi_4b
3. **Feature Fragments:** Modular features (networking, sensors, debugging)
4. **Environment Fragments:** Build-specific settings (debug, release, testing)

**Example Project Structure:**

```bash
my_iot_project/
├── prj.conf                    # Base configuration
├── CMakeLists.txt
├── src/
├── include/
├── boards/
│   ├── rpi_4b.conf            # Raspberry Pi 4B settings
│   └── rpi_4b.overlay         # Hardware customizations
├── fragments/
│   ├── debug.conf             # Debug features
│   ├── release.conf           # Production optimizations
│   ├── networking.conf        # Network stack
│   ├── sensors.conf           # Sensor drivers
│   └── logging.conf           # Logging configuration
└── overlays/
    ├── testing.overlay        # Test-specific hardware
    └── production.overlay     # Production hardware
```

### Performance and Memory Optimization

**Memory-Constrained Configurations:**

```bash
# Minimize memory usage
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_IDLE_STACK_SIZE=512
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=1024
CONFIG_HEAP_MEM_POOL_SIZE=8192

# Disable memory-heavy features
CONFIG_THREAD_NAME=n
CONFIG_THREAD_STACK_INFO=n
CONFIG_KERNEL_MEM_POOL=n
CONFIG_NET_BUF_DATA_SIZE=128
```

**Real-Time Performance Configurations:**

```bash
# Optimize for deterministic behavior
CONFIG_TIMESLICING=n           # Disable time-slicing
CONFIG_PREEMPT_ENABLED=y       # Enable preemption
CONFIG_IRQ_OFFLOAD=y          # Enable IRQ offloading
CONFIG_THREAD_ANALYZER=n       # Disable runtime analysis
CONFIG_SYS_CLOCK_TICKS_PER_SEC=10000  # Higher tick rate
```

### Validation and Testing

**Configuration Validation Commands:**

```bash
# Check configuration consistency
west build -b rpi_4b -- -DKCONFIG_ROOT=Kconfig

# Generate configuration documentation
west build -b rpi_4b -t kconfig-html

# Show final configuration
west build -b rpi_4b -t kconfig-print
```

---

This theory foundation prepares you for hands-on configuration practice in the Lab section, where you'll apply these concepts to create optimized Zephyr applications for your Raspberry Pi 4B.