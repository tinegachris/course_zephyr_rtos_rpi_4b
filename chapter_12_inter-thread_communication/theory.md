# Chapter 12: Inter-Thread Communication

## Theory and Implementation

### 1. Message Queues: Structured Data Exchange

Message queues provide the fundamental building blocks for thread-safe data exchange in embedded systems. They implement a producer-consumer pattern where threads can send and receive structured data asynchronously, with built-in buffering and synchronization.

#### Core Concepts

**Message Queue Architecture**: Zephyr message queues store fixed-size messages in a circular buffer. Each queue has a defined capacity and message size, providing predictable memory usage and performance characteristics.

**Thread Safety**: All queue operations are atomic and thread-safe. Multiple threads can safely send and receive messages simultaneously without external synchronization.

**Blocking Behavior**: Operations support configurable timeouts, allowing threads to block indefinitely, timeout after a specified period, or return immediately if the operation cannot complete.

#### API Usage Patterns

```c
#include <zephyr/kernel.h>

// Define message structure
struct sensor_data {
    uint32_t timestamp;
    int16_t temperature;
    int16_t humidity;
    uint8_t sensor_id;
};

// Define message queue with 10 message capacity
K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_data), 10, 4);

// Producer thread - sensor reading
void sensor_thread(void *arg1, void *arg2, void *arg3)
{
    struct sensor_data reading;
    
    while (1) {
        // Simulate sensor reading
        reading.timestamp = k_uptime_get_32();
        reading.temperature = get_temperature();
        reading.humidity = get_humidity();
        reading.sensor_id = SENSOR_DHT22;
        
        // Send message with 100ms timeout
        int ret = k_msgq_put(&sensor_queue, &reading, K_MSEC(100));
        if (ret != 0) {
            printk("Failed to send sensor data: %d\n", ret);
        }
        
        k_msleep(1000); // Read every second
    }
}

// Consumer thread - data processing
void processing_thread(void *arg1, void *arg2, void *arg3)
{
    struct sensor_data data;
    
    while (1) {
        // Wait indefinitely for new data
        int ret = k_msgq_get(&sensor_queue, &data, K_FOREVER);
        if (ret == 0) {
            // Process the sensor data
            process_sensor_reading(&data);
            
            // Check queue status
            uint32_t used = k_msgq_num_used_get(&sensor_queue);
            if (used > 7) {
                printk("Warning: Queue nearly full (%u/10)\n", used);
            }
        }
    }
}
```

#### Advanced Queue Operations

**Queue Inspection**: Monitor queue state for performance analysis and debugging:

```c
void monitor_queue_health(void)
{
    uint32_t used = k_msgq_num_used_get(&sensor_queue);
    uint32_t free = k_msgq_num_free_get(&sensor_queue);
    
    printk("Queue status: %u used, %u free\n", used, free);
    
    if (used > 8) {
        // Queue nearly full - consider priority processing
        handle_queue_congestion();
    }
}
```

**Queue Purging**: Clear all pending messages when needed:

```c
void reset_sensor_pipeline(void)
{
    k_msgq_purge(&sensor_queue);
    printk("Sensor queue cleared\n");
}
```

### 2. FIFOs and LIFOs: Ordered Data Processing

FIFOs (First-In-First-Out) and LIFOs (Last-In-First-Out) provide lightweight mechanisms for passing pointers between threads, enabling efficient zero-copy data transfer for variable-sized objects.

#### FIFO Implementation

FIFOs maintain insertion order, making them ideal for streaming data and sequential processing:

