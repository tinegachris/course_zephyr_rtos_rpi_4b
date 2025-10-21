# Chapter 5: West - Lab

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../README.md)

---

This lab provides hands-on experience with West workspace management, manifest creation, and multi-repository development workflows. You'll build practical skills using your Raspberry Pi 4B while learning professional West techniques used in production Zephyr development.

---

## Prerequisites and Setup

Before starting this lab, ensure you have:

- **Hardware:** Raspberry Pi 4B with SD card and development setup
- **Software Environment:** Working Zephyr development environment from previous chapters
- **Completed:** Chapters 1-4 of this course
- **Git Knowledge:** Basic understanding of Git commands and repository management

**Verify Your Current Environment:**

```bash
# Check if you have an existing West workspace
ls -la ~/zephyrproject
echo $ZEPHYR_BASE

# Verify Git configuration
git config --global user.name
git config --global user.email

# If not configured, set them now
git config --global user.name "Your Name"
git config --global user.email "your.email@domain.com"
```

---

## Lab Exercise 1: Understanding West Workspaces

### Objective

Explore and analyze an existing West workspace to understand its structure, manifest configuration, and repository relationships.

### Step 1: Analyze Current Workspace Structure

**Examine your existing Zephyr workspace:**

```bash
cd ~/zephyrproject

# Display workspace structure
tree -L 2 -a .

# Examine West configuration
ls -la .west/
cat .west/config

# Show manifest file location
cat .west/config | grep -A 2 "\[manifest\]"
```

**Understand the manifest file:**

```bash
# View the current manifest
cat zephyr/west.yml | head -50

# Count total projects in manifest
grep -c "^  - name:" zephyr/west.yml

# Show remotes configuration
grep -A 10 "remotes:" zephyr/west.yml
```

### Step 2: Explore West Commands

**Repository status and information:**

```bash
# Show all projects in workspace
west list

# Display detailed project information
west list -f "{name:20} {path:30} {revision:12}"

# Check status of all repositories
west status

# Show any local modifications
west diff --name-only
```

**Advanced workspace queries:**

```bash
# Show projects with specific groups
west list -f "{name:20} {groups}" | grep -v "None"

# Find projects with local changes
west forall -c "git status --porcelain" 2>/dev/null | grep -B1 -A5 "^[AMDRC]"

# Show commit information across repositories
west log --oneline -n 3
```

### Step 3: Workspace Configuration

**Examine and modify West configuration:**

```bash
# Show all configuration settings
west config

# Set workspace-specific defaults
west config build.board rpi_4b
west config build.pristine auto
west config build.cmake-args -- -DCMAKE_VERBOSE_MAKEFILE=ON

# Verify configuration changes
west config | grep "build\."
```

---

## Lab Exercise 2: Creating Custom Manifests

### Objective

Create custom manifest files for different development scenarios, understanding how to manage dependencies and repository versions.

### Step 1: Create a Development Manifest

**Set up the exercise directory:**

```bash
cd ~
mkdir west_lab
cd west_lab

# Create a manifest repository
mkdir custom-manifest
cd custom-manifest
git init
```

**Create a development-focused manifest:**

```bash
cat > west.yml << 'EOF'
# Development Manifest for IoT Sensor Project
# Includes development tools and testing frameworks
manifest:
  defaults:
    remote: upstream
    revision: main
    
  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
    - name: zephyr-testing
      url-base: https://github.com/zephyrproject-rtos
      
  projects:
    # Core Zephyr with specific stable version
    - name: zephyr
      revision: v3.5.0
      west-commands: scripts/west-commands.yml
      import: true
      
    # Additional testing and development tools
    - name: net-tools
      remote: zephyr-testing
      revision: f49bd1354616fae4093bf36e5eaee43c51a55127
      path: tools/net-tools
      groups:
        - tools
        
    # Custom application placeholder
    - name: sensor-app
      url: https://github.com/example/sensor-app.git
      revision: develop
      path: applications/sensor-app
      groups:
        - applications
      
  group-filter: [+tools, +applications]
  
  self:
    path: manifests
EOF
```

**Create a production manifest:**

