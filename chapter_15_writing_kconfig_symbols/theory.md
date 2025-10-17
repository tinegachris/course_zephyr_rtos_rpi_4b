# Chapter 15 - Writing Kconfig Symbols Theory

## 15.1 Understanding Kconfig in Zephyr

Kconfig is the configuration system used by Zephyr RTOS to control build-time options. It allows developers to customize their applications by selecting which features to include, setting parameter values, and controlling compilation behavior.

### The Role of Kconfig Symbols

Kconfig symbols are the building blocks of the configuration system. They define:

* **Feature toggles**: Enable or disable specific functionality
* **Parameter values**: Set numeric, string, or hex values
* **Dependencies**: Control when options are available
* **Defaults**: Provide sensible starting configurations

### Why Kconfig Matters

In embedded systems, one size does not fit all. Consider these scenarios:

* **Memory-constrained devices**: Need to disable unused features
* **Performance-critical applications**: Require specific optimizations
* **Hardware variants**: Need different peripheral configurations
* **Safety-critical systems**: Require specific feature combinations

## 15.2 Kconfig Syntax Fundamentals

### Basic Configuration Types

```kconfig
# Boolean configuration
config FEATURE_ENABLED
    bool "Enable special feature"
    default y
    help
      Enable or disable the special feature functionality.

# Integer configuration
config BUFFER_SIZE
    int "Buffer size in bytes"
    default 1024
    range 64 4096
    help
      Size of the main buffer in bytes.

# String configuration
config DEVICE_NAME
    string "Device name"
    default "my_device"
    help
      Name of the device for identification.

# Hexadecimal configuration
config BASE_ADDRESS
    hex "Base memory address"
    default 0x20000000
    help
      Base address for memory mapping.
```

### Menu Organization

```kconfig
menuconfig MY_SUBSYSTEM
    bool "My Subsystem"
    help
      Enable my custom subsystem with various options.

if MY_SUBSYSTEM

config MY_SUBSYSTEM_DEBUG
    bool "Enable debug output"
    default n
    help
      Enable debugging messages for my subsystem.

config MY_SUBSYSTEM_PRIORITY
    int "Thread priority"
    default 5
    range 0 15
    help
      Priority level for subsystem threads.

endif # MY_SUBSYSTEM
```

## 15.3 Advanced Kconfig Features

### Dependencies and Selections

```kconfig
config ADVANCED_CRYPTO
    bool "Advanced cryptography"
    depends on CRYPTO_ENABLED
    select MATH_LIB
    select RANDOM_GENERATOR
    help
      Enable advanced cryptographic algorithms.
      Requires basic crypto support and automatically
      includes math library and random number generation.
```

### Choice Groups

```kconfig
choice COMMUNICATION_PROTOCOL
    prompt "Select communication protocol"
    default COMM_UART
    help
      Choose the primary communication protocol.

config COMM_UART
    bool "UART communication"
    help
      Use UART for communication.

config COMM_SPI
    bool "SPI communication"
    help
      Use SPI for communication.

config COMM_I2C
    bool "I2C communication"
    help
      Use I2C for communication.

endchoice
```

### Conditional Configuration

```kconfig
config SENSOR_MODULE
    bool "Sensor module support"
    default y
    help
      Enable sensor module functionality.

if SENSOR_MODULE

choice SENSOR_TYPE
    prompt "Primary sensor type"
    default SENSOR_TEMPERATURE

config SENSOR_TEMPERATURE
    bool "Temperature sensor"

config SENSOR_HUMIDITY
    bool "Humidity sensor"

config SENSOR_PRESSURE
    bool "Pressure sensor"

endchoice

config SENSOR_SAMPLING_RATE
    int "Sampling rate (Hz)"
    default 10
    range 1 1000
    help
      How often to sample the sensor data.

config SENSOR_CALIBRATION
    bool "Enable sensor calibration"
    default y
    help
      Include calibration routines for improved accuracy.

endif # SENSOR_MODULE
```

## 15.4 Module Integration Patterns

### Creating Module Kconfig

For a custom module, create a structured Kconfig file:

```kconfig
# modules/my_module/Kconfig

menuconfig MY_MODULE
    bool "My Custom Module"
    help
      Enable my custom module with configurable options.

if MY_MODULE

config MY_MODULE_LOG_LEVEL
    int "Log level"
    default 3
    range 0 4
    help
      Logging level for the module:
      0 = No logging
      1 = Error only
      2 = Warning and error
      3 = Info, warning, and error
      4 = Debug and all above

config MY_MODULE_THREAD_STACK_SIZE
    int "Thread stack size"
    default 1024
    range 512 4096
    help
      Stack size for module thread in bytes.

config MY_MODULE_THREAD_PRIORITY
    int "Thread priority"
    default 7
    range 0 15
    help
      Priority of the module thread.

config MY_MODULE_MAX_CONNECTIONS
    int "Maximum connections"
    default 5
    range 1 20
    help
      Maximum number of simultaneous connections.

config MY_MODULE_FEATURES
    bool "Enable advanced features"
    default n
    help
      Enable additional features that increase memory usage.

if MY_MODULE_FEATURES

config MY_MODULE_ENCRYPTION
    bool "Enable encryption"
    select CRYPTO
    default y
    help
      Enable data encryption for secure communication.

config MY_MODULE_COMPRESSION
    bool "Enable compression"
    default n
    help
      Enable data compression to reduce bandwidth usage.

endif # MY_MODULE_FEATURES

endif # MY_MODULE
```

