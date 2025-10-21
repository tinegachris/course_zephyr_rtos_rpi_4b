# Chapter 3: Zephyr Build System - Lab Exercises

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

In this hands-on lab, you'll master the Zephyr build system through progressive exercises. Each lab builds on concepts from the Theory section and prepares you for advanced development scenarios.

---

## Prerequisites

Before starting these exercises, ensure you have:

- Completed Chapter 2 labs successfully
- Zephyr development environment set up with VS Code
- West workspace initialized in `~/zephyrproject`
- Raspberry Pi 4B connected and accessible

**Verify Your Setup:**

```bash
# Check Zephyr environment
echo $ZEPHYR_BASE
west --version

# Verify board support
west boards | grep rpi_4b
```

---

## Lab 1: Understanding West Workspace Structure

### Objective
Explore the West workspace structure and understand multi-repository management.

### Exercise 1.1: Analyze Workspace Layout

**Step 1:** Navigate to your Zephyr workspace and examine the structure:

```bash
cd ~/zephyrproject
tree -L 3 -d
```

**Step 2:** Examine West configuration:

```bash
# View West configuration
cat .west/config

# Show manifest information
west manifest --path
west list
```

**Step 3:** Explore project repositories:

```bash
# Check Zephyr repository status
cd zephyr
git log --oneline -5
git describe --tags

# Check module repositories
cd ../modules/hal/nordic
git log --oneline -3
```

**Expected Output Analysis:**
- Understand how West manages multiple repositories
- Identify the role of each repository in the ecosystem
- See how versions are synchronized across repositories

### Exercise 1.2: West Update and Synchronization

**Step 1:** Update all repositories:

```bash
cd ~/zephyrproject
west update
```

**Step 2:** Check for changes:

```bash
west status
west diff
```

**Step 3:** Practice selective updates:

```bash
# Update only specific projects
west update zephyr
west update hal_nordic
```

**Learning Outcome:**
You'll understand how West keeps your entire development environment synchronized and up-to-date.

---

## Lab 2: CMake Build Configuration Mastery

### Objective
Master CMake configuration for Zephyr applications with focus on modularity and optimization.

### Exercise 2.1: Create a Multi-Module Application

**Step 1:** Create a new application structure:

```bash
cd ~/zephyrproject
mkdir -p apps/sensor_hub/{src/{sensors,display,networking},include,boards}
cd apps/sensor_hub
```

**Step 2:** Create the main CMakeLists.txt:

```cmake
# apps/sensor_hub/CMakeLists.txt
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_hub)

# Main application source
target_sources(app PRIVATE src/main.c)

# Add modules
add_subdirectory(src/sensors)
add_subdirectory(src/display)
add_subdirectory(src/networking)

# Global includes
target_include_directories(app PRIVATE include/)

# Board-specific configuration
if(BOARD STREQUAL "rpi_4b")
    message(STATUS "Configuring for Raspberry Pi 4B")
    target_compile_definitions(app PRIVATE BOARD_RPI4B=1)
endif()
```

**Step 3:** Create module headers:

Create the following files in `apps/sensor_hub/include/`:

```c
// apps/sensor_hub/include/sensors.h
#ifndef SENSORS_H_
#define SENSORS_H_

int sensors_init(void);

#endif /* SENSORS_H_ */
```

```c
// apps/sensor_hub/include/display.h
#ifndef DISPLAY_H_
#define DISPLAY_H_

int display_init(void);

#endif /* DISPLAY_H_ */
```

```c
// apps/sensor_hub/include/networking.h
#ifndef NETWORKING_H_
#define NETWORKING_H_

int networking_init(void);

#endif /* NETWORKING_H_ */
```

**Step 4:** Create main application:

```c
// apps/sensor_hub/src/main.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensors.h"
#include "display.h"
#include "networking.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Sensor Hub Application Starting...");

    #ifdef BOARD_RPI4B
    LOG_INF("Running on Raspberry Pi 4B");
    #endif

    // Initialize modules
    sensors_init();
    display_init();
    networking_init();

    LOG_INF("All modules initialized successfully");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
```

**Step 5:** Create sensor module:

```cmake
# apps/sensor_hub/src/sensors/CMakeLists.txt
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/temperature.c
)

# Conditional compilation
if(CONFIG_BME280)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bme280_driver.c)
endif()
```

```c
// apps/sensor_hub/src/sensors/sensors.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

int sensors_init(void)
{
    LOG_INF("Initializing sensor subsystem");
    return 0;
}
```

**Step 6:** Create display module:

```cmake
# apps/sensor_hub/src/display/CMakeLists.txt
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/display.c
)

if(CONFIG_DISPLAY)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lcd_driver.c)
endif()
```

```c
// apps/sensor_hub/src/display/display.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display, LOG_LEVEL_INF);

int display_init(void)
{
    LOG_INF("Initializing display subsystem");
    return 0;
}
```

**Step 7:** Create networking module:

