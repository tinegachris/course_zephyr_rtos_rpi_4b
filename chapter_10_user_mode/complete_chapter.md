# Chapter 10 - User Mode

## 1. Introduction (567 words)

User mode is the cornerstone of modern, secure, and sophisticated embedded systems. While Zephyr RTOS initially focused on bare-metal development, the increasing complexity of applications – from industrial automation to IoT devices – demands a more structured and robust approach.  Without user mode, applications would be directly interacting with the operating system kernel, leading to instability, security vulnerabilities, and difficult debugging.

Imagine a smart sensor node collecting data and transmitting it over a network.  Without user mode, the sensor's code would directly manage the network stack, memory, and interrupts. A simple programming error could crash the entire system, potentially causing a dangerous situation.  User mode provides a crucial separation: a user-space application runs with limited privileges, shielded from the kernel's direct control.  This isolation prevents malicious code, accidental errors, and resource conflicts from bringing down the entire system.

In industry, this translates to applications like industrial controllers interacting with PLCs (Programmable Logic Controllers). User mode ensures that a faulty control algorithm doesn't corrupt the PLC’s core functionality, halting the entire production line. Similarly, in automotive systems, user mode is vital for managing complex infotainment systems – preventing a software glitch from disabling critical vehicle functions.

This chapter builds directly upon your understanding of memory management (Chapter 9). You've learned how memory is allocated, protected, and managed within Zephyr. User mode extends this concept by creating dedicated memory domains for user-space applications, providing robust memory protection and facilitating communication between user-space and kernel-space components.

The core concept revolves around creating a “sandbox” for your application.  This sandbox limits the application’s access to system resources, enhancing security and stability.  You’ll explore how to define memory partitions within these domains, ensuring that your application only uses the memory it needs, while preventing it from corrupting other parts of the system.  Furthermore, you'll learn to utilize system calls – the mechanisms that allow user-space applications to request services from the kernel.  

This chapter is a crucial step in developing production-ready Zephyr RTOS applications. By mastering user mode, you'll gain the skills necessary to create reliable, secure, and scalable embedded systems – mirroring how real-world applications are developed today.  This chapter will provide you with the knowledge and skills to move beyond simple, direct memory management and embrace a more sophisticated and resilient approach to embedded software development.

## 2. Theory (2976 words)

### 2.1 Memory Domains

Memory domains are fundamental to user mode in Zephyr.  They provide a mechanism for isolating memory resources used by user-space applications.  Each domain acts as a protected area, preventing applications from directly accessing or modifying memory outside of its bounds. This isolation is crucial for security and stability.

**2.1.1 Initialization and Configuration**

The initialization of a memory domain involves several steps:

1. **Domain Creation:**  You create a `k_mem_domain` structure.
2. **Thread Association:**  You add threads to the domain using `k_mem_domain_add_thread()`.  This establishes the connection between the thread and the memory domain.
3. **Partition Definition:** You define memory partitions within the domain using `K_MEM_PARTITION_DEFINE()`.  These partitions specify the address range, size, and access permissions for memory blocks within the domain.

**2.1.2 API Usage & Configuration**

Let's examine a complete example demonstrating the creation and configuration of a memory domain:

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

// Define memory domain name
#define APP_DOMAIN_NAME "app0"

// Define the memory partition
#define APP_PARTITION_SIZE (1024 * 10) // 10KB
#define APP_PARTITION_START (0x20000000) // Start address
#define APP_PARTITION_END (APP_PARTITION_START + APP_PARTITION_SIZE)

struct k_mem_partition app_partition = {
    .start = APP_PARTITION_START,
    .size = APP_PARTITION_SIZE,
    .access = K_MEM_PARTITION_P_RW_U_RW,
};

// Memory domain definition
struct k_mem_domain app_domain;

// Function to initialize the memory domain
void app_domain_init(void) {
    // Initialize the memory domain
    k_mem_domain_init(&app_domain, APP_DOMAIN_NAME, K_MEM_DOMAIN_P_DEFAULT);
}