```bash
cat > west-production.yml << 'EOF'
# Production Manifest for IoT Sensor Project
# Locked versions for stable deployment
manifest:
  defaults:
    remote: upstream
    
  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
    - name: production
      url-base: https://github.com/mycompany/production
      
  projects:
    # Zephyr locked to specific commit for stability
    - name: zephyr
      revision: 8a63b32f1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f
      west-commands: scripts/west-commands.yml
      import: 
        # Only import essential modules, exclude testing
        name-allowlist: 
          - hal_nordic
          - cmsis
          - mbedtls
          - lvgl
        
    # Production application with tagged release
    - name: sensor-app
      remote: production
      revision: v2.1.0
      path: applications/sensor-app
      
  # Exclude all development and testing groups
  group-filter: [-babblesim, -optional, -testing]
  
  self:
    path: manifests
EOF
```

### Step 2: Test Custom Manifests

**Initialize workspace with development manifest:**

```bash
# Commit manifest files
git add .
git commit -m "Add development and production manifests"

# Create development workspace
cd ~/west_lab
mkdir dev-workspace
cd dev-workspace

# Initialize with development manifest
west init -l ../custom-manifest/
west update --stats

# Verify workspace contents
west list -f "{name:20} {path:30} {groups}"
```

**Test manifest switching:**

```bash
# Switch to production manifest
west config manifest.file west-production.yml
west update --stats

# Compare project lists
echo "=== Development Projects ==="
west config manifest.file west.yml
west list | wc -l

echo "=== Production Projects ==="
west config manifest.file west-production.yml
west list | wc -l
```

### Step 3: Manifest Validation and Debugging

**Validate manifest syntax:**

```bash
# Check manifest validity
west manifest --validate

# Show resolved manifest content
west manifest --resolve | head -50

# Debug manifest processing
west --verbose manifest --resolve > resolved-manifest.yml
```

---

## Lab Exercise 3: Multi-Repository Development Workflow

### Objective

Implement a professional development workflow using multiple repositories, feature branches, and coordinated updates across the workspace.

### Step 1: Set Up Multi-Repository Project

**Create a realistic project structure:**

```bash
cd ~/west_lab
mkdir iot-project
cd iot-project

# Create application repository
mkdir sensor-application
cd sensor-application
git init

# Create basic application structure
mkdir -p src include boards/rpi_4b
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iot_sensor)

target_sources(app PRIVATE
    src/main.c
    src/sensor_manager.c
    src/data_processor.c
)

target_include_directories(app PRIVATE include)
EOF

cat > prj.conf << 'EOF'
# IoT Sensor Application Configuration
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Sensor support
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_GPIO=y

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3

# Networking (for IoT connectivity)
CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_UDP=y
CONFIG_NET_DHCPV4=y
EOF

cat > src/main.c << 'EOF'
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(iot_sensor, LOG_LEVEL_INF);

#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED_NODE, gpios, {0});

int main(void)
{
    LOG_INF("IoT Sensor Application Starting");
    LOG_INF("Target: %s", CONFIG_BOARD);
    
    // Initialize LED
    if (gpio_is_ready_dt(&led)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        LOG_INF("LED initialized successfully");
    } else {
        LOG_WRN("LED not available");
    }
    
    while (1) {
        if (gpio_is_ready_dt(&led)) {
            gpio_pin_toggle_dt(&led);
        }
        
        LOG_INF("Sensor reading cycle");
        k_sleep(K_SECONDS(2));
    }
    
    return 0;
}
EOF

cat > src/sensor_manager.c << 'EOF'
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_mgr, LOG_LEVEL_DBG);

void sensor_manager_init(void)
{
    LOG_INF("Sensor manager initialized");
}

int sensor_read_temperature(float *temperature)
{
    // Simulated temperature reading
    *temperature = 23.5f;
    LOG_DBG("Temperature reading: %.1f°C", *temperature);
    return 0;
}
EOF

cat > src/data_processor.c << 'EOF'
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(data_proc, LOG_LEVEL_DBG);

void data_processor_init(void)
{
    LOG_INF("Data processor initialized");
}

int process_sensor_data(float temperature)
{
    LOG_DBG("Processing temperature: %.1f°C", temperature);
    
    // Simple processing logic
    if (temperature > 25.0f) {
        LOG_WRN("High temperature detected: %.1f°C", temperature);
        return 1;
    }
    
    return 0;
}
EOF

# Create header files
cat > include/sensor_manager.h << 'EOF'
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

void sensor_manager_init(void);
int sensor_read_temperature(float *temperature);

#endif
EOF

cat > include/data_processor.h << 'EOF'
#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

void data_processor_init(void);
int process_sensor_data(float temperature);

#endif
EOF

# Create board-specific overlay
cat > boards/rpi_4b.overlay << 'EOF'
/ {
    aliases {
        led0 = &status_led;
    };

    leds {
        compatible = "gpio-leds";
        status_led: led_0 {
            gpios = <&gpio0 18 GPIO_ACTIVE_HIGH>;
            label = "Status LED";
        };
    };
};

&gpio0 {
    status = "okay";
};
EOF

git add .
git commit -m "Initial IoT sensor application"
cd ..
```