```c
#include <zephyr/kernel.h>

// Data structure for FIFO items
struct data_packet {
    void *fifo_reserved;  // Required for FIFO linkage
    uint8_t *payload;
    size_t length;
    uint32_t sequence_num;
};

// Define FIFO
K_FIFO_DEFINE(packet_fifo);

// Memory pool for packets
K_MEM_SLAB_DEFINE(packet_slab, sizeof(struct data_packet), 20, 4);

// Producer thread
void network_rx_thread(void *arg1, void *arg2, void *arg3)
{
    struct data_packet *packet;
    static uint32_t seq_num = 0;
    
    while (1) {
        // Allocate packet from memory pool
        int ret = k_mem_slab_alloc(&packet_slab, (void **)&packet, K_MSEC(100));
        if (ret != 0) {
            printk("Failed to allocate packet\n");
            continue;
        }
        
        // Simulate receiving network data
        packet->payload = receive_network_data(&packet->length);
        packet->sequence_num = seq_num++;
        
        // Add to FIFO - never blocks
        k_fifo_put(&packet_fifo, packet);
        
        k_yield(); // Allow other threads to run
    }
}

// Consumer thread
void packet_processor_thread(void *arg1, void *arg2, void *arg3)
{
    struct data_packet *packet;
    
    while (1) {
        // Get next packet in order
        packet = k_fifo_get(&packet_fifo, K_FOREVER);
        
        // Process packet data
        process_packet_data(packet->payload, packet->length);
        
        // Free packet resources
        free_network_buffer(packet->payload);
        k_mem_slab_free(&packet_slab, (void *)packet);
    }
}
```

#### LIFO Implementation

LIFOs provide stack behavior, useful for undo operations and recursive processing:

```c
K_LIFO_DEFINE(task_lifo);

struct task_item {
    void *lifo_reserved;  // Required for LIFO linkage
    int task_type;
    void *task_data;
    k_timeout_t deadline;
};

// Add urgent task (will be processed first)
void add_urgent_task(int type, void *data)
{
    struct task_item *task = allocate_task_item();
    task->task_type = type;
    task->task_data = data;
    task->deadline = K_MSEC(100);
    
    k_lifo_put(&task_lifo, task);  // Goes to front of queue
}

// Task processor
void task_processor_thread(void *arg1, void *arg2, void *arg3)
{
    struct task_item *task;
    
    while (1) {
        task = k_lifo_get(&task_lifo, K_MSEC(50));
        if (task != NULL) {
            // Process most recent task first
            execute_task(task->task_type, task->task_data);
            free_task_item(task);
        } else {
            // No tasks - perform background maintenance
            system_maintenance();
        }
    }
}
```

### 3. Mailboxes: Complex Message Passing

Mailboxes provide sophisticated message passing with support for variable message sizes and complex delivery semantics. They're ideal for request-response patterns and inter-service communication.

#### Basic Mailbox Operations

```c
#include <zephyr/kernel.h>

// Define mailbox
K_MBOX_DEFINE(service_mbox);

// Message structure
struct service_request {
    uint32_t request_id;
    uint8_t service_type;
    void *request_data;
    size_t data_size;
    k_tid_t sender_tid;
};

struct service_response {
    uint32_t request_id;
    int status_code;
    void *response_data;
    size_t data_size;
};

// Client thread - send request
int send_service_request(uint8_t service, void *data, size_t size, 
                        struct service_response *response)
{
    static uint32_t req_id = 1;
    struct service_request request = {
        .request_id = req_id++,
        .service_type = service,
        .request_data = data,
        .data_size = size,
        .sender_tid = k_current_get()
    };
    
    struct k_mbox_msg send_msg = {
        .info = request.request_id,
        .size = sizeof(request),
        .tx_data = &request,
        .tx_block = K_MEM_POOL_DEFINE(req_pool, 16, 256, 8, 4),
        .tx_target_thread = K_ANY
    };
    
    // Send request
    int ret = k_mbox_put(&service_mbox, &send_msg, K_MSEC(1000));
    if (ret != 0) {
        return ret;
    }
    
    // Wait for response
    struct k_mbox_msg recv_msg = {
        .info = request.request_id,
        .size = sizeof(*response),
        .rx_source_thread = K_ANY
    };
    
    ret = k_mbox_get(&service_mbox, &recv_msg, response, K_MSEC(5000));
    return ret;
}

// Service thread - handle requests
void service_thread(void *arg1, void *arg2, void *arg3)
{
    struct k_mbox_msg msg;
    struct service_request request;
    struct service_response response;
    
    while (1) {
        // Wait for service request
        int ret = k_mbox_get(&service_mbox, &msg, &request, K_FOREVER);
        if (ret != 0) {
            continue;
        }
        
        // Process the request
        response.request_id = request.request_id;
        response.status_code = process_service_request(&request, &response);
        
        // Send response back to client
        struct k_mbox_msg reply_msg = {
            .info = response.request_id,
            .size = sizeof(response),
            .tx_data = &response,
            .tx_target_thread = request.sender_tid
        };
        
        k_mbox_put(&service_mbox, &reply_msg, K_NO_WAIT);
    }
}
```