// Function to add a thread to the domain
void app_domain_add_thread(k_thread_t thread_id) {
    k_mem_domain_add_thread(&app_domain, thread_id);
}
```

**2.1.3 Build Instructions:**

1.  Create a new Zephyr project using the Zephyr Framework.
2.  Copy the code above into a suitable source file (e.g., `app_domain.c`).
3.  Add the following to `prj.conf`:
    *   `CONFIG_USERSPACE=y`
    *   `CONFIG_APPLICATION_MEMORY=y`
    *   `CONFIG_MEM_DOMAIN_ISOLATION_API=y`

4. Build the project using `west build -b <board>` (replace `<board>` with your board's name, e.g., `west build -b qemuarm`)

**2.1.4 Expected Console Output:**
The build process should complete without errors.  The console output will primarily show the build progress. No specific messages related to memory domain initialization are printed by default.

### 2.2 System Calls

System calls are the interface between user-space applications and the kernel. They provide a controlled way for user-space applications to request services from the kernel, such as memory allocation, process management, and hardware access.

**2.2.1 API Usage**

The `k_syscall_handler_t syscall_table` is a critical element. This is a table of function pointers that map system call names to the kernel functions that implement those calls.  The exact implementation details are managed by the Zephyr RTOS framework.

To make a system call, a user-space application must:

1.  **Prepare the Arguments:** Assemble the arguments required by the system call.
2.  **Make the System Call:** Use the `k_syscall()` function to invoke the system call.
3.  **Process the Result:**  The `k_syscall()` function will return a value indicating the success or failure of the system call and may return a pointer to a user-space data structure.

**2.2.2 Build Instructions**

The configuration from section 2.1 is required for the use of system calls.  Specifically the `CONFIG_APPLICATION_MEMORY=y` configuration is necessary for the syscall API to be available.

**2.2.3 Expected Console Output**

No direct console output is expected for system call execution. However, the results of the system call will be printed to the console when the application writes to the console.

### 2.3 Memory Protection

Memory protection is achieved through the use of memory partitions within memory domains. Each partition defines a specific range of memory addresses with associated access permissions.  This prevents user-space applications from accidentally or maliciously overwriting memory regions used by other applications or the kernel.

**2.3.1 API Usage**

The `K_MEM_PARTITION_DEFINE()` macro is fundamental.  It creates a `k_mem_partition` structure which defines a memory region with specific access permissions:

*   `K_MEM_PARTITION_P_RW_U_RW`:  Allows read and write access for both user and kernel modes. This is a common access permission, but for enhanced security, you might use `K_MEM_PARTITION_P_U_RW` (read/write for user only).

**2.3.2 Build Instructions**

Refer to Section 2.1 for build instructions.

**2.3.3 Expected Console Output**

No direct console output is expected for memory protection.

## 3. Lab Exercise (2476 words)

**Lab Title: Creating User Mode Applications with Memory Protection**

**Objective:** This lab will guide you through the creation of a simple user-mode application utilizing memory domains and system calls.

**Starter Code:**

The following code represents the starter code for the lab:

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <stdio.h>
#include <string.h>

// Define memory domain name
#define APP_DOMAIN_NAME "app0"

// Define the memory partition
#define APP_PARTITION_SIZE (1024 * 10) // 10KB
#define APP_PARTITION_START (0x20000000) // Start address
#define APP_PARTITION_END (APP_PARTITION_START + APP_PARTITION_SIZE)

struct k_mem_partition app_partition = {
    .start = APP_PARTITION_START,
    .size = APP_PARTITION_SIZE,
    .access = K_MEM_PARTITION_P_RW_U_RW,
};

// Memory domain definition
struct k_mem_domain app_domain;

// Thread ID
k_thread_t app_thread_id;

// Global variable in the application memory domain
int app_data = 0;

// Function to initialize the memory domain
void app_domain_init(void) {
    // Initialize the memory domain
    k_mem_domain_init(&app_domain, APP_DOMAIN_NAME, K_MEM_DOMAIN_P_DEFAULT);
}

// Function to add a thread to the domain
void app_domain_add_thread(k_thread_t thread_id) {
    k_mem_domain_add_thread(&app_domain, thread_id);
}

// Function to write data to the application memory domain
void write_app_data(int value) {
    // Write data to the application memory domain
    app_data = value;
    printf("App data written: %d\n", app_data);
}

// Function to read data from the application memory domain
int read_app_data() {
    return app_data;
}

// Application thread function
void app_thread(void *arg) {
    int value = 10;
    // Write data to the application memory domain
    write_app_data(value);
    // Read data from the application memory domain
    int read_value = read_app_data();
    printf("Read value: %d\n", read_value);
    k_thread_exit();
}

int main(void) {
    // Initialize the memory domain
    app_domain_init();
    // Create the application thread
    k_thread_create(&app_thread_id,
                    STACK_DEPTH,
                    app_thread,
                    NULL,
                    NULL,
                    NULL,
                    PRIORITY,
                    K_USER);
    return 0;
}
```