**Create custom drivers repository:**

```bash
mkdir custom-drivers
cd custom-drivers
git init

mkdir -p drivers/sensor include/drivers
cat > drivers/sensor/custom_temp_sensor.c << 'EOF'
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(custom_temp_sensor, LOG_LEVEL_DBG);

struct custom_temp_data {
    float temperature;
};

static int custom_temp_sample_fetch(const struct device *dev,
                                  enum sensor_channel chan)
{
    struct custom_temp_data *data = dev->data;
    
    // Simulate sensor reading
    data->temperature = 22.0f + (k_uptime_get() % 100) / 10.0f;
    
    LOG_DBG("Fetched temperature: %.1f°C", data->temperature);
    return 0;
}

static int custom_temp_channel_get(const struct device *dev,
                                 enum sensor_channel chan,
                                 struct sensor_value *val)
{
    struct custom_temp_data *data = dev->data;
    
    if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
        return -ENOTSUP;
    }
    
    val->val1 = (int32_t)data->temperature;
    val->val2 = ((int32_t)(data->temperature * 1000000)) % 1000000;
    
    return 0;
}

static const struct sensor_driver_api custom_temp_api = {
    .sample_fetch = custom_temp_sample_fetch,
    .channel_get = custom_temp_channel_get,
};

static struct custom_temp_data custom_temp_data_0;

DEVICE_DT_INST_DEFINE(0, NULL, NULL,
                      &custom_temp_data_0, NULL,
                      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
                      &custom_temp_api);
EOF

cat > include/drivers/custom_temp_sensor.h << 'EOF'
#ifndef CUSTOM_TEMP_SENSOR_H
#define CUSTOM_TEMP_SENSOR_H

#include <zephyr/drivers/sensor.h>

#define CUSTOM_TEMP_SENSOR_NODE DT_INST(0, custom_temp_sensor)

#endif
EOF

git add .
git commit -m "Add custom temperature sensor driver"
cd ..
```

### Step 2: Create Comprehensive Project Manifest

**Create project manifest:**

```bash
cd ~/west_lab/iot-project
mkdir manifests
cd manifests
git init

cat > west.yml << 'EOF'
      url-base: file://..
      
  projects:
    # Core Zephyr RTOS
    - name: zephyr
      revision: v3.5.0
      west-commands: scripts/west-commands.yml
      import: true
      
    # Our application code
    - name: sensor-application
      remote: local
      path: app
      groups: [app]
      
    # Custom drivers
    - name: custom-drivers
      remote: local
      path: modules/drivers/custom
      groups: [drivers]
      
  group-filter: [+app, +drivers]
  
  self:
    path: manifests
EOF

git add west.yml
git commit -m "Initial IoT project manifest"
cd ..
```

### Step 3: Test Multi-Repository Workflow

**Initialize and test the workspace:**

```bash
cd ~/west_lab
mkdir iot-workspace
cd iot-workspace

# Initialize workspace
west init -l ../iot-project/manifests/
west update

# Verify workspace structure
tree -L 3 .

# Examine project layout
west list -f "{name:20} {path:25} {groups}"
```

**Test building the application:**

```bash
# Build the IoT application
west build -b rpi_4b app

# Check build output
ls -la build/zephyr/

# Test with debug configuration
echo 'CONFIG_LOG_DEFAULT_LEVEL=4' >> app/prj.conf
west build -b rpi_4b --pristine app

# Verify debug output in configuration
grep LOG_DEFAULT_LEVEL build/zephyr/include/generated/autoconf.h
```

### Step 4: Feature Development Workflow

**Implement feature branch workflow:**

