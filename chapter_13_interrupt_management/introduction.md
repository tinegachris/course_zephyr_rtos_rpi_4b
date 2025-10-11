# Chapter 13 - Interrupt Management

## Building on Communication and Threading Foundations

Having mastered inter-thread communication in Chapter 12—message queues, mailboxes, and Zbus publish-subscribe patterns—you now understand how threads exchange data and coordinate activities within your embedded applications. However, these communication mechanisms operate entirely within the software domain, managing interactions between threads that you explicitly create and control.

Real-world embedded systems must also respond to **external events** from the physical world—sensor readings, button presses, network packets, hardware triggers—events that occur independently of your thread scheduling and communication patterns. Interrupt management provides the critical bridge between the external hardware world and your carefully orchestrated software architecture.

## Introduction

Interrupt management represents the cornerstone of real-time embedded systems, extending your thread communication skills into hardware event handling. Building directly upon the communication patterns you've mastered, interrupts provide the mechanism by which external hardware events integrate seamlessly with your message queues, mailboxes, and publish-subscribe architectures.

**Why is it Crucial?**

Consider a scenario: a sensor continuously monitors temperature. Without an interrupt handler, the main thread would have to repeatedly poll the sensor for updates. This is inefficient, consumes CPU resources, and introduces latency. An interrupt handler, triggered by the sensor's data ready signal, allows the system to react *immediately* to new temperature readings, enabling faster and more responsive control logic.

**Real-World Applications:**

* **Industrial Automation:**  Sensors monitoring machine status, actuators responding to urgent alerts, and data logging - all heavily reliant on interrupt-driven systems.
* **Robotics:**  Vision sensors providing immediate feedback, motor control systems reacting to positional changes, and safety systems triggering emergency stops.
* **Automotive Systems:**  CAN bus data, airbag deployment sensors, and engine control units (ECUs) all utilize interrupt-driven architectures for real-time control and safety.
* **IoT Devices:** Environmental sensors, wearable devices, and smart home devices constantly collect data and respond to user interactions, again, frequently leveraging interrupts.

**Connecting Hardware Events to Communication Patterns:**

Interrupts work seamlessly with the communication mechanisms you've mastered in Chapter 12. Hardware events trigger interrupt handlers that can immediately signal threads through semaphores, post messages to queues, or publish events via Zbus—integrating external events directly into your structured communication architecture. This creates responsive systems where hardware events flow naturally through your message-passing designs.

**Motivation & Practicality:**

This chapter will provide you with practical techniques for leveraging these powerful concepts. We'll explore how to set up interrupt handlers, use the workqueue system for delayed processing, and – critically – use the *interrupt-safe APIs* that Zephyr provides.  Understanding these nuances is paramount for building reliable, responsive, and efficient embedded systems.  Incorrectly using non-interrupt-safe APIs within an interrupt handler can lead to unpredictable behavior, system crashes, and security vulnerabilities.

## 2. Theory (2845 words)

### 2.1 Interrupts in Zephyr

Zephyr's interrupt handling is built upon a modular and flexible architecture. It allows you to connect interrupts to specific handler functions that are executed when the interrupt signal occurs.

**Core Concepts:**

* **Interrupt Lines:** Physical hardware lines that signal an interrupt event.
* **Interrupt Controller:**  The hardware component that manages interrupt requests.
* **Interrupt Handler:**  A function that's executed when an interrupt signal is raised.
* **IRQ_CONNECT():**  A Zephyr API used to register an interrupt handler with the system.

**Example:**

```c
#include <zephyr/sys/irq.h>
#include <zephyr/kernel.h>

void my_isr(const void *arg)
{
    /* ISR implementation - keep minimal */
    printk("Interrupt triggered!\n");
    k_sem_give(&data_ready_sem); //Signal the main thread
}

IRQ_CONNECT(MY_IRQ, MY_IRQ_PRIORITY, my_isr, NULL, 0);
irq_enable(MY_IRQ);
```

**Explanation:**

