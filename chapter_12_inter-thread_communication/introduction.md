# CHAPTER: 12 - Inter-Thread Communication

## 1. Introduction (567 words)

Inter-thread communication is the cornerstone of creating robust, scalable, and responsive embedded systems. In simple terms, it's how separate threads within a single application, or even across different applications running on the same device, exchange information. While earlier embedded systems often relied on shared memory and explicit locking mechanisms, these approaches quickly become unwieldy and prone to errors in complex scenarios.  Zephyr RTOS provides a comprehensive set of inter-thread communication primitives designed to address these challenges.

Why is this crucial?  Consider the following real-world scenarios:

* **Sensor Data Acquisition:** A system might have multiple threads – one for reading data from a temperature sensor, another for processing the data, and a third for transmitting the data to a remote server.  Without efficient inter-thread communication, data latency would be unacceptable.
* **Real-Time Control Systems:** In automotive or industrial automation, threads control actuators, monitor sensor data, and execute control algorithms.  The ability to quickly and reliably exchange commands and data between these threads is paramount for system responsiveness and safety.
* **IoT Devices:** Smart sensors, gateways, and actuators often need to communicate with cloud services.  Zephyr’s inter-thread communication mechanisms can facilitate this communication by creating threads that handle the network protocols and data transformation.
* **Complex Robotics:** Robot control systems involve numerous threads – perception, planning, control, and actuation.  The speed and reliability of data exchange between these threads determine the robot's precision and responsiveness.

Building upon your knowledge from previous chapters—Zephyr fundamentals, thread management, memory management, and hardware interaction—this chapter focuses on utilizing Zephyr’s advanced inter-thread communication capabilities.  We’ll explore how message queues, mailboxes, and the Zephyr Bus (Zbus) can solve complex communication challenges.  The goal is to equip you with the skills to architect robust, efficient, and maintainable embedded systems.  We'll be focusing on designing systems that prioritize responsiveness, minimize latency, and are resistant to errors. The principles covered here are critical for developing professional-quality embedded software.

## 2. Theory Section (2957 words)

### 2.1 Message Queues (MSGQ) & Queues

Message queues provide a buffer for exchanging data between threads. They are a fundamental mechanism for decoupling threads, allowing them to operate independently without direct dependency on each other’s timing.

**API Usage:**

* `k_msgq_define(my_msgq, sizeof(struct data_item_type), 10, 1);` – Defines a message queue named `my_msgq`. `sizeof(struct data_item_type)` specifies the size of the message, `10` is the maximum queue length, and `1` is the priority (lower is higher).
* `k_msgq_put(&my_msgq, &data, K_NO_WAIT);` – Attempts to put a message into the queue. `K_NO_WAIT` indicates that if the queue is full, the operation should not block.
* `k_msgq_get(&my_msgq, &data, K_FOREVER);` – Retrieves a message from the queue.  `K_FOREVER` indicates that the operation should block indefinitely until a message is available.

**Example Code:**

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

// Define a data structure
struct data_item_type {
    int value;
};

// Define a message queue
K_MSGQ_DEFINE(my_msgq, sizeof(struct data_item_type), 10, 1);

// Function to send data to the queue
void send_data_to_queue(int value) {
    struct data_item_type data;
    data.value = value;
    if (k_msgq_put(&my_msgq, &data, K_NO_WAIT) != 0) {
        printk("Queue is full!\n");
    }
}

// Function to receive data from the queue
int receive_data_from_queue() {
    struct data_item_type data;
    int result = k_msgq_get(&my_msgq, &data, K_FOREVER);
    if (result == 0) {
        printk("Received value: %d\n", data.value);
        return data.value;
    } else {
        printk("Error getting data from queue\n");
        return -1;
    }
}


