# Chapter 14 - Modules Theory

## 14.1 Understanding Zephyr Modules

Zephyr modules are self-contained units of functionality that can be shared across multiple projects. They provide a standardized way to organize, distribute, and reuse code components in the Zephyr ecosystem.

### Key Benefits of Modules

*   **Code Reusability**: Write once, use in multiple projects.
*   **Maintainability**: Isolated functionality is easier to maintain and debug.
*   **Scalability**: Large projects can be broken into manageable components.
*   **Community Sharing**: Modules can be shared with the broader Zephyr community.
*   **Version Control**: Independent versioning and dependency management.

## 14.2 Module Architecture

A well-structured Zephyr module follows this organization:

```text
my_module/
├── zephyr/
│   ├── module.yml          # Module metadata
│   ├── CMakeLists.txt      # Build configuration
│   └── Kconfig             # Configuration options
├── include/
│   └── my_module/
│       └── api.h           # Public API headers
├── src/
│   ├── my_module.c         # Implementation
│   └── internal.h          # Private headers
├── dts/
│   └── bindings/           # Device tree bindings
├── tests/
│   └── src/
│       └── test_my_module.c # Unit tests
└── samples/
    └── basic/
        ├── src/
        │   └── main.c      # Example usage
        ├── CMakeLists.txt
        └── prj.conf
```

### `module.yml`

This file contains metadata about the module, such as its name, version, and dependencies.

```yaml
name: my_module
version: "1.0"
description: A simple module to toggle a GPIO pin.
maintainer: "Your Name"
contact: "your.email@example.com"
homepage: "https://example.com"
remote: "https://github.com/yourusername/my_module"
license: MIT
dependencies:
  - zephyr
```

### `CMakeLists.txt`

This file provides instructions for building the module.

```cmake
# modules/my_module/zephyr/CMakeLists.txt

zephyr_library()

zephyr_library_sources(../src/my_module.c)

zephyr_include_directories(../include)
```

This tells the Zephyr build system to compile the source files and add the `include` directory to the search path.

### `Kconfig`

This file defines the configuration options for the module.

```kconfig
# modules/my_module/zephyr/Kconfig

config MY_MODULE
    bool "Enable My Module"
    default n
    help
      Enable custom module functionality.
```

## 14.3 Integrating Modules with West

To make your module available to other projects, you host it in a Git repository and add it to your application's West manifest file (`west.yml`).

### West Manifest (`west.yml`)

```yaml
manifest:
  remotes:
    - name: my-github
      url-base: https://github.com/my-username
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: main
      import: true
    - name: my_custom_module
      remote: my-github
      revision: main
      path: modules/my_custom_module
```

After adding the module to your manifest, run `west update` to clone the repository into the specified path. The Zephyr build system will automatically discover the module and its Kconfig and build files.

## 14.4 Application Integration

### Main Application (`src/main.c`)

```c
#include <zephyr/kernel.h>
#include <my_module/api.h>

int main(void)
{
#ifdef CONFIG_MY_MODULE
    my_module_init();
    my_module_do_work();
#else
    printk("My Custom Module is not enabled.\n");
#endif
    return 0;
}
```

### Project Configuration (`prj.conf`)

Enable your module and its features in your application's `prj.conf` file:

```kconfig
CONFIG_MY_MODULE=y
```

## 14.5 Advanced Module Concepts

### Module Dependencies

Your module can depend on other Zephyr subsystems or modules. Use `select` in your `Kconfig` file to automatically enable dependencies:

```kconfig
config MY_MODULE
    bool "Enable My Custom Module"
    select GPIO
    select ADC
    help
      This module requires GPIO and ADC support.
```

### Conditional Compilation

Use `zephyr_library_sources_ifdef` in your module's `CMakeLists.txt` to conditionally compile files:

```cmake
# Conditionally compile feature_x.c if the Kconfig option is enabled
zephyr_library_sources_ifdef(CONFIG_MY_MODULE_FEATURE_X src/feature_x.c)
```

### Device Tree Bindings

If your module includes a driver, provide device tree bindings in the `dts/bindings` directory. This allows users to configure the hardware for your driver in their board's overlay file.

This modular approach promotes code reusability, maintainability, and scalability—essential for building robust and complex embedded systems.