```cmake
# apps/sensor_hub/src/networking/CMakeLists.txt
if(CONFIG_NETWORKING)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/networking.c
        ${CMAKE_CURRENT_SOURCE_DIR}/wifi_manager.c
    )
endif()
```

```c
// apps/sensor_hub/src/networking/networking.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(networking, LOG_LEVEL_INF);

int networking_init(void)
{
    LOG_INF("Initializing networking subsystem");
    return 0;
}
```

**Step 8:** Create configuration file:

```bash
# apps/sensor_hub/prj.conf
CONFIG_LOG=y
CONFIG_PRINTK=y

# Enable modules based on requirements
CONFIG_SENSOR=y
CONFIG_DISPLAY=y
CONFIG_NETWORKING=y

# Optimization settings
CONFIG_SPEED_OPTIMIZATIONS=y
CONFIG_MAIN_STACK_SIZE=4096
```

### Exercise 2.2: Build and Test

**Step 1:** Build the application:

```bash
cd ~/zephyrproject/apps/sensor_hub
west build -b rpi_4b
```

**Step 2:** Analyze build output:

```bash
# Check generated files
ls build/zephyr/

# View memory usage
cat build/zephyr/zephyr.map | grep -A 10 "Memory Configuration"

# Examine symbols
nm build/zephyr/zephyr.elf | grep -E "(sensors_init|display_init|networking_init)"
```

**Step 3:** Test conditional compilation:

```bash
# Build without networking
west build -b rpi_4b -- -DCONFIG_NETWORKING=n

# Compare binary sizes
ls -la build/zephyr/zephyr.bin
```

---

## Lab 3: Advanced Build Configuration

### Objective
Implement advanced build configurations including board-specific optimizations and custom configurations.

### Exercise 3.1: Board-Specific Configurations

**Step 1:** Create board-specific configuration:

```bash
# apps/sensor_hub/boards/rpi_4b.conf
CONFIG_ARM_MPU=y
CONFIG_FPU=y
CONFIG_SPEED_OPTIMIZATIONS=y

# Raspberry Pi specific settings
CONFIG_SERIAL=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y

# Enable additional features for RPi4B
CONFIG_SMP=y
CONFIG_MP_NUM_CPUS=4
```

**Step 2:** Create device tree overlay:

```dts
// apps/sensor_hub/boards/rpi_4b.overlay
/ {
    aliases {
        sensor-i2c = &i2c1;
        status-led = &led0;
    };
    
    sensors {
        compatible = "sensor-hub";
        
        temperature_sensor: bme280@76 {
            compatible = "bosch,bme280";
            reg = <0x76>;
            label = "BME280_TEMP";
        };
    };
};

&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;
};

&gpio {
    status = "okay";
};
```

**Step 3:** Test board-specific build:

```bash
west build -b rpi_4b -p
west build -t menuconfig
```

### Exercise 3.2: Custom Build Targets

**Step 1:** Add custom CMake targets:

```cmake
# Add to main CMakeLists.txt
add_custom_target(analyze
    COMMAND ${CMAKE_SIZE} ${PROJECT_BINARY_DIR}/zephyr/zephyr.elf
    COMMAND ${CMAKE_OBJDUMP} -h ${PROJECT_BINARY_DIR}/zephyr/zephyr.elf
    DEPENDS ${PROJECT_BINARY_DIR}/zephyr/zephyr.elf
    COMMENT "Analyzing binary size and sections"
)

add_custom_target(flash_and_monitor
    COMMAND west flash
    COMMAND west attach
    COMMENT "Flash firmware and start monitoring"
)
```

**Step 2:** Use custom targets:

```bash
west build -t analyze
west build -t flash_and_monitor
```

---

## Lab 4: Build System Debugging and Optimization

### Objective
Learn to debug build issues and optimize build performance.

### Exercise 4.1: Verbose Build Analysis

**Step 1:** Enable verbose build output:

```bash
west build -b rpi_4b -v
```

**Step 2:** Analyze compilation flags:

```bash
# Generate compilation database
west build -b rpi_4b -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Examine compile commands
jq '.[0]' build/compile_commands.json
```

**Step 3:** Debug CMake configuration:

```bash
# Show CMake variables
cmake -LAH build/ | grep ZEPHYR

# Debug CMake generation
west build -b rpi_4b -- --debug-output
```

### Exercise 4.2: Build Performance Optimization

**Step 1:** Measure build times:

```bash
# Time full rebuild
time west build -b rpi_4b -p

# Enable ccache
export USE_CCACHE=1
time west build -b rpi_4b -p

# Use Ninja generator (faster than Make)
west build -b rpi_4b -p -- -G Ninja
time west build -b rpi_4b -p
```

**Step 2:** Parallel compilation:

```bash
# Use multiple cores
west build -b rpi_4b -- -j$(nproc)

# Check CPU usage during build
htop &
west build -b rpi_4b -p -- -j$(nproc)
```

---

## Lab 5: Real-World Build Integration

### Objective
Integrate the build system with development workflows and CI/CD practices.

### Exercise 5.1: Automated Build Scripts

**Step 1:** Create build automation script:

