# Chapter 14 - Modules Theory

## 14.1 Understanding Zephyr Modules

Zephyr modules are self-contained units of functionality that can be shared across multiple projects. They provide a standardized way to organize, distribute, and reuse code components in the Zephyr ecosystem.

### Key Benefits of Modules

* **Code Reusability**: Write once, use in multiple projects
* **Maintainability**: Isolated functionality is easier to maintain and debug
* **Scalability**: Large projects can be broken into manageable components
* **Community Sharing**: Modules can be shared with the broader Zephyr community
* **Version Control**: Independent versioning and dependency management

## 14.2 Module Architecture

### Module Directory Structure

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

### Creating a New Module

Let's create a simple module that toggles a GPIO pin. This example illustrates the fundamental steps involved in defining and building a Zephyr module.

**1. Project Structure:**

The key to understanding modules is recognizing their directory structure.  We'll establish the following:

```
my_module/
├── zephyr/
│   ├── module.yml          # Module metadata
│   ├── CMakeLists.txt      # Build configuration
│   └── Kconfig             # Configuration options
├── include/
│   └── my_module/
│       └── api.h           # Public API
├── src/
│   └── my_module.c         # Implementation
└── dts/
    └── bindings/           # Device tree bindings
```

**2. `module.yml`:**

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

**3. `CMakeLists.txt`:**

This file provides instructions for building the module.

```cmake
zephyr_library_sources_ifdef(
    src/my_module.c
    TARGET_MODULE_NAME
)
```
This line conditionally includes the `my_module.c` file in the build process, based on the `TARGET_MODULE_NAME` variable, which we'll configure in the Kconfig.

**4. `Kconfig`:**

This file defines the configuration options for the module.

```kconfig
config MY_MODULE
    bool "Enable My Module"
    default n
    help
      Enable custom module functionality.
```

**5. `include/my_module/api.h`:**

This header file defines the public API for the module.

```c
#ifndef MY_MODULE_API_H
#define MY_MODULE_API_H

#include <zephyr/sys/types.h>

// Function to toggle the GPIO pin
void my_module_toggle_gpio(int gpio_num);

#endif
```

**6. `src/my_module.c`:**

This file contains the implementation of the module's functionality.

```c
#include <zephyr/sys/types.h>
#include "api.h"

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define GPIO_PORT 1  // Replace with the appropriate GPIO port
#define GPIO_PIN 1   // Replace with the appropriate GPIO pin

void my_module_toggle_gpio(int gpio_num)
{
    // Check if the GPIO is configured
    if (!GPIO_IS_ENABLED()) {
        return;
    }

    // Toggle the GPIO pin
    gpio_toggle(GPIO_PORT, GPIO_PIN);

    // Optional: Add logging
    printf("GPIO %d toggled\n", gpio_num);
}

// Utility function to enable GPIO
void GPIO_IS_ENABLED()
{
    return gpio_enable_request(GPIO_PORT, GPIO_PIN, 1);
}

```

**7. Build and Test:**

1.  **Configuration:** Open your Zephyr project's configuration file (`prj.conf`). Ensure the `CONFIG_MY_MODULE` option is set to `y`.
2.  **Build:** Use `west build -b <board_name>` to build the module. Replace `<board_name>` with your target board's name.  For example, `west build -b qemu_x86` for QEMU.
3.  **Test:**  After the build completes, run the module code.  You'll need to add a way to invoke `my_module_toggle_gpio` from somewhere.  A simple way to do this is to add a `main()` function within the same project, or another entry point.

## Module Structure

The organization of a module is critical for maintainability and reusability. Here’s a breakdown of the key components and best practices:

*   **`zephyr/module.yml`:** As described above, this file is mandatory and contains metadata about the module.
*   **`include/my_module/api.h`:**  Defines the public API – the functions and data structures that users of the module can access. This helps to isolate the module's internal implementation.
*   **`src/my_module.c`:** Contains the implementation of the module’s functionality.
*   **`dts/bindings/`:** (Optional) If your module interacts with specific hardware, you'll define device tree bindings here to configure the hardware.
* **Testing:** Ensure thorough unit tests are included for any new or modified functionality.

## Integration with Zephyr Subsystems

Modules seamlessly integrate with other Zephyr subsystems:

*   **Threads:**  Modules can be implemented as threads, enabling concurrent execution and asynchronous operation.
*   **Logging:** Leverage Zephyr's logging subsystem for debugging and monitoring.
*   **Memory Management:** Utilize Zephyr's memory management features for efficient allocation and deallocation.
*   **Device Drivers:** Modules can be integrated as device drivers, interacting directly with hardware.


---

This provides a solid foundation for understanding modules in Zephyr.  Remember to adapt the code examples to your specific needs and always refer to the Zephyr documentation for the latest information and best practices. This approach emphasizes a modular design, promoting code reusability, maintainability, and scalability—essential for building robust and complex embedded systems.