```bash
# Create feature branch across repositories
west forall -c "git checkout -b feature/enhanced-logging"

# Modify application for enhanced logging
cat >> app/src/main.c << 'EOF'

void log_system_info(void) 
{
    LOG_INF("=== System Information ===");
    LOG_INF("Kernel version: %s", KERNEL_VERSION_STRING);
    LOG_INF("Board: %s", CONFIG_BOARD);
    LOG_INF("CPU: %s", CONFIG_SOC);
    LOG_INF("Free memory: %zu bytes", k_heap_free_get(&k_malloc_heap));
    LOG_INF("==========================");
}
EOF

# Update main function to call system info
sed -i '/LOG_INF("IoT Sensor Application Starting");/a\    log_system_info();' app/src/main.c

# Commit changes in application
cd app
git add .
git commit -m "Add enhanced system information logging"
cd ..

# Update custom driver with more detailed logging
sed -i 's/LOG_DBG("Fetched temperature:/LOG_INF("Sensor fetch - temperature:/g' modules/drivers/custom/drivers/sensor/custom_temp_sensor.c

cd modules/drivers/custom
git add .
git commit -m "Enhance driver logging for debugging"
cd ../../..

# Test the enhanced feature
west build -b rpi_4b --pristine app

# Show git status across all repositories
west status
```

---

## Lab Exercise 4: Advanced West Configuration

### Objective

Configure advanced West features including build defaults, extension commands, and workspace automation.

### Step 1: Advanced Configuration Setup

**Configure build defaults:**

```bash
cd ~/west_lab/iot-workspace

# Set comprehensive build defaults
west config build.board rpi_4b
west config build.pristine auto
west config build.dir-fmt "build-{board}-{source_dir}"
west config build.cmake-args -- "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"

# Configure update behavior
west config update.sync true
west config update.narrow false

# Set debugging preferences
west config runner.openocd.gdb-port 3333
west config runner.gdb.exe arm-none-eabi-gdb
```

**Create custom West extension commands:**

```bash
mkdir -p tools/west-commands
cat > tools/west-commands/custom_commands.py << 'EOF'
"""Custom West commands for IoT project development"""

import os
import subprocess
from west.commands import WestCommand
from west import log

class ProjectInfo(WestCommand):
    def __init__(self):
        super().__init__(
            'project-info',
            'show detailed project information',
            'Display comprehensive information about the IoT project workspace')

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name, help=self.help)
        parser.add_argument('--verbose', '-v', action='store_true',
                          help='show verbose output')
        return parser

    def do_run(self, args, unknown_args):
        log.inf("=== IoT Project Information ===")
        
        # Show workspace info
        result = subprocess.run(['west', 'topdir'], capture_output=True, text=True)
        if result.returncode == 0:
            log.inf(f"Workspace root: {result.stdout.strip()}")
        
        # Show active projects
        result = subprocess.run(['west', 'list', '--format={name}'], 
                              capture_output=True, text=True)
        if result.returncode == 0:
            projects = result.stdout.strip().split('\n')
            log.inf(f"Active projects: {len(projects)}")
            if args.verbose:
                for project in projects:
                    log.inf(f"  - {project}")
        
        # Show build configuration
        result = subprocess.run(['west', 'config', 'build.board'], 
                              capture_output=True, text=True)
        if result.returncode == 0:
            log.inf(f"Default board: {result.stdout.strip()}")

class QuickBuild(WestCommand):
    def __init__(self):
        super().__init__(
            'quick-build',
            'build with predefined configurations',
            'Quick build command with common configuration presets')

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name, help=self.help)
        parser.add_argument('preset', choices=['debug', 'release', 'test'],
                          help='build preset to use')
        parser.add_argument('--board', '-b', 
                          help='target board (overrides default)')
        return parser

    def do_run(self, args, unknown_args):
        board = args.board or 'rpi_4b'
        
        config_files = {
            'debug': 'prj.conf',
            'release': 'prj.conf',  # Could add release-specific config
            'test': 'prj.conf'      # Could add test-specific config
        }
        
        build_args = ['west', 'build', '-b', board, '--pristine']
        
        if args.preset == 'debug':
            build_args.extend(['--', '-DCONF_FILE=prj.conf', 
                             '-DCMAKE_BUILD_TYPE=Debug'])
        elif args.preset == 'release':
            build_args.extend(['--', '-DCONF_FILE=prj.conf',
                             '-DCMAKE_BUILD_TYPE=Release'])
        elif args.preset == 'test':
            build_args.extend(['--', '-DCONF_FILE=prj.conf',
                             '-DCONFIG_ZTEST=y'])
        
        build_args.append('app')
        
        log.inf(f"Building {args.preset} configuration for {board}")
        subprocess.run(build_args)

# Register commands
WEST_COMMANDS = [ProjectInfo(), QuickBuild()]
EOF

cat > tools/west-commands.yml << 'EOF'
west-commands:
  - file: tools/west-commands/custom_commands.py
EOF

# Add custom commands to manifest
cd manifests
sed -i '/self:/a\    west-commands: ../tools/west-commands.yml' west.yml
git add ../tools/
git add west.yml
git commit -m "Add custom West extension commands"
cd ..

# Update workspace to include new commands
west update
```