1.  **`#include <zephyr/sys/irq.h>`:** Includes the necessary headers for interrupt management.
2.  **`IRQ_CONNECT(MY_IRQ, MY_IRQ_PRIORITY, my_isr, NULL, 0);`:**  This is the crucial line.
    *   `MY_IRQ`:  A symbolic name representing the interrupt line.  This should correspond to the physical line connected to the interrupt controller.
    *   `MY_IRQ_PRIORITY`: A symbolic name representing the priority of the interrupt.  Lower values indicate higher priority.
    *   `my_isr`:  The name of the interrupt handler function.
    *   `NULL`:  Pointer to an argument that will be passed to the interrupt handler.
    *   `0`:  The interrupt flag.  Typically set to `0` for basic interrupt handling.
3.  **`irq_enable(MY_IRQ);`**:  Enables the interrupt line, allowing the system to respond to interrupts.  It's essential to enable interrupts before using them.

**Interrupt-Safe APIs:**

It is *absolutely critical* to use only the interrupt-safe APIs within your interrupt handlers. Failure to do so can lead to unpredictable behavior and system crashes.  The Zephyr documentation explicitly lists these:

*   `k_sem_give()`:  Signals a semaphore.
*   `k_msgq_put(..., K_NO_WAIT)`:  Puts a message onto a message queue.
*   `k_work_submit()`:  Submits a work item to a workqueue.
*   `k_poll_signal_raise()`:  Raises a signal for polling.

**NOT Safe in ISR:**

*   `k_sem_take()` with timeout
*   `k_mutex_lock()`
*   `k_sleep()`  (Any blocking operation)

## 2.2 Handler Thread

Handler threads are specialized threads designed to receive and process data from interrupt handlers. They provide a safe and controlled environment for handling interrupt-driven tasks.

**Why use a Handler Thread?**

*   **Avoid Priority Inversion:** Without a handler thread, a high-priority interrupt handler could block a lower-priority thread, leading to priority inversion.
*   **Safe Context Switching:** Ensures that context switching is performed safely within the thread's context.
*   **Resource Protection:** The handler thread can protect shared resources, preventing race conditions.

**Example (Simplified):**

(The example uses the same ISR as above, but this section focuses on the setup and interaction with the handler thread).

```c
#include <zephyr/sys/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>  //For k_work_submit_to_queue

//... (my_isr function as before) ...

K_THREAD_DEFINE(handler_thread, 1024, handler_thread_func, NULL, NULL, NULL); // 1024 Bytes
```

**`handler_thread_func()`:**

```c
void handler_thread_func(void *arg)
{
    K_PROLONG_SLEEP(1000 * K_MSEC); //Keep the thread alive
    printk("Handler thread running\n");
    // Process data received from the interrupt
}
```

**Explanation:**

1.  **`K_THREAD_DEFINE(...)`:** Defines a new thread named `handler_thread`.
    *   `handler_thread`: The name of the thread.
    *   `1024`: The stack size for the thread (1024 bytes).
    *   `handler_thread_func`: The function that will be executed by the thread.
    *   `NULL`:  No arguments passed to the thread.

**Interaction:** The `my_isr` function signals the completion of a task by giving a semaphore.  The handler thread waits for this signal, ensuring that the data is processed correctly.

## 2.3 Workqueue

Work queues are a flexible and efficient mechanism for deferred processing of tasks. They allow tasks to signal the need for processing, and a separate thread handles the actual processing.

**Key Components:**

*   **`k_work_queue_init()`:**  Initializes a workqueue.
*   **`k_work_queue_start()`:** Starts the workqueue, creating the thread that handles work items.
*   **`k_work_submit()`:**  Submits a work item to the workqueue.
*   **`k_work_submit_to_queue()`:** Submits a work item to a specific queue.

**Example:**

```c
#include <zephyr/sys/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

//... (my_isr function as before) ...
K_THREAD_DEFINE(workqueue_thread, 1024, workqueue_thread_func, NULL, NULL, NULL);

//Work item definition
K_WORK_DEFINE(my_work, work_handler);
```

**`work_handler()`:**

```c
void work_handler(struct k_work *work)
{
    printk("Processing work item\n");
    // Perform the actual processing
}
```

**Explanation:**

1.  **`K_WORK_DEFINE(...)`:** Defines a work item named `my_work`.

2.  **`k_work_submit()`:** Submits the `my_work` to the workqueue.

**Workflow:**

1.  The interrupt handler calls `k_work_submit()`.
2.  The work item is added to the workqueue.
3.  The workqueue thread processes the work item, executing the `work_handler` function.

## 3. Lab Exercise (2300 words)

**Lab Goal:** To understand and implement interrupt handling, workqueues, and their interaction in Zephyr.

**Starter Code:**  (Provided as a separate file – `interrupt_lab.tar.gz`). This includes a basic Zephyr project structure with the necessary headers, configurations, and a minimal framework.

**Step-by-Step Instructions (with Verification Procedures):**

**Part 1: Basic Concepts & Simple Implementation (Approx. 600 words)**

1.  **Build the Project:**
    *   Use West to build the project: `west build -b *`
    *   Observe the build output for errors.
2.  **Configure the Interrupt:**
    *   Modify `prj.conf` to define `MY_IRQ` (e.g., `MY_IRQ = GPIO0` – you'll need a suitable GPIO pin).  Verify the IRQ priority.
3.  **Implement the Interrupt Handler:**
    *   Replace the existing `my_isr` function with your own, printing a message to the console.
4.  **Create a Handler Thread:**  The starter code includes this.
5.  **Test the Interrupt:**  Upload the code to your target device and verify that the message is printed when the interrupt is triggered (e.g., by pressing a button connected to the GPIO pin).
6.  **Verification:**  Use West’s `west test` command (if tests are provided) or manually trigger the interrupt.

**Part 2: Intermediate Features & System Integration (Approx. 700 words)**

1.  **Modify the Handler Thread:**  Change the `handler_thread_func()` to wait on the semaphore given by the ISR.
2.  **Implement a Simple Workqueue:** Create a work item (`my_work`) and submit it to the workqueue from the interrupt handler.
3.  **Implement the Workhandler:** The workhandler should simply print a message to the console.
4.  **Verification:**  Ensure that the message is printed after the interrupt triggers the workqueue submission.
5.  **Advanced:** Use West’s tracing and logging tools to observe the flow of execution between the interrupt, the workqueue, and the handler thread.

**Part 3: Advanced Usage & Real-World Scenarios (Approx. 600 words)**

1.  **Data Passing:** Modify the code to pass data from the interrupt handler to the workqueue. This can be done by using a `struct` to encapsulate the data.
2.  **Error Handling:** Add error handling to the interrupt handler and the workqueue thread.
3.  **Testing:** Create a scenario where the interrupt handler needs to process data and then signal the workqueue to perform a more complex operation.
4.  **Explore:** Experiment with different priority levels for the interrupt and the workqueue thread to observe the impact on performance and responsiveness.

**Part 4: Performance Optimization & Troubleshooting (Approx. 200 words)**

1.  **Profiling:** Use West’s profiling tools to identify performance bottlenecks.
2.  **Optimization:** Optimize the interrupt handler and the workqueue thread for maximum performance.
3.  **Troubleshooting:**  Use West’s debugging tools to identify and resolve any issues.  Common issues include priority inversion, race conditions, and deadlocks.

**Extension Challenges (Advanced Students):**

*   Implement a more complex processing logic in the workqueue thread.
*   Add support for multiple interrupts.
*   Implement a more sophisticated error handling mechanism.
*   Add a mechanism to synchronize the interrupt and the workqueue thread.

## 4. Troubleshooting Guide

| Issue                     | Possible Cause                               | Solution                                   |
| ------------------------- | -------------------------------------------- | ------------------------------------------- |
| No output to console       | Interrupt not enabled, handler thread not running, workqueue not started | Ensure IRQ is enabled, verify handler thread is running, start the workqueue |
| Priority inversion          | Interrupt has higher priority than handler thread| Use a priority-based locking mechanism. |
| Race conditions             | Multiple threads accessing shared resources   | Use mutexes or semaphores to protect shared resources. |
| Deadlock                   | Two or more threads are waiting for each other | Carefully design the synchronization mechanisms. |
| West Build Errors        | Incorrect configuration, missing dependencies    | Review `prj.conf` and check the build logs. |

This comprehensive lab exercise, combined with the detailed troubleshooting guide, will provide you with a solid understanding of interrupt handling and workqueues in Zephyr. Remember to experiment, explore, and learn from your mistakes. Good luck!

**Note:** The `interrupt_lab.tar.gz` file contains the basic project structure and starter code.  It's designed to be a starting point for your exploration.  You will need to modify and extend this code as you work through the lab exercises.

(Please note that due to the limitations of text-based communication, providing the actual starter code directly is not possible. However, this detailed outline and instructions will enable you to create the complete project.)