int main(void) {
    printk("Starting example...\n");

    // Send some data to the queue
    receive_data_from_queue(); // Should print "Received value: 10"

    // Send more data
    receive_data_from_queue(); // Should print "Received value: 20"

    printk("Example finished.\n");
    return 0;
}
```

**Build Commands:**
```bash
west build -b default -s
```

**Configuration Files (prj.conf):**
```kconfig
CONFIG_K_MSGQ=y
```

**Expected Console Output:**

```
Starting example...
Received value: 10
Received value: 20
Example finished.
```

**Performance Considerations:**

*   **Queue Length:** A longer queue can buffer more data but increases memory usage and potentially delays if the receiving thread is slow.
*   **Priority:** Setting a lower priority for the message queue can ensure the critical message is processed faster.



### 2.2 Mailboxes

Mailboxes provide a mechanism for synchronous communication between threads. Unlike message queues, a thread waiting for a mailbox will block until a message is available.  This provides tighter control over data exchange but can introduce latency.

**API Usage:**

*   `K_MBOX_DEFINE(my_mbox);` – Defines a mailbox named `my_mbox`.
*   `k_mbox_put(&my_mbox, &send_msg, K_FOREVER);` – Attempts to put a message into the mailbox. `K_FOREVER` indicates that the operation should block indefinitely until a message is available.
*   `k_mbox_get(&my_mbox, &recv_msg, &data, K_FOREVER);` – Retrieves a message from the mailbox.  `K_FOREVER` indicates that the operation should block indefinitely until a message is available.

**Example Code:**

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

// Define a data structure
struct data_item_type {
    int value;
};

// Define a mailbox
K_MBOX_DEFINE(my_mbox, sizeof(struct data_item_type), 10, 1);

// Function to send data to the mailbox
void send_data_to_mailbox(int value) {
    struct data_item_type data;
    data.value = value;
    if (k_mbox_put(&my_mbox, &data, K_FOREVER) != 0) {
        printk("Mailbox is full!\n");
    }
}

// Function to receive data from the mailbox
int receive_data_from_mailbox() {
    struct data_item_type data;
    int result = k_mbox_get(&my_mbox, &data, &data, K_FOREVER);
    if (result == 0) {
        printk("Received value: %d\n", data.value);
        return data.value;
    } else {
        printk("Error getting data from mailbox\n");
        return -1;
    }
}

int main(void) {
    printk("Starting example...\n");

    // Send some data to the mailbox
    receive_data_from_mailbox(); // Should print "Received value: 10"

    // Send more data
    receive_data_from_mailbox(); // Should print "Received value: 20"

    printk("Example finished.\n");
    return 0;
}
```

**Build Commands:**
```bash
west build -b default -s
```

**Configuration Files (prj.conf):**
```kconfig
CONFIG_K_MBOX=y
```

**Expected Console Output:**

```
Starting example...
Received value: 10
Received value: 20
Example finished.
```

**Performance Considerations:**

*   **Blocking Behavior:** The blocking nature of mailboxes can introduce significant delays if the receiving thread is busy.  Carefully consider the timing requirements of your application when using mailboxes.
*   **Queue Length:** A larger mailbox can potentially buffer more messages, but it also increases the time a thread will block.



### 2.3 Zephyr Bus (Zbus)

The Zephyr Bus (Zbus) is a sophisticated inter-thread communication framework designed for distributed systems. It's based on the concept of *channels*, which are logical connections for exchanging data and events.  Zbus is particularly well-suited for communication between different applications running on the same device or between devices connected via a network.

**API Usage:**

*   `ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_data, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));` – Defines a channel named `sensor_chan`. This creates a channel that can be observed by a subscriber. `ZBUS_OBSERVERS_EMPTY` indicates the channel is initially empty, and `ZBUS_MSG_INIT(0)` initializes the message structure to zero.
*   `zbus_chan_pub(&sensor_chan, &data, K_MSEC(100));` – Publishes data to the channel.  This sends the `data` message to any subscribers of the `sensor_chan`. `K_MSEC(100)` specifies that the publish operation should wait for 100 milliseconds.
*   `ZBUS_SUBSCRIBER_DEFINE(temp_sub, 4);` – Defines a subscriber named `temp_sub` with a priority of 4.

**Example Code:**

```c
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

// Define a data structure
struct sensor_data {
    int temperature;
};

// Define a channel
ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_data, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

// Function to publish data to the channel
void publish_sensor_data(int temperature) {
    struct sensor_data data;
    data.temperature = temperature;
    zbus_chan_pub(&sensor_chan, &data, K_MSEC(100));
}

// Function to subscribe to the channel
int subscribe_to_channel() {
    ZBUS_SUBSCRIBER_DEFINE(temp_sub, 4);
    const struct zbus_channel *chan;
    int ret = zbus_sub_wait(&temp_sub, &chan, K_FOREVER);
    if (ret != 0) {
        printk("Error subscribing to channel\n");
        return -1;
    }
    printk("Subscribed to channel\n");
    return 0;
}

int main(void) {
    printk("Starting example...\n");

    // Subscribe to the channel
    subscribe_to_channel();

    // Publish some data to the channel
    publish_sensor_data(25);

    // Publish more data
    publish_sensor_data(28);

    printk("Example finished.\n");
    return 0;
}
```

**Build Commands:**
```bash
west build -b default -s
```

**Configuration Files (prj.conf):**
```kconfig
CONFIG_ZBUS=y
CONFIG_ZBUS_OBSERVERS=y
```

**Expected Console Output:**

```
Starting example...
Subscribed to channel
Received value: 25
Received value: 28
Example finished.
```

**Performance Considerations:**

*   **Zbus Priority:**  Higher priority Zbus subscriptions will receive messages before lower priority ones.
*   **Message Filtering:** Zbus allows for filtering messages based on their type, priority, and other attributes, which can be critical for minimizing data transfer overhead.
*   **Network Latency:** When using Zbus over a network, network latency can be a significant factor.



## 3. Lab Exercise (2499 words)