### 4. Zephyr Bus (Zbus): Publish-Subscribe Architecture

Zbus provides a sophisticated publish-subscribe communication framework for building scalable, event-driven systems. It supports multiple observer types and enables loose coupling between system components.

#### Channel Definition and Management

```c
#include <zephyr/zbus/zbus.h>

// Define message structures
struct sensor_event {
    uint32_t timestamp;
    uint8_t sensor_id;
    float value;
    uint8_t status;
};

struct control_command {
    uint8_t device_id;
    uint8_t command_type;
    float parameter;
    uint32_t timeout_ms;
};

// Validation function for sensor data
bool sensor_data_validator(const void *msg, size_t msg_size)
{
    const struct sensor_event *event = (const struct sensor_event *)msg;
    return (event->sensor_id < MAX_SENSORS && 
            event->value >= MIN_SENSOR_VALUE && 
            event->value <= MAX_SENSOR_VALUE);
}

// Define channels with observers
ZBUS_CHAN_DEFINE(sensor_chan,           // Channel name
                struct sensor_event,    // Message type
                sensor_data_validator,  // Validator function
                NULL,                   // User data
                ZBUS_OBSERVERS(sensor_processor_sub, 
                              data_logger_sub,
                              alert_listener), // Observer list
                ZBUS_MSG_INIT(.timestamp = 0,
                             .sensor_id = 0,
                             .value = 0.0f,
                             .status = 0));

ZBUS_CHAN_DEFINE(control_chan,
                struct control_command,
                NULL,                   // No validator
                NULL,                   // No user data
                ZBUS_OBSERVERS(actuator_sub,
                              status_listener),
                ZBUS_MSG_INIT(.device_id = 0,
                             .command_type = 0,
                             .parameter = 0.0f,
                             .timeout_ms = 0));
```

#### Observer Implementation

**Listeners (Synchronous Observers)**:

```c
// Fast synchronous processing
void sensor_alert_callback(const struct zbus_channel *chan)
{
    if (chan == &sensor_chan) {
        const struct sensor_event *event = zbus_chan_const_msg(chan);
        
        // Quick alert processing - no blocking operations
        if (event->value > CRITICAL_THRESHOLD) {
            set_alert_led(true);
            trigger_emergency_shutdown();
        }
    }
}

ZBUS_LISTENER_DEFINE(alert_listener, sensor_alert_callback);

// Status monitoring listener
void system_status_callback(const struct zbus_channel *chan)
{
    static uint32_t event_count = 0;
    
    event_count++;
    
    if (chan == &sensor_chan) {
        update_sensor_status_display();
    } else if (chan == &control_chan) {
        update_control_status_display();
    }
    
    // Periodic health check
    if (event_count % 100 == 0) {
        perform_system_health_check();
    }
}

ZBUS_LISTENER_DEFINE(status_listener, system_status_callback);
```

**Subscribers (Asynchronous Observers)**:

```c
// Define subscribers with different priorities
ZBUS_SUBSCRIBER_DEFINE(sensor_processor_sub, 4);  // High priority
ZBUS_SUBSCRIBER_DEFINE(data_logger_sub, 6);       // Medium priority
ZBUS_SUBSCRIBER_DEFINE(actuator_sub, 3);          // Highest priority

// Sensor data processor thread
void sensor_processor_thread(void *arg1, void *arg2, void *arg3)
{
    const struct zbus_channel *chan;
    struct sensor_event event;
    
    while (1) {
        // Wait for sensor data
        int ret = zbus_sub_wait_msg(&sensor_processor_sub, &chan, 
                                   &event, K_FOREVER);
        if (ret != 0) {
            continue;
        }
        
        if (chan == &sensor_chan) {
            // Complex sensor data processing
            float processed_value = apply_sensor_calibration(event.value, 
                                                             event.sensor_id);
            
            // Apply filtering
            processed_value = low_pass_filter(processed_value, event.sensor_id);
            
            // Check for trends
            analyze_sensor_trends(event.sensor_id, processed_value);
            
            // Store processed data
            store_processed_sensor_data(event.sensor_id, processed_value, 
                                       event.timestamp);
        }
    }
}

// Data logging thread
void data_logger_thread(void *arg1, void *arg2, void *arg3)
{
    const struct zbus_channel *chan;
    
    while (1) {
        // Wait for any channel update
        int ret = zbus_sub_wait(&data_logger_sub, &chan, K_FOREVER);
        if (ret != 0) {
            continue;
        }
        
        // Log all channel activity
        uint32_t timestamp = k_uptime_get_32();
        
        if (chan == &sensor_chan) {
            const struct sensor_event *event = zbus_chan_const_msg(chan);
            log_sensor_event(timestamp, event);
        } else if (chan == &control_chan) {
            const struct control_command *cmd = zbus_chan_const_msg(chan);
            log_control_command(timestamp, cmd);
        }
    }
}

// Actuator control thread
void actuator_thread(void *arg1, void *arg2, void *arg3)
{
    const struct zbus_channel *chan;
    struct control_command cmd;
    
    while (1) {
        int ret = zbus_sub_wait_msg(&actuator_sub, &chan, &cmd, K_FOREVER);
        if (ret != 0) {
            continue;
        }
        
        if (chan == &control_chan) {
            // Execute control command
            execute_actuator_command(&cmd);
            
            // Provide feedback
            struct sensor_event feedback = {
                .timestamp = k_uptime_get_32(),
                .sensor_id = cmd.device_id + ACTUATOR_FEEDBACK_OFFSET,
                .value = get_actuator_position(cmd.device_id),
                .status = FEEDBACK_STATUS_OK
            };
            
            // Publish feedback to sensor channel
            zbus_chan_pub(&sensor_chan, &feedback, K_MSEC(100));
        }
    }
}
```

#### Publishing Data

```c
// Sensor reading thread
void sensor_reading_thread(void *arg1, void *arg2, void *arg3)
{
    struct sensor_event event;
    
    while (1) {
        // Read from multiple sensors
        for (int i = 0; i < NUM_SENSORS; i++) {
            event.timestamp = k_uptime_get_32();
            event.sensor_id = i;
            event.value = read_sensor_value(i);
            event.status = get_sensor_status(i);
            
            // Publish sensor data - triggers all observers
            int ret = zbus_chan_pub(&sensor_chan, &event, K_MSEC(10));
            if (ret != 0) {
                printk("Failed to publish sensor %d data: %d\n", i, ret);
            }
        }
        
        k_msleep(SENSOR_READING_INTERVAL_MS);
    }
}

// Control command generator
void control_logic_thread(void *arg1, void *arg2, void *arg3)
{
    struct control_command cmd;
    
    while (1) {
        // Wait for control trigger
        k_sem_take(&control_trigger_sem, K_FOREVER);
        
        // Generate control commands based on system state
        for (int device = 0; device < NUM_ACTUATORS; device++) {
            if (needs_control_update(device)) {
                cmd.device_id = device;
                cmd.command_type = calculate_command_type(device);
                cmd.parameter = calculate_command_parameter(device);
                cmd.timeout_ms = COMMAND_TIMEOUT_MS;
                
                // Publish control command
                int ret = zbus_chan_pub(&control_chan, &cmd, K_MSEC(50));
                if (ret != 0) {
                    printk("Failed to publish control command for device %d\n", 
                           device);
                }
            }
        }
    }
}
```

#### Runtime Observer Management

```c
// Runtime observer registration
int register_runtime_observer(void)
{
    // Create runtime observer
    ZBUS_LISTENER_DEFINE(runtime_listener, runtime_callback);
    
    // Add to existing channel
    int ret = zbus_chan_add_obs(&sensor_chan, &runtime_listener, K_MSEC(100));
    if (ret != 0) {
        printk("Failed to add runtime observer: %d\n", ret);
        return ret;
    }
    
    printk("Runtime observer registered successfully\n");
    return 0;
}

// Runtime observer removal
int unregister_runtime_observer(void)
{
    int ret = zbus_chan_rm_obs(&sensor_chan, &runtime_listener, K_MSEC(100));
    if (ret != 0) {
        printk("Failed to remove runtime observer: %d\n", ret);
        return ret;
    }
    
    printk("Runtime observer unregistered successfully\n");
    return 0;
}
```

### 5. Pipes: Streaming Data Communication

