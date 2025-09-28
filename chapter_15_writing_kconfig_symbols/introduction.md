## Chapter 15 - Writing Kconfig Symbols

### 1. Introduction (532 words)

This chapter delves into a powerful and often overlooked aspect of Zephyr RTOS development: the creation and use of Kconfig symbols. While Chapters 1-14 have equipped you with the foundational knowledge of building, configuring, and interacting with Zephyr, crafting effective Kconfig symbols unlocks a deeper level of customization and integration, particularly crucial for modern embedded systems.  Think of Kconfig symbols as the building blocks that define the configurable options available during the Zephyr build process. They’re not just about toggling features; they're the mechanism by which you tailor your Zephyr applications to specific hardware platforms, target performance profiles, and application requirements.

In the world of embedded systems, a "one-size-fits-all" approach rarely works.  Consider a drone control system. Should it prioritize low latency for precise control, or low power consumption for extended flight time?  Or, let's say you're developing a sensor network – do you need support for multiple sensor types, and if so, what communication protocols will you use? Kconfig symbols let you answer these questions directly into the build process.

The impact of Kconfig symbols is amplified when considering industry applications. Automotive systems, for example, require stringent safety certifications, dictating specific hardware configurations and feature selections. Medical devices demand precise timing and reliability, heavily influencing the choice of real-time capabilities.  Industrial automation relies on robust communication protocols and configurable performance.  Properly leveraging Kconfig allows developers to create Zephyr applications that meet these demanding requirements.

This chapter builds upon the concepts introduced in previous chapters. You’ll revisit the configuration system you’ve already learned about – the core of selecting modules and settings.  However, we’re now shifting our focus from merely *selecting* to *defining* the choices available to the user.  You’ve learned how to interact with GPIO, I2C, and device trees. Now, Kconfig symbols provide a standardized and powerful way to control which of those interactions are enabled during the build, influencing the resulting application's behavior.  Essentially, you're becoming a configuration architect for your Zephyr application.

Throughout this chapter, you'll discover how Kconfig symbols are used to create modular designs, allowing you to select only the features your application needs, reducing code size, improving performance, and enhancing maintainability. This is a cornerstone of robust, scalable, and reliable embedded system development with Zephyr.



### 2. Theory (2983 words)

#### 2.1 Kconfig Syntax

The Kconfig system relies on a set of keywords and syntax to define configurable options.  Here's a breakdown of the essential elements:

* **`config MY_OPTION`**:  This is the core statement for declaring a configuration option.  `MY_OPTION` is the name of the option, and it’s case-sensitive.
* **`bool`, `string`, `int`, `hex`**:  These keywords specify the *type* of the configuration option.  Choosing the correct type is vital for proper data handling and validation.  `bool` for true/false choices, `string` for textual configuration, `int` for integer values, and `hex` for hexadecimal values.
* **`"Enable feature"`**:  This is the help text associated with the configuration option.  It’s displayed during the configuration process and provides context for the user.
* **`default y`**, **`default "default_value"`**, **`default 100`**, **`default 0x1000`**: These specify the *default* value of the option.  If the user doesn't explicitly select a value during configuration, this default will be used.  The defaults are only applied if the option is enabled.
* **`range 1 1000`**: Defines a range for integer values.
* **`depends on MY_BOOL_OPTION`**: This establishes a *dependency*. The `MY_OPTION` is only enabled if the `MY_BOOL_OPTION` is enabled.
* **`select SOME_DEPENDENCY`**:  Automatically includes the definition of `SOME_DEPENDENCY` if `MY_OPTION` is selected. This is used to ensure the required modules or definitions are brought into the application.
* **`choice`**: Introduces a choice group, presenting multiple options to the user.
* **`prompt "Select communication protocol"`**:  Sets the prompt message displayed to the user when a choice group is encountered.
* **`endchoice`**: Marks the end of a choice group.

**Example:**

```kconfig
config MY_BOOL_OPTION
    bool "Enable feature"
    default y
    help
      Boolean configuration option.
```

In this example, `MY_BOOL_OPTION` is a boolean configuration option with the default value of `y` and the help text "Enable feature."  When the user configures this option, they can select `y` or `n`.

#### 2.2 Kconfig Extension

Beyond the basic syntax, Kconfig offers several extensions to enhance its flexibility:

* **`if MY_OPTION`**: This conditional statement allows you to selectively include or exclude code based on the value of `MY_OPTION`. It’s a fundamental building block for creating modular and adaptable applications.
* **`endif # MY_OPTION`**:  Closes the conditional block.  Crucially, the `endif` statement *must* match the `if` statement.
* **`include "my_kconfig_file.kconfig"`**: Allows you to include other Kconfig files, promoting code reuse and organization. This is vital for larger projects.
* **`export`**:  Used within a module's Kconfig file to expose configuration options defined within that module to the system configuration.  This allows you to create truly reusable modules.

**Example:  Module Integration**

Let's illustrate the use of `if` statements and modules:

**`my_module.kconfig`:**

```kconfig
menuconfig MY_MODULE
    bool "My Custom Module"
    help
      Enable custom module with sub-options.

    if MY_MODULE

        config MY_MODULE_DEBUG
            bool "Enable debug output"
            default n

        config MY_MODULE_BUFFER_COUNT
            int "Number of buffers"
            default 4
            range 1 16

    endif # MY_MODULE
```

In this example,  `MY_MODULE` is a menu entry.  The `if MY_MODULE` statement ensures that the `MY_MODULE_DEBUG` and `MY_MODULE_BUFFER_COUNT` options are only available if `MY_MODULE` is enabled. This demonstrates how modules can encapsulate their own configuration options.

**Example:  Using `export`**

Let's say you have a module called `led_control` that needs to expose configuration options like brightness and color.

**`led_control.kconfig`:**

```kconfig
menuconfig LED_CONTROL_MODULE
    bool "LED Control Module"
    help
      Configure LED control options.

    if LED_CONTROL_MODULE

        export LED_BRIGHTNESS
            int "LED Brightness"
            default 50
            range 0 255

        export LED_COLOR
            string "LED Color"
            default "red"
            choices "red green blue yellow cyan magenta"

    endif # LED_CONTROL_MODULE
```

Now, your main application’s Kconfig file can `include` `led_control.kconfig` and then use `LED_BRIGHTNESS` and `LED_COLOR` if `LED_CONTROL_MODULE` is enabled.



#### 2.3 Dependencies and Visibility

* **`depends on`**:  This creates a direct dependency between configuration options.  If `MY_BOOL_OPTION` is disabled, `MY_OPTION` will also be disabled.
* **`select`**: This allows you to automatically include the definition of a module or configuration option when another option is enabled.  Ensures that the dependent components are always present.

#### 2.4 Invisible Symbols (for Defaults):

These symbols are used to define default values for options, which are only applied when the user doesn’t explicitly set a value. They’re often used for platform-specific configurations.

### 3. Lab Exercise (2246 words)

**Lab Goal:** Create and configure a module that uses custom Kconfig options, showcasing the fundamental concepts of Kconfig symbols.

**Starter Code:**  (Files provided in a git repository – for brevity, the code won’t be shown here, but assumes a typical Zephyr project structure with a `src` directory, a `prj.conf` file, and a CMakeLists.txt file)

**Project Structure:**

* `src/`: Contains source files (e.g., `main.c`, `my_module.c`).
* `prj.conf`: Zephyr configuration file.
* `CMakeLists.txt`:  CMake configuration file.
* `kconfig/`:  Directory to hold Kconfig files.

**Lab Breakdown:**

**Part 1: Basic Concepts and Simple Implementations (750 words)**

1. **Create `my_module.kconfig`:**  Start by creating a new file named `my_module.kconfig` inside the `kconfig/` directory.  This file will define the configuration options for our custom module.  The initial content should resemble the following:

   ```kconfig
   menuconfig MY_MODULE
       bool "My Custom Module"
       help
         Enable custom module with sub-options.
   ```

2. **Create `my_module.c`:**  Create a `my_module.c` file in the `src/` directory. This file will implement the functionality that will be controlled by the Kconfig options.  For now, the simplest implementation is a function that prints a message to the console when the module is enabled.

   ```c
   #include <stdio.h>

   void my_module_function() {
       printf("My Custom Module is enabled!\n");
   }
   ```

3. **Configure the System:** Modify `prj.conf` to include `my_module.kconfig`.  Add the line:

   ```
   KCONFIG_FILES += "kconfig/my_module.kconfig"
   ```

4. **Build and Run:** Use `west build` to build the project. Then, run the resulting application.  You should see the message "My Custom Module is enabled!" printed to the console.