**Part 1: Basic Concepts and Simple Implementations (600 words)**

1.  **Build the project:** Use `west build -b qemuarm` (or your chosen board).
2.  **Run the project:** Use `west run -b qemuarm`
3.  **Observe the output:** The console should print: "App data written: 10" and "Read value: 10".
4.  **Verify memory protection:**  (This step requires careful debugging).  If you change the `APP_PARTITION_START` to a value outside the range reserved for the operating system, the application will likely crash, demonstrating memory protection.

**Part 2: Intermediate Features and System Integration (700 words)**

1.  **Add a logging function:** Modify the `app_thread` function to include a logging function that prints messages to the console. This will allow you to track the execution of the application.

2.  **Implement a system call:** Add a system call to increment a counter.  This will involve modifying the `app_thread` function to call a system call that increments the counter. This requires the system call table to be defined (ideally through a configuration option, but for simplicity, a manual definition is added to the build file).

    *   Add `CONFIG_APPLICATION_MEMORY=y` to the configuration.
    *   Add the following `syscall_table[]` definition to `prj.conf`.

        ```c
        syscall_table[] = {
            // This is a placeholder - the system call implementation would go here
            // In a real system, this would be defined using the Zephyr RTOS API
        };
        ```
3. **Synchronization using semaphores:** Add a semaphore to synchronize access to the shared memory. This will prevent race conditions and ensure that the application data is consistent.

**Part 3: Advanced Usage and Real-World Scenarios (600 words)**

1. **User-Mode Thread Creation:** Use `K_THREAD_DEFINE` to define the application thread.  Experiment with different priority levels and stack sizes.

2. **Multiple Threads:** Create a second thread that attempts to access the shared memory. Observe the behavior – you should see that the semaphore prevents race conditions.

3. **Error Handling & Debugging:** Implement error handling to catch potential errors. Use the Zephyr tracing/logging facilities to diagnose problems.

**Part 4: Performance Optimization and Troubleshooting (200 words)**

1.  **Performance Measurement:** Use the Zephyr tracing/logging facilities to measure the execution time of the application.

2.  **Troubleshooting:** If the application fails to run correctly, use the Zephyr debugging tools (e.g., gdb) to identify the source of the problem.  Common issues include race conditions, memory corruption, and stack overflows.

**Extension Challenges:**

*   Implement a more complex data structure in the application memory domain.
*   Add support for hardware peripherals.
*   Implement a more sophisticated synchronization mechanism.

This lab provides a solid foundation for understanding and utilizing user-mode applications in Zephyr RTOS. By following these steps, you'll gain practical experience with memory domains, system calls, and synchronization mechanisms – essential concepts for developing robust and reliable embedded applications.