### Step 2: Test Advanced Features

**Test custom commands:**

```bash
# Test project info command
west project-info
west project-info --verbose

# Test quick build presets
west quick-build debug
west quick-build release --board rpi_4b

# Verify build outputs
ls -la build*/zephyr/zephyr.elf
```

**Advanced workspace operations:**

```bash
# Show comprehensive workspace status
west status --verbose

# Perform batch operations across repositories
west forall -c "git log --oneline -3"

# Check for security vulnerabilities in dependencies
west forall -c "git fsck --full" | grep -i error || echo "All repositories healthy"

# Generate workspace documentation
west list -f "{name:15} {url:50} {revision:10}" > workspace-inventory.txt
cat workspace-inventory.txt
```

---

## Lab Exercise 5: Production Deployment Workflow

### Objective

Implement a production-ready deployment workflow with version locking, automated testing, and release management.

### Step 1: Create Release Management System

**Set up release configuration:**

```bash
cd ~/west_lab/iot-project/manifests

# Create production release manifest
cat > west-release-v1.0.yml << 'EOF'
# IoT Project Production Release v1.0
# All components locked to tested versions
manifest:
  defaults:
    remote: upstream

  remotes:
    - name: upstream
      url-base: https://github.com/zephyrproject-rtos
    # Note: Using a relative file path for a local manifest repository.
    # This is useful for self-contained projects like this lab.
    - name: local      url-base: file:///home/user/west_lab/iot-project

  projects:
    # Zephyr locked to LTS version
    - name: zephyr
      revision: v3.5.0  # LTS release
      west-commands: scripts/west-commands.yml
      import:
        # Only essential modules for production
        name-allowlist:
          - cmsis
          - hal_rpi_pico
          - mbedtls
          - tinycrypt
        name-blocklist:
          - bsim
          - babblesim
          - openthread
      
    # Application locked to release tag
    - name: sensor-application
      remote: local
      revision: HEAD  # Would be a release tag in real scenario
      path: app
      
    # Drivers locked to stable version
    - name: custom-drivers
      remote: local  
      revision: HEAD  # Would be a release tag in real scenario
      path: modules/drivers/custom
      
  # Exclude all non-production groups
  group-filter: [-babblesim, -optional, -testing]
  
  self:
    path: manifests
EOF

git add west-release-v1.0.yml
git commit -m "Add production release manifest v1.0"
```

**Create automated build and test script:**

```bash
cd ~/west_lab/iot-project
cat > tools/build-release.sh << 'EOF'
#!/bin/bash
set -e

RELEASE_VERSION=$1
BOARDS=("rpi_4b")

if [ -z "$RELEASE_VERSION" ]; then
    echo "Usage: $0 <release-version>"
    echo "Example: $0 v1.0.0"
    exit 1
fi

echo "=== Building Release $RELEASE_VERSION ==="

# Create release workspace
RELEASE_DIR="release-$RELEASE_VERSION"
rm -rf "$RELEASE_DIR"
mkdir "$RELEASE_DIR"
cd "$RELEASE_DIR"

# Initialize with release manifest
west init -l "../manifests/" -m "west-release-$RELEASE_VERSION.yml"
west update --stats

echo "=== Workspace initialized ==="
west list

# Build for all supported boards
for board in "${BOARDS[@]}"; do
    echo "=== Building for $board ==="
    west build -b "$board" --pristine app
    
    # Copy binary to release artifacts
    mkdir -p "artifacts/$board"
    cp "build/zephyr/zephyr.elf" "artifacts/$board/"
    cp "build/zephyr/zephyr.bin" "artifacts/$board/" 2>/dev/null || true
    cp "build/zephyr/zephyr.hex" "artifacts/$board/" 2>/dev/null || true
    
    # Generate build info
    echo "Build Date: $(date)" > "artifacts/$board/build-info.txt"
    echo "Release: $RELEASE_VERSION" >> "artifacts/$board/build-info.txt"
    echo "Board: $board" >> "artifacts/$board/build-info.txt"
    echo "Zephyr Version: $(cat zephyr/VERSION)" >> "artifacts/$board/build-info.txt"
    
    # Clean build directory for next iteration
    rm -rf build/
done

echo "=== Release Build Complete ==="
echo "Artifacts available in: $(pwd)/artifacts/"
ls -la artifacts/*/
EOF

chmod +x tools/build-release.sh
```