Pipes provide efficient streaming data transfer between threads, supporting variable-size data blocks and flow control. They're ideal for data streaming, buffering between threads with different processing rates, and implementing data pipelines.

#### Basic Pipe Operations

```c
#include <zephyr/kernel.h>

// Define pipe with 1KB buffer
K_PIPE_DEFINE(data_pipe, 1024, 4);

// Producer thread - streaming data generator
void data_producer_thread(void *arg1, void *arg2, void *arg3)
{
    uint8_t buffer[256];
    size_t bytes_written;
    
    while (1) {
        // Generate streaming data
        size_t data_size = generate_stream_data(buffer, sizeof(buffer));
        
        // Write to pipe - may write partial data
        int ret = k_pipe_put(&data_pipe, buffer, data_size, 
                           &bytes_written, 1, K_MSEC(100));
        
        if (ret == 0) {
            printk("Wrote %zu bytes to pipe\n", bytes_written);
        } else if (ret == -EAGAIN) {
            // Pipe full - apply backpressure
            k_msleep(10);
        } else {
            printk("Pipe write error: %d\n", ret);
        }
        
        k_yield();
    }
}

// Consumer thread - streaming data processor
void data_consumer_thread(void *arg1, void *arg2, void *arg3)
{
    uint8_t buffer[512];
    size_t bytes_read;
    
    while (1) {
        // Read available data from pipe
        int ret = k_pipe_get(&data_pipe, buffer, sizeof(buffer),
                           &bytes_read, 1, K_MSEC(1000));
        
        if (ret == 0 && bytes_read > 0) {
            // Process received data
            process_stream_data(buffer, bytes_read);
            printk("Processed %zu bytes from pipe\n", bytes_read);
        } else if (ret == -EAGAIN) {
            // No data available
            perform_idle_tasks();
        } else {
            printk("Pipe read error: %d\n", ret);
        }
    }
}
```

#### Advanced Pipe Usage

**Flow Control and Buffering**:

```c
// Monitoring pipe utilization
void monitor_pipe_health(void)
{
    size_t bytes_used, bytes_available;
    
    k_pipe_read_avail(&data_pipe, &bytes_available);
    k_pipe_write_avail(&data_pipe, &bytes_used);
    
    float utilization = (float)bytes_used / (bytes_used + bytes_available) * 100;
    
    printk("Pipe utilization: %.1f%% (%zu used, %zu free)\n",
           utilization, bytes_used, bytes_available);
    
    if (utilization > 80.0f) {
        // High utilization - consider flow control
        enable_flow_control();
    }
}

// Flush pipe contents
void reset_data_pipeline(void)
{
    k_pipe_flush(&data_pipe);
    printk("Data pipeline flushed\n");
}
```

### 6. Communication Mechanism Selection Guide

Choosing the right communication mechanism depends on your specific requirements:

#### Decision Matrix

| Mechanism | Data Size | Ordering | Overhead | Use Case |
|-----------|-----------|----------|----------|----------|
| Message Queue | Fixed | FIFO | Medium | Structured messages |
| FIFO | Variable | FIFO | Low | Sequential processing |
| LIFO | Variable | LIFO | Low | Stack operations |
| Mailbox | Variable | None | High | Complex messaging |
| Zbus | Variable | Observer | Medium | Event-driven systems |
| Pipe | Stream | FIFO | Low | Data streaming |

#### Selection Criteria

**For Simple Data Exchange**:

- Use **Message Queues** for fixed-size structured data
- Use **FIFOs** for variable-size data with ordering requirements

**For Complex Communication**:

- Use **Mailboxes** for request-response patterns
- Use **Zbus** for publish-subscribe architectures

**For Streaming Data**:

- Use **Pipes** for continuous data streams
- Use **FIFOs** for discrete data packets

**Performance Considerations**:

- **Lowest Overhead**: FIFOs, LIFOs, Pipes
- **Medium Overhead**: Message Queues, Zbus
- **Highest Overhead**: Mailboxes

**Memory Usage**:

- **Fixed Memory**: Message Queues
- **Dynamic Memory**: FIFOs, LIFOs, Mailboxes
- **Configurable**: Pipes, Zbus

This comprehensive coverage of Zephyr's inter-thread communication mechanisms provides the foundation for building sophisticated embedded systems. The next section will demonstrate these concepts through practical laboratory exercises.