5. **Experiment with Defaults:** Modify the `default` value of `MY_MODULE` to `n`. Rebuild and run. The message should *not* be printed. This demonstrates how the default value is used when no explicit selection is made.

**Part 2: Intermediate Features and System Integration (750 words)**

1. **Add a Debug Option:**  Within `my_module.kconfig`, add a configuration option called `MY_MODULE_DEBUG`,  a boolean option with a default of `n`.  Also, add a `my_module_debug_output` function to `my_module.c` that prints a debug message.

   * `my_module.kconfig`:
     ```kconfig
     menuconfig MY_MODULE
         bool "My Custom Module"
         help
           Enable custom module with sub-options.

         config MY_MODULE_DEBUG
             bool "Enable debug output"
             default n
             help
               Enable debugging output.

         if MY_MODULE

             config MY_MODULE_BUFFER_COUNT
                 int "Number of buffers"
                 default 4
                 range 1 16
             endif # MY_MODULE
         ```

   * `my_module.c`:
     ```c
     #include <stdio.h>

     void my_module_function() {
         printf("My Custom Module is enabled!\n");
     }

     void my_module_debug_output() {
         printf("Debug output from My Custom Module\n");
     }
     ```

2. **Introduce Dependencies:**  Add `depends on` clause to make `MY_MODULE_DEBUG` dependent on `MY_MODULE`. This ensures that the `my_module_debug_output` function is only defined when `MY_MODULE` is enabled.

3. **Add a Range Constraint:**  Introduce a `range` constraint to the `MY_MODULE_BUFFER_COUNT` option, restricting the allowed values to the range of 1 to 16.

4. **Update `prj.conf`:**  Rebuild the project after making these changes. Verify that the `MY_MODULE_BUFFER_COUNT` option can only be set to values between 1 and 16.

**Part 3: Advanced Usage and Real-World Scenarios (750 words)**

1. **Use `if` Statements:** Introduce more complex logic using `if` statements within the `my_module.c` file. For example, you could add code that prints a different message depending on whether `MY_MODULE_DEBUG` is enabled or not.

2. **Introduce an `export`**: Add an `export` to expose the `MY_MODULE_BUFFER_COUNT` option to the system configuration.  This will allow you to configure the number of buffers directly from the main configuration menu.

   * `my_module.kconfig`:
      ```kconfig
      menuconfig MY_MODULE
          bool "My Custom Module"
          help
            Enable custom module with sub-options.

          config MY_MODULE_DEBUG
              bool "Enable debug output"
              default n
              help
                Enable debugging output.

          if MY_MODULE

              config MY_MODULE_BUFFER_COUNT
                  int "Number of buffers"
                  default 4
                  range 1 16
              endif # MY_MODULE

          export MY_MODULE_BUFFER_COUNT
          ```

3. **Create a  `my_module.c` that handles the exported option:**  Modify `my_module.c` to read the value of `MY_MODULE_BUFFER_COUNT` and use it to set the number of buffers.  Use this value in the `my_module_function` to set the appropriate buffer size.

4. **Test and Verify:** Thoroughly test the changes to ensure that the custom module functions correctly and that the exported option can be configured through the system configuration.

**Part 4:  Troubleshooting and Considerations (250 words)**

* **Kconfig Syntax:** Double-check the syntax of your Kconfig files. Errors in syntax can prevent the configuration from being parsed correctly.
* **Rebuild After Changes:**  Always rebuild the project after making any changes to the Kconfig files or source code.
* **System Configuration:**  Understand how the system configuration works and how Kconfig options are processed.
* **Error Messages:** Carefully read any error messages generated by the build system.  These messages can provide valuable clues about the cause of the problem.



**Expected Outcome:**  The lab should result in a fully functional custom module that can be configured through the Zephyr system configuration, showcasing the core concepts of Kconfig symbols and their usage.  The module’s functionality should be controllable and configurable via the `prj.conf` file, demonstrating the flexibility and power of Kconfig.  This lab also highlights the best practices for developing and maintaining Kconfig-based configurations in Zephyr.
***

(Note:  This detailed breakdown provides a significant amount of explanation and guidance.  A real implementation would involve detailed code comments and explanations for each step.)

**Important:**  The provided code for the Kconfig files and `my_module.c` will be provided in a git repository. This is for the purpose of this lab, and you will need to have access to the code to complete the tasks.