### Step 2: Test Release Workflow

**Execute release build:**

```bash
cd ~/west_lab/iot-project

# Test the release build process
./tools/build-release.sh v1.0

# Examine release artifacts
ls -la release-v1.0/artifacts/
cat release-v1.0/artifacts/rpi_4b/build-info.txt

# Verify binary size and contents
file release-v1.0/artifacts/rpi_4b/zephyr.elf
size release-v1.0/artifacts/rpi_4b/zephyr.elf
```

**Create release validation script:**

```bash
cat > tools/validate-release.sh << 'EOF'
#!/bin/bash
set -e

RELEASE_DIR=$1

if [ ! -d "$RELEASE_DIR" ]; then
    echo "Release directory $RELEASE_DIR not found"
    exit 1
fi

echo "=== Validating Release: $RELEASE_DIR ==="

cd "$RELEASE_DIR"

# Validate workspace integrity
echo "Checking workspace integrity..."
west status | grep -q "^$" && echo "✓ All repositories clean" || echo "✗ Uncommitted changes found"

# Validate build artifacts
echo "Checking build artifacts..."
for board_dir in artifacts/*/; do
    board=$(basename "$board_dir")
    echo "Validating $board artifacts..."
    
    if [ -f "$board_dir/zephyr.elf" ]; then
        echo "✓ ELF binary exists"
        file_output=$(file "$board_dir/zephyr.elf")
        echo "  Type: $file_output"
    else
        echo "✗ ELF binary missing"
    fi
    
    if [ -f "$board_dir/build-info.txt" ]; then
        echo "✓ Build info exists"
        echo "  $(cat "$board_dir/build-info.txt" | head -2)"
    else
        echo "✗ Build info missing"
    fi
done

# Generate release report
echo "=== Release Report ===" > release-report.txt
date >> release-report.txt
west list -f "{name:20} {revision:40}" >> release-report.txt
echo "" >> release-report.txt
echo "Build Artifacts:" >> release-report.txt
find artifacts/ -name "*.elf" -exec ls -lh {} \; >> release-report.txt

echo "✓ Release validation complete"
echo "Report saved to: $(pwd)/release-report.txt"
EOF

chmod +x tools/validate-release.sh

# Validate the release
./tools/validate-release.sh release-v1.0

# Review validation report
cat release-v1.0/release-report.txt
```

---

## Lab Summary and Best Practices

### Key Skills Developed

Through these exercises, you have mastered:

1. **West Workspace Management:** Understanding workspace structure, manifest configuration, and repository relationships
2. **Custom Manifest Creation:** Building development, production, and testing manifest configurations
3. **Multi-Repository Workflows:** Coordinating development across multiple repositories with proper branching strategies
4. **Advanced West Configuration:** Implementing custom commands, build defaults, and automation scripts
5. **Production Deployment:** Creating reproducible release processes with version locking and validation

### Professional Development Workflows

**Development Phase Best Practices:**
- Use development manifests with latest stable versions
- Enable comprehensive testing and debugging tools
- Implement feature branch workflows across all repositories
- Maintain clean commit history in all repositories

**Production Release Best Practices:**
- Lock all dependencies to specific commit hashes or stable tags
- Exclude testing and development-only modules from production builds
- Implement automated build and validation processes
- Generate comprehensive build reports and artifact inventories

**Team Collaboration Best Practices:**
- Use shared manifest repositories for team synchronization
- Implement consistent configuration across all team members
- Document manifest changes with clear commit messages
- Use group filters to manage different team roles and requirements

### Real-World Applications

The techniques learned in this lab directly apply to:

- **IoT Product Development:** Managing multiple hardware variants and software configurations
- **Embedded Systems Teams:** Coordinating development across distributed teams with consistent environments
- **Continuous Integration:** Implementing reproducible builds in automated pipelines
- **Version Control:** Maintaining precise control over all project dependencies

### Next Steps

With West mastery complete, you're prepared for:
- **Advanced Zephyr Development:** Complex applications requiring multiple modules and dependencies
- **Custom Board Support:** Creating board support packages with proper manifest integration
- **Production Deployment:** Implementing professional-grade release management processes
- **Team Leadership:** Architecting development workflows for embedded systems teams

West provides the foundation for scalable, maintainable Zephyr development. These lab exercises have equipped you with production-ready skills for professional embedded development environments.

[Next Chapter 6: Zephyr Fundamentals](../chapter_06_zephyr_fundamentals/README.md)