## 15.5 Integration with Source Code

### Using Kconfig in C Code

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Use Kconfig values in source code */
#define MODULE_STACK_SIZE CONFIG_MY_MODULE_THREAD_STACK_SIZE
#define MODULE_PRIORITY CONFIG_MY_MODULE_THREAD_PRIORITY
#define MAX_CONNECTIONS CONFIG_MY_MODULE_MAX_CONNECTIONS

LOG_MODULE_REGISTER(my_module, CONFIG_MY_MODULE_LOG_LEVEL);

/* Conditional compilation based on Kconfig */
#ifdef CONFIG_MY_MODULE_FEATURES
static bool advanced_features_enabled = true;
#else
static bool advanced_features_enabled = false;
#endif

/* Thread definition using Kconfig values */
K_THREAD_DEFINE(my_module_thread, MODULE_STACK_SIZE, my_module_thread_func,
                NULL, NULL, NULL, MODULE_PRIORITY, 0, 0);

void my_module_init(void)
{
    LOG_INF("Initializing module with stack size: %d", MODULE_STACK_SIZE);
    LOG_INF("Thread priority: %d", MODULE_PRIORITY);
    LOG_INF("Max connections: %d", MAX_CONNECTIONS);
    
    #ifdef CONFIG_MY_MODULE_ENCRYPTION
    LOG_INF("Encryption enabled");
    init_encryption();
    #endif
    
    #ifdef CONFIG_MY_MODULE_COMPRESSION
    LOG_INF("Compression enabled");
    init_compression();
    #endif
}
```

### CMakeLists.txt Integration

```cmake
# CMakeLists.txt

zephyr_library()

# Always include the main module file
zephyr_library_sources(src/my_module.c)

# Conditionally include feature files
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_ENCRYPTION src/encryption.c)
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_COMPRESSION src/compression.c)

# Include directories based on configuration
if(CONFIG_MY_MODULE_FEATURES)
    zephyr_include_directories(include/advanced)
endif()

# Pass configuration to compiler
if(CONFIG_MY_MODULE_FEATURES)
    zephyr_compile_definitions(MODULE_ADVANCED_MODE=1)
endif()
```

## 15.6 Best Practices

### Naming Conventions

* Use descriptive, hierarchical names: `CONFIG_SENSOR_TEMP_MAX31855_ENABLED`
* Follow subsystem prefixes: `CONFIG_BT_*`, `CONFIG_NET_*`, `CONFIG_FS_*`
* Use consistent suffixes: `_ENABLED`, `_SIZE`, `_PRIORITY`, `_COUNT`

### Documentation Standards

```kconfig
config FEATURE_X
    bool "Enable Feature X"
    default n
    help
      Feature X provides advanced capability for specific use cases.
      
      This feature increases memory usage by approximately 2KB and
      requires additional processing time. Only enable if you need
      the advanced functionality.
      
      Dependencies: Requires CONFIG_BASE_FEATURE to be enabled.
      
      Note: This feature is experimental in this release.
```

### Dependency Management

```kconfig
# Good: Clear dependencies
config ADVANCED_FEATURE
    bool "Advanced feature"
    depends on BASE_FEATURE
    depends on !MEMORY_CONSTRAINED
    select REQUIRED_LIB
    help
      Feature that builds on base functionality.

# Better: Logical grouping
config FEATURE_SET_A
    bool "Feature Set A"
    select FEATURE_A1
    select FEATURE_A2
    select FEATURE_A3
    help
      Enable all features in set A.
```

### Validation and Ranges

```kconfig
config BUFFER_COUNT
    int "Number of buffers"
    default 4
    range 1 32
    help
      Number of buffers to allocate. More buffers improve
      performance but increase memory usage.

config THREAD_PRIORITY
    int "Thread priority"
    default 7
    range 0 CONFIG_NUM_PREEMPT_PRIORITIES
    help
      Thread priority level. Lower numbers mean higher priority.
```

## 15.7 Testing and Validation

### Configuration Testing

Create test configurations to validate different Kconfig combinations:

```kconfig
# test_configs/minimal.conf
CONFIG_MY_MODULE=y
CONFIG_MY_MODULE_LOG_LEVEL=1
CONFIG_MY_MODULE_THREAD_STACK_SIZE=512
CONFIG_MY_MODULE_MAX_CONNECTIONS=1

# test_configs/full_features.conf
CONFIG_MY_MODULE=y
CONFIG_MY_MODULE_FEATURES=y
CONFIG_MY_MODULE_ENCRYPTION=y
CONFIG_MY_MODULE_COMPRESSION=y
CONFIG_MY_MODULE_LOG_LEVEL=4
CONFIG_MY_MODULE_THREAD_STACK_SIZE=2048
CONFIG_MY_MODULE_MAX_CONNECTIONS=10
```

### Automated Validation

```python
# scripts/validate_config.py
def validate_memory_usage(config):
    """Ensure configuration doesn't exceed memory limits"""
    stack_size = config.get('CONFIG_MY_MODULE_THREAD_STACK_SIZE', 1024)
    max_conn = config.get('CONFIG_MY_MODULE_MAX_CONNECTIONS', 5)
    
    total_memory = stack_size + (max_conn * 256)  # 256 bytes per connection
    
    if total_memory > 8192:  # 8KB limit
        raise ValueError(f"Configuration exceeds memory limit: {total_memory} bytes")
```

This theory foundation provides the knowledge needed to create professional, maintainable Kconfig configurations for Zephyr applications.