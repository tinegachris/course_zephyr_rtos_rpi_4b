# Chapter 13 - Interrupt Management Theory

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

## 13.1 Interrupts in Zephyr

Zephyr's interrupt handling is built upon a modular and flexible architecture. It allows you to connect interrupts to specific handler functions that are executed when the interrupt signal occurs.

### Core Concepts

* **Interrupt Lines:** Physical hardware lines that signal an interrupt event.
* **Interrupt Controller:** The hardware component that manages interrupt requests.
* **Interrupt Handler:** A function that's executed when an interrupt signal is raised.
* **IRQ_CONNECT():** A Zephyr API used to register an interrupt handler with the system.

### Basic Interrupt Setup

```c
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

void my_isr(const void *arg)
{
    /* ISR implementation - keep minimal */
    printk("Interrupt triggered!\n");
    k_sem_give(&data_ready_sem); // Signal the main thread
}

IRQ_CONNECT(MY_IRQ, MY_IRQ_PRIORITY, my_isr, NULL, 0);
irq_enable(MY_IRQ);
```

### IRQ_CONNECT Parameters

1. **`MY_IRQ`**: A symbolic name representing the interrupt line. This should correspond to the physical line connected to the interrupt controller.
2. **`MY_IRQ_PRIORITY`**: A symbolic name representing the priority of the interrupt. Lower values indicate higher priority.
3. **`my_isr`**: The name of the interrupt handler function.
4. **`NULL`**: Pointer to an argument that will be passed to the interrupt handler.
5. **`0`**: The interrupt flag. Typically set to `0` for basic interrupt handling.

### Interrupt-Safe APIs

It is **absolutely critical** to use only the interrupt-safe APIs within your interrupt handlers. Failure to do so can lead to unpredictable behavior and system crashes.

**Safe in ISR:**
- `k_sem_give()`: Signals a semaphore.
- `k_msgq_put(..., K_NO_WAIT)`: Puts a message onto a message queue.
- `k_work_submit()`: Submits a work item to a workqueue.
- `k_poll_signal_raise()`: Raises a signal for polling.

**NOT Safe in ISR:**
- `k_sem_take()` with timeout
- `k_mutex_lock()`
- `k_sleep()` (Any blocking operation)

## 13.2 Handler Threads

Handler threads are specialized threads designed to receive and process data from interrupt handlers. They provide a safe and controlled environment for handling interrupt-driven tasks.

### Why Use Handler Threads?

- **Avoid Priority Inversion**: Without a handler thread, a high-priority interrupt handler could block a lower-priority thread.
- **Safe Context Switching**: Ensures that context switching is performed safely within the thread's context.
- **Resource Protection**: The handler thread can protect shared resources, preventing race conditions.

### Example Implementation

```c
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

// Define handler thread
K_THREAD_DEFINE(handler_thread, 1024, handler_thread_func, NULL, NULL, NULL,
                K_PRIO_COOP(7), 0, 0);

void handler_thread_func(void *arg1, void *arg2, void *arg3)
{
    while (1) {
        // Wait for interrupt signal
        k_sem_take(&data_ready_sem, K_FOREVER);
        
        // Process data received from the interrupt
        printk("Handler thread processing interrupt data\n");
        
        // Perform complex processing that shouldn't be done in ISR
        process_interrupt_data();
    }
}
```

## 13.3 Workqueues

Work queues are a flexible and efficient mechanism for deferred processing of tasks. They allow tasks to signal the need for processing, and a separate thread handles the actual processing.

### Key Components

- **`k_work_queue_init()`**: Initializes a workqueue.
- **`k_work_queue_start()`**: Starts the workqueue, creating the thread that handles work items.
- **`k_work_submit()`**: Submits a work item to the workqueue.
- **`k_work_submit_to_queue()`**: Submits a work item to a specific queue.

### Basic Workqueue Example

```c
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

// Define work item
K_WORK_DEFINE(my_work, work_handler);

// Work handler function
void work_handler(struct k_work *work)
{
    printk("Processing work item\n");
    // Perform the actual processing
    handle_interrupt_processing();
}

// In the ISR, submit work
void my_isr(const void *arg)
{
    // Minimal ISR - just submit work
    k_work_submit(&my_work);
}
```

### Custom Workqueue

```c
#define WORKQUEUE_STACK_SIZE 1024
#define WORKQUEUE_PRIORITY 5

K_THREAD_STACK_DEFINE(workqueue_stack, WORKQUEUE_STACK_SIZE);

static struct k_work_q custom_work_q;

void init_custom_workqueue(void)
{
    k_work_queue_init(&custom_work_q);
    k_work_queue_start(&custom_work_q, workqueue_stack,
                       K_THREAD_STACK_SIZEOF(workqueue_stack),
                       WORKQUEUE_PRIORITY, NULL);
}

// Submit to custom workqueue
void submit_to_custom_queue(void)
{
    k_work_submit_to_queue(&custom_work_q, &my_work);
}
```

## 13.4 Advanced Interrupt Concepts

### Dynamic Interrupt Connection

```c
void setup_dynamic_interrupt(void)
{
    int ret = irq_connect_dynamic(MY_IRQ, MY_IRQ_PRIORITY, 
                                  my_dynamic_isr, NULL, 0);
    if (ret < 0) {
        printk("Failed to connect dynamic interrupt\n");
        return;
    }
    
    irq_enable(MY_IRQ);
}
```

### Interrupt Priorities

Zephyr supports multiple interrupt priority levels. Lower numerical values indicate higher priority:

```c
#define HIGH_PRIORITY_IRQ    0
#define MEDIUM_PRIORITY_IRQ  5
#define LOW_PRIORITY_IRQ     10

IRQ_CONNECT(TIMER_IRQ, HIGH_PRIORITY_IRQ, timer_isr, NULL, 0);
IRQ_CONNECT(UART_IRQ, MEDIUM_PRIORITY_IRQ, uart_isr, NULL, 0);
IRQ_CONNECT(GPIO_IRQ, LOW_PRIORITY_IRQ, gpio_isr, NULL, 0);
```

### Interrupt Nesting

Zephyr supports interrupt nesting, where higher priority interrupts can preempt lower priority ones:

```c
void high_priority_isr(const void *arg)
{
    // This can interrupt lower priority ISRs
    handle_critical_event();
}

void low_priority_isr(const void *arg)
{
    // Can be interrupted by higher priority ISRs
    handle_normal_event();
}
```

## 13.5 Best Practices

### ISR Design Guidelines

1. **Keep ISRs minimal**: Only do what's absolutely necessary in the ISR
2. **Use deferred processing**: Use workqueues or handler threads for complex processing
3. **Avoid blocking operations**: Never call blocking APIs in ISRs
4. **Minimize stack usage**: ISRs have limited stack space
5. **Use interrupt-safe APIs only**: Stick to the approved API list

### Error Handling in Interrupts

```c
void robust_isr(const void *arg)
{
    int ret;
    
    // Check hardware status
    if (!is_interrupt_valid()) {
        return; // Spurious interrupt
    }
    
    // Use interrupt-safe API with error checking
    ret = k_msgq_put(&my_msgq, &data, K_NO_WAIT);
    if (ret != 0) {
        // Handle queue full condition
        increment_overflow_counter();
    }
    
    // Clear interrupt source
    clear_interrupt_flag();
}
```

### Performance Considerations

```c
// Efficient ISR with minimal processing
void efficient_isr(const void *arg)
{
    // Read hardware register once
    uint32_t status = read_status_register();
    
    // Quick decision making
    if (status & CRITICAL_FLAG) {
        k_work_submit(&critical_work);
    } else {
        k_work_submit(&normal_work);
    }
    
    // Clear interrupt quickly
    write_status_register(status);
}
```

This theory foundation provides the essential knowledge needed to implement robust interrupt handling in Zephyr applications.

[Next: Interrupt Management Lab](./lab.md)
