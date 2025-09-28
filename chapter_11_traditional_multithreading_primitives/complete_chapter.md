# Chapter 11 - Traditional Multithreading Primitives

## 1. Introduction (500-600 words)

In the previous chapters, we’ve laid a solid foundation for developing robust embedded systems using Zephyr RTOS. We've explored thread management, tracing/logging, memory management, and hardware interaction. However, many real-world embedded applications demand more sophisticated concurrency control mechanisms – the ability to manage multiple threads executing concurrently, safely and efficiently.  Traditional multithreading primitives, despite being conceptually older, remain *crucial* for advanced systems where responsiveness, real-time performance, and handling complex, intertwined tasks are paramount.

Why are these primitives still important? Consider scenarios like:

* **Industrial Automation:** Controlling robotic arms, managing sensor networks, and coordinating multiple actuators require precise timing and synchronization between threads.
* **Automotive Systems:**  Advanced driver-assistance systems (ADAS) rely on concurrent execution of perception, planning, and control threads.
* **Medical Devices:**  Monitoring patient data, controlling medical equipment, and managing alerts necessitate accurate synchronization.
* **IoT Devices:**  Managing numerous connected devices, handling data streams, and responding to user input concurrently are common requirements.

Without proper synchronization, you’ll quickly encounter race conditions – unpredictable behavior caused by multiple threads accessing and modifying shared resources simultaneously. This can lead to corrupted data, system instability, and, in the worst cases, complete system failure. 

Zephyr’s traditional multithreading primitives – mutexes, semaphores, spinlocks, and polling – provide the tools to prevent these issues. These primitives don’t attempt to provide full-featured multitasking; instead, they focus on *controlled access* to shared resources.  The Zephyr approach aligns well with the resource-constrained nature of many embedded systems, offering a direct and efficient way to manage concurrency. 

This chapter will deepen your understanding of these primitives, equipping you with the skills to design and implement robust, concurrent applications in Zephyr.  We'll move beyond simple thread management and delve into the techniques necessary to build truly reliable and responsive embedded systems.  Furthermore, this chapter integrates concepts learned throughout previous chapters, reinforcing your understanding of the Zephyr ecosystem.



## 2. Theory (2500-3000 words)

### 2.1. Mutual Exclusion, Critical Sections, and Spinlocks

**Concept:**  Mutual exclusion is the fundamental principle of ensuring that only one thread can access a shared resource (e.g., a hardware peripheral, a data structure) at any given time. This is achieved by a *critical section* – a code segment that requires exclusive access.

**Implementation:** Zephyr employs spinlocks to enforce mutual exclusion. A spinlock is a type of lock that a thread repeatedly checks (spins) until the lock is available.

**Code Example:**

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

K_MUTEX_DEFINE(my_mutex);

void critical_section_function(void) {
  K_MUTEX_DEFINE(my_mutex);
  // Critical section - only one thread can execute this code at a time
  printk("Thread %d entering critical section\n", k_self());
  k_sleep_ms(100); // Simulate some work in the critical section
  printk("Thread %d exiting critical section\n", k_self());
}

int main(void) {
  // Example of using the mutex
  k_thread_create(&my_thread, &critical_section_function, NULL);
  k_sleep_ms(500); // Let the thread run for a while
  return 0;
}
```

**Build Commands:**

```bash
west build -b <board_name>
```

**Configuration Files:**
*   `prj.conf`:  (Example - needs adaptation to your board)
    ```kconfig
    menuconfig
        ...
        Networking config ...
        ...
        Threading support
           [*] Enable thread management
           [*] Enable mutex support
    ```

**Step-by-Step Analysis:**

1.  `#include <zephyr/kernel.h>`:  Includes the necessary kernel headers.
2.  `K_MUTEX_DEFINE(my_mutex);`: Defines a mutex named `my_mutex`.
3.  `void critical_section_function(void)`:  This function represents the critical section.
4.  `k_mutex_lock(&my_mutex);`: Attempts to acquire the lock. If the lock is already held by another thread, the thread blocks (spins) until the lock becomes available.
5.  `k_mutex_unlock(&my_mutex);`: Releases the lock, allowing another thread to acquire it.

**Error Handling & Debugging:**  In a production environment, you’d include robust error handling – checking return values of `k_mutex_lock` and `k_mutex_unlock` and potentially using a debugging system (like Zephyr’s tracing/logging) to track lock contention.  

**Performance Implications:** Spinlocks are very efficient in low-contention scenarios but can waste CPU cycles if contention is high.  Consider using priority inheritance or other advanced locking mechanisms in such cases.


### 2.2. Semaphores

**Concept:** Semaphores are a signaling mechanism that can be used to control access to a limited number of resources.  They maintain a counter representing the availability of resources.  A thread can increment the counter (signal) or decrement it (wait).

**Code Example:**

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

K_SEM_DEFINE(my_sem, 0, 1); // Initialize to 1 (one resource available)

void producer(void) {
    k_sem_give(&my_sem); // Signal that a resource is available
    printk("Producer: Resource available!\n");
    k_sleep_ms(500);
}

void consumer(void) {
    k_sem_take(&my_sem, K_MSEC(50)); // Wait until a resource is available
    printk("Consumer: Resource acquired!\n");
    k_sleep_ms(500);
}

