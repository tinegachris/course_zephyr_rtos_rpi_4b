# Chapter 12: Inter-Thread Communication

## Laboratory Exercises

### Lab Overview

This comprehensive laboratory explores Zephyr's inter-thread communication mechanisms through progressively complex exercises. You'll implement real-world communication patterns, analyze performance characteristics, and design robust distributed systems using message queues, FIFOs, mailboxes, Zbus, and pipes.

### Prerequisites

- Zephyr development environment configured
- Understanding of thread management and synchronization
- Basic knowledge of embedded system architecture
- Familiarity with real-time system constraints

### Lab 1: Message Queue Implementation (45 minutes)

**Objective**: Implement a sensor data collection system using message queues for structured inter-thread communication.

#### Setup

Create a new Zephyr project with the following structure:
```
sensor_system/
├── CMakeLists.txt
├── prj.conf
├── src/
│   └── main.c
└── boards/
    └── native_posix.conf
```

#### Implementation Steps

**Step 1**: Configure the project

Create `prj.conf`:
```ini
CONFIG_MULTITHREADING=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_PRINTK=y
CONFIG_ASSERT=y
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_NAME=y
```

Create `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_system)

target_sources(app PRIVATE src/main.c)
```

**Step 2**: Implement the sensor data system

Create `src/main.c`:

```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(sensor_system, LOG_LEVEL_INF);

/* Sensor data structure */
struct sensor_reading {
    uint32_t timestamp;
    uint8_t sensor_id;
    int16_t temperature;
    uint16_t humidity;
    uint8_t status;
};

/* Define message queue for sensor data */
K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_reading), 20, 4);

/* Thread stacks */
K_THREAD_STACK_DEFINE(sensor_thread_stack, 1024);
K_THREAD_STACK_DEFINE(processor_thread_stack, 1024);
K_THREAD_STACK_DEFINE(monitor_thread_stack, 512);

/* Thread control blocks */
struct k_thread sensor_thread_data;
struct k_thread processor_thread_data;
struct k_thread monitor_thread_data;

/* Statistics */
static uint32_t readings_generated = 0;
static uint32_t readings_processed = 0;
static uint32_t queue_overruns = 0;

/* Sensor thread - data producer */
void sensor_thread_entry(void *arg1, void *arg2, void *arg3)
{
    struct sensor_reading reading;
    uint8_t sensor_count = 3;
    
    LOG_INF("Sensor thread started");
    
    while (1) {
        for (uint8_t i = 0; i < sensor_count; i++) {
            /* Generate sensor reading */
            reading.timestamp = k_uptime_get_32();
            reading.sensor_id = i;
            reading.temperature = (int16_t)(sys_rand32_get() % 500) + 200; /* 20.0°C to 70.0°C */
            reading.humidity = (uint16_t)(sys_rand32_get() % 1000); /* 0% to 100% */
            reading.status = (sys_rand32_get() % 100) < 95 ? 0 : 1; /* 95% good readings */
            
            /* Send to queue with timeout */
            int ret = k_msgq_put(&sensor_queue, &reading, K_MSEC(100));
            if (ret == 0) {
                readings_generated++;
                LOG_DBG("Sensor %d: T=%d.%d°C, H=%d.%d%%, Status=%s",
                       reading.sensor_id,
                       reading.temperature / 10, reading.temperature % 10,
                       reading.humidity / 10, reading.humidity % 10,
                       reading.status == 0 ? "OK" : "ERROR");
            } else {
                queue_overruns++;
                LOG_WRN("Queue full, reading dropped (sensor %d)", i);
            }
        }
        
        /* Sensor reading interval */
        k_msleep(1000);
    }
}
```

#### Testing and Analysis

**Build and Run**:

```bash
west build -p -b native_posix
west build -t run
```

**Expected Output**:

- Sensor readings generated every second
- Real-time processing with statistics
- Queue utilization monitoring
- Alert generation for extreme values

**Exercise Questions**:

1. **Queue Sizing**: Modify the queue size to 5 messages. What happens to the overrun rate?

2. **Processing Delays**: Add a `k_msleep(2000)` in the processor thread. How does this affect system performance?

3. **Priority Impact**: Change the processor thread priority to 4 (higher than sensor). Does this improve efficiency?

4. **Timeout Analysis**: Change the sensor thread timeout from `K_MSEC(100)` to `K_NO_WAIT`. What's the trade-off?

### Lab 2: FIFO-based Pipeline Processing (40 minutes)

**Objective**: Implement a data processing pipeline using FIFOs for zero-copy data transfer.

#### Implementation

Create a new project directory `pipeline_system/` and implement a multi-stage processing pipeline with FIFOs connecting each stage.

**Key Features**:

- Data generator creates variable-size packets
- Preprocessing stage applies filtering algorithms
- Main processor performs complex computations
- Output stage handles results with performance monitoring

#### Analysis Exercises

1. **Pipeline Efficiency**: Analyze the throughput of each stage. Which stage becomes the bottleneck?

2. **Memory Management**: Monitor heap and slab utilization. How does packet size affect memory usage?

3. **Deadline Monitoring**: Implement deadline tracking. What happens when processing load increases?

### Lab 3: Zbus Event-Driven System (50 minutes)

**Objective**: Design a comprehensive IoT sensor system using Zbus for publish-subscribe communication.

#### System Architecture

Create an IoT monitoring system with:

- Multiple sensor publishers
- Data processing subscribers
- Alert management listeners
- Configuration management
- Real-time dashboard updates

#### Advanced Exercises

1. **Dynamic Observer Registration**: Implement runtime addition and removal of observers based on system conditions.

2. **Message Validation**: Create comprehensive validators that check message integrity and business logic.

3. **Performance Monitoring**: Add Zbus channel statistics tracking and performance analysis.

4. **Fault Tolerance**: Implement error handling and recovery mechanisms for channel communication failures.

### Lab 4: Performance Analysis and Optimization (30 minutes)

**Objective**: Analyze and optimize inter-thread communication performance across different mechanisms.

#### Benchmark Implementation

Create a performance testing framework that compares:

- Message queue latency and throughput
- FIFO zero-copy performance
- Zbus publication and subscription overhead
- Memory usage patterns
- Real-time characteristics

#### Analysis Questions

1. **Performance Comparison**: Which mechanism provides the lowest latency? Why?

2. **Memory Efficiency**: Compare static vs dynamic memory usage patterns.

3. **Scalability**: How does performance change with different message sizes and queue depths?

4. **Real-time Characteristics**: Which mechanisms provide the most predictable timing?

### Lab Summary and Assessment

#### Key Learning Outcomes

After completing these laboratories, you should understand:

1. **Message Queue Design**: Implementing producer-consumer patterns with proper queue sizing and timeout handling

2. **Zero-Copy Communication**: Using FIFOs and LIFOs for efficient data transfer without memory copying

3. **Event-Driven Architecture**: Building scalable systems using Zbus publish-subscribe patterns

4. **Performance Optimization**: Analyzing and optimizing communication performance for real-time requirements

5. **System Integration**: Combining multiple communication mechanisms in complex embedded systems

#### Extension Exercises

1. **Hybrid Communication**: Design a system that uses multiple communication mechanisms together

2. **Fault Tolerance**: Implement error detection and recovery for communication failures

3. **Load Balancing**: Create systems that dynamically balance processing load across multiple threads

4. **Protocol Implementation**: Use communication primitives to implement custom communication protocols

5. **Real-time Analysis**: Perform detailed timing analysis and worst-case execution time calculations

This comprehensive laboratory experience provides practical skills for implementing professional-grade inter-thread communication in embedded systems using Zephyr RTOS.

This comprehensive laboratory experience provides practical skills for implementing professional-grade inter-thread communication in embedded systems using Zephyr RTOS.