```bash
#!/bin/bash
# apps/sensor_hub/scripts/build.sh

set -e

BOARD=${1:-rpi_4b}
BUILD_TYPE=${2:-debug}

echo "Building sensor_hub for $BOARD ($BUILD_TYPE)"

# Clean previous build
west build -b $BOARD -p

# Configure based on build type
if [ "$BUILD_TYPE" == "release" ]; then
    west build -b $BOARD -- -DCONFIG_DEBUG=n -DCONFIG_ASSERT=n
else
    west build -b $BOARD -- -DCONFIG_DEBUG=y -DCONFIG_ASSERT=y
fi

# Generate build report
echo "Build completed successfully"
ls -la build/zephyr/zephyr.*
size build/zephyr/zephyr.elf
```

**Step 2:** Create deployment script:

```bash
#!/bin/bash
# apps/sensor_hub/scripts/deploy.sh

set -e

echo "Deploying to Raspberry Pi 4B"

# Flash firmware
west flash

# Verify deployment
echo "Waiting for device to boot..."
sleep 3

# Monitor output for verification
timeout 10s west attach || true

echo "Deployment completed"
```

### Exercise 5.2: Build Verification

**Step 1:** Create test configuration:

```bash
# apps/sensor_hub/test.conf
CONFIG_ZTEST=y
CONFIG_ZTEST_NEW_API=y

# Merge with main config
CONFIG_LOG=y
CONFIG_PRINTK=y
```

**Step 2:** Build and run tests:

```bash
west build -b native_posix -p -- -DCONF_FILE="prj.conf test.conf"
west build -t run
```

---

## Lab 6: Advanced Device Tree Integration

### Objective
Master device tree integration in the build process for hardware customization.

### Exercise 6.1: Custom Device Tree Bindings

**Step 1:** Create custom binding:

```yaml
# apps/sensor_hub/dts/bindings/sensor-hub.yaml
description: Sensor Hub Device

compatible: "sensor-hub"

properties:
  sensors:
    type: phandle-array
    description: List of connected sensors
    
  polling-interval:
    type: int
    default: 1000
    description: Sensor polling interval in milliseconds
```

**Step 2:** Use custom binding:

```dts
// apps/sensor_hub/app.overlay
/ {
    sensor_hub: sensor-hub {
        compatible = "sensor-hub";
        sensors = <&temperature_sensor>;
        polling-interval = <500>;
    };
};
```

### Exercise 6.2: Build-Time Device Tree Analysis

**Step 1:** Examine generated device tree:

```bash
west build -b rpi_4b

# View final device tree
dtc -I dtb -O dts build/zephyr/zephyr.dts.pre.tmp > final.dts
cat final.dts | grep -A 10 sensor-hub
```

**Step 2:** Debug device tree issues:

```bash
# Check device tree compiler output
west build -v 2>&1 | grep -E "(dtc|device.tree)"

# Validate device tree syntax
dtc -I dts -O dtb app.overlay -o /tmp/test.dtb
```

---

## Troubleshooting Common Build Issues

### Issue 1: Missing Dependencies

**Symptoms:**
```
CMake Error: find_package could not find module FindZephyr
```

**Solution:**
```bash
# Ensure ZEPHYR_BASE is set
source ~/zephyrproject/zephyr/zephyr-env.sh

# Verify West workspace
west topdir
```

### Issue 2: Toolchain Problems

**Symptoms:**
```
arm-zephyr-eabi-gcc: command not found
```

**Solution:**
```bash
# Check SDK installation (update to current version)
echo $ZEPHYR_SDK_INSTALL_DIR
ls $ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/

# Check SDK version
cat $ZEPHYR_SDK_INSTALL_DIR/sdk_version

# Reinstall SDK if necessary
west sdk install
```

### Issue 3: Configuration Conflicts

**Symptoms:**
```
Configuration 'CONFIG_SENSOR' has unmet dependencies
```

**Solution:**
```bash
# Use menuconfig to resolve
west build -t menuconfig

# Check dependency requirements
west build -t help
```

---

## Lab Summary and Next Steps

### What You've Accomplished

1. **West Mastery:** Multi-repository management and synchronization
2. **CMake Expertise:** Modular builds and advanced configuration
3. **Build Optimization:** Performance tuning and debugging techniques
4. **Device Tree Integration:** Custom hardware descriptions and bindings
5. **Automation:** Scripts and workflows for efficient development

### Verification Checklist

- [ ] Successfully built multi-module application
- [ ] Created board-specific configurations
- [ ] Implemented build automation scripts
- [ ] Debugged and optimized build performance
- [ ] Integrated custom device tree elements

### Preparing for Chapter 4

The build system knowledge you've gained is essential for the configuration management topics in Chapter 4. You'll use these skills to:

- Customize Kconfig options for specific applications
- Manage complex configuration dependencies
- Optimize system resources for your target hardware

Your comprehensive understanding of the build system positions you perfectly for advanced Zephyr development in the upcoming chapters.

[Next: Chapter 4: Configure Zephyr](./../chapter_04_configure_zephyr/README.md)