int main(void) {
    k_thread_create(&producer_thread, &producer, NULL);
    k_thread_create(&consumer_thread, &consumer, NULL);
    return 0;
}
```

**Build Commands:** (Same as mutex example)

**Configuration Files:** (Same as mutex example)

**Step-by-Step Analysis:**

1.  `K_SEM_DEFINE(my_sem, 0, 1);`: Defines a semaphore named `my_sem` with an initial value of 1 (one resource available).
2.  `k_sem_give(&my_sem);`: Increments the semaphore counter, signaling that a resource is available.
3.  `k_sem_take(&my_sem, K_MSEC(50));`: Decrements the semaphore counter. If the counter is 0, the thread blocks (waits) until another thread calls `k_sem_give`.  The timeout prevents indefinite blocking.



### 2.3. Polling

**Concept:** Polling involves repeatedly checking a shared resource’s status until it becomes available. This is typically used when semaphores or mutexes are not suitable due to latency requirements or limited synchronization needs.  Zephyr's `k_poll` system provides an efficient way to poll multiple events.

**Code Example:**

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

K_SEM_DEFINE(my_event, 0, 1);

void post_event(void) {
    k_event_post(&my_event, 0x001);
}

void wait_for_event(void) {
    uint32_t events = k_event_wait(&my_event, 0xFFF, false, K_MSEC(50));
    if (events == 0) {
        printk("Timeout waiting for event!\n");
    } else {
        printk("Event received!\n");
    }
}

int main(void) {
    k_thread_create(&post_thread, &post_event, NULL);
    k_thread_create(&wait_thread, &wait_for_event, NULL);
    return 0;
}
```

**Build Commands:** (Same as mutex example)

**Configuration Files:** (Same as mutex example)

**Step-by-Step Analysis:**
1. `K_POLL_EVENT_STATIC_INITIALIZER`: creates a poll event initialization for polling a semaphore.
2. The `k_poll` function continuously monitors the specified events.
3.  `k_event_post(&my_event, 0x001);`: Posts the event, waking up waiting threads.
4.  `k_event_wait(&my_event, 0xFFF, false, K_MSEC(50));`: Waits for the event to occur.



## 3. Lab Exercise (2000-2500 words)

**Lab Structure:**

*   **Part 1**: Basic Concepts & Simple Implementations
*   **Part 2**: Intermediate Features & System Integration
*   **Part 3**: Advanced Usage & Real-World Scenarios
*   **Part 4**: Performance Optimization & Troubleshooting

### Part 1: Basic Concepts & Simple Implementations (300 words)

**Objective:** Understand the basic usage of mutexes and semaphores.

**Task:** Create a simple program that uses a mutex to protect access to a shared counter.  Another thread increments the counter.  The program should print the counter value after each increment.

**Starter Code:** (Provided - includes a basic Zephyr project setup)

**Steps:**
1.  Build the project.
2.  Modify the `critical_section_function` to increment a global counter variable.
3.  Add a `k_sleep_ms(100);` call to the `critical_section_function` to simulate work.
4.  Compile and run the program.

**Expected Output:**  You should see the counter value increasing with each increment, and no data corruption.

### Part 2: Intermediate Features & System Integration (600 words)

**Objective:** Implement a producer-consumer pattern using semaphores.

**Task:** Create a producer-consumer system. One thread (the producer) generates data, and another thread (the consumer) consumes it.  Use a semaphore to synchronize the two threads.  The producer adds numbers to a buffer, and the consumer retrieves and prints them.

**Starter Code:**  (Includes a project with a buffer structure and basic thread management)

**Steps:**
1.  Implement the producer thread:  Generate numbers from 0 to 99 and add them to the buffer.
2.  Implement the consumer thread: Retrieve numbers from the buffer and print them.
3.  Use a semaphore to ensure that the producer and consumer do not access the buffer simultaneously.
4.  Add a `k_sleep_ms(500);` call in each thread to simulate work and allow the system to respond to events.

**Expected Output:** The numbers from 0 to 99 should be printed in the same order by the consumer.

### Part 3: Advanced Usage & Real-World Scenarios (800 words)

**Objective:**  Integrate polling with semaphores for more complex synchronization.

**Task:**  Implement the same producer-consumer pattern, but this time use polling to manage the shared buffer. The buffer needs to be protected by semaphores and managed by polling. 

**Starter Code:** (Includes a project with a buffer structure and basic thread management)

**Steps:**
1.  Modify the `k_event_wait()` function in the consumer thread to monitor for the producer to post events to the buffer.
2.  Implement the producer thread to post events to the buffer.
3.  Use a semaphore to synchronize the two threads. 

**Expected Output:** The numbers from 0 to 99 should be printed in the same order by the consumer.

### Part 4: Performance Optimization & Troubleshooting (500 words)

**Objective:** Troubleshoot common issues and explore performance optimization strategies.

**Task:** Identify and resolve common synchronization issues, and implement a simple benchmarking test. 

**Steps:**
1.  **Race Condition Simulation:** Introduce a deliberate race condition (e.g., a double increment) to demonstrate the need for mutual exclusion. Observe the results.
2.  **Benchmarking:** Implement a simple benchmarking test to compare the performance of mutex-based and semaphore-based synchronization. Measure the execution time of a critical section or producer-consumer operation.
3.  **Troubleshooting:** Debug the code using Zephyr's tracing/logging system to identify and fix any synchronization issues.



## Additional Resources:

*   Zephyr RTOS Documentation: [https://docs.zephyrproject.org/](https://docs.zephyrproject.org/)
*   Zephyr Community Forum: [https://www.zephyrproject.org/community/](https://www.zephyrproject.org/community/)

This comprehensive chapter provides a solid foundation for understanding and applying traditional multithreading primitives in Zephyr RTOS. By working through the lab exercises and exploring the theoretical concepts, you’ll be well-equipped to tackle complex concurrency challenges in your embedded projects.
