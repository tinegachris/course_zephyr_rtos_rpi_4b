# Chapter 18: Networked Environmental Monitor - Implementation Lab

## Lab Overview

This lab will guide you through the complete implementation of the Networked Environmental Monitor project on the Raspberry Pi 4B. You will write the code for each component of the system, integrate them, and test the final application.

## Part 1: Project Setup

### Step 1: Create the Project Directory

First, create a new project directory for the capstone project:

```bash
mkdir -p networked_monitor/src
cd networked_monitor
```

### Step 2: Create the `CMakeLists.txt` file

Create a `CMakeLists.txt` file in the `networked_monitor` directory:

```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(networked_monitor)

target_sources(app PRIVATE 
    src/main.c
    src/sensor_manager.c
    src/web_server.c
    )

zephyr_include_directories(src)
```

### Step 3: Create the `prj.conf` file

Create a `prj.conf` file in the `networked_monitor` directory. This file will enable all the necessary Kconfig options for our project.

```kconfig
# Kernel
CONFIG_MULTITHREADING=y
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3

# I2C
CONFIG_I2C=y

# BME280 Sensor
CONFIG_SENSOR=y
CONFIG_BME280=y

# Networking
CONFIG_NETWORKING=y
CONFIG_NET_TCP=y
CONFIG_NET_SOCKETS=y
CONFIG_NET_CONFIG_SETTINGS=y
CONFIG_NET_CONFIG_IEEE802154_DEV_NAME="" # Disable 802.15.4
CONFIG_NET_CONFIG_MY_IPV4_ADDR="192.168.1.100"
CONFIG_NET_CONFIG_MY_IPV4_NETMASK="255.255.255.0"
CONFIG_NET_CONFIG_MY_IPV4_GW="192.168.1.1"

# Power Management
CONFIG_PM=y
CONFIG_PM_DEVICE=y

# Shell
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
```

## Part 2: Sensor Integration (BME280)

### Hardware Setup

Before you begin, you need to connect the BME280 sensor to your Raspberry Pi 4B. The BME280 sensor typically has four pins: VCC, GND, SCL, and SDA.

**Connections:**

| BME280 Pin | Raspberry Pi 4B Pin (Physical) | Raspberry Pi 4B GPIO |
|------------|----------------------------------|----------------------|
| VCC        | Pin 1 (3.3V Power)               |                      |
| GND        | Pin 6 (Ground)                   |                      |
| SCL        | Pin 5 (GPIO3)                    | I2C1_SCL             |
| SDA        | Pin 3 (GPIO2)                    | I2C1_SDA             |

**Note:** We are using the `i2c1` bus on the Raspberry Pi 4B, which corresponds to GPIO2 (SDA) and GPIO3 (SCL).

### Step 1: Add the BME280 to the Device Tree

Create a `boards` directory and add an overlay file for the Raspberry Pi 4B: `rpi_4b.overlay`.

`boards/rpi_4b.overlay`:
```dts
&i2c1 {
    status = "okay";
    bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
    };
};
```

### Step 2: Create the Sensor Manager

Create `src/sensor_manager.h`:
```c
#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_

struct sensor_reading {
    struct sensor_value temp;
    struct sensor_value press;
    struct sensor_value humidity;
};

void sensor_thread(void);

#endif /* SENSOR_MANAGER_H_ */
```

Create `src/sensor_manager.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(sensor_manager, LOG_LEVEL_INF);

extern struct sensor_reading latest_reading;
extern struct k_mutex data_mutex;

static const struct device *const bme280 = DEVICE_DT_GET_ANY(bosch_bme280);

void sensor_thread(void)
{
    struct sensor_reading data;

    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return;
    }

    while (1) {
        sensor_sample_fetch(bme280);
        sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &data.temp);
        sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &data.press);
        sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &data.humidity);

        if (k_mutex_lock(&data_mutex, K_MSEC(500)) == 0) {
            latest_reading = data;
            k_mutex_unlock(&data_mutex);
        }

        k_sleep(K_SECONDS(5));
    }
}
```

## Part 3: Web Server Implementation

### Step 1: Create the Web Server Manager

Create `src/web_server.h`:
```c
#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

void web_server_thread(void);

#endif /* WEB_SERVER_H_ */
```

Create `src/web_server.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(web_server, LOG_LEVEL_INF);

extern struct sensor_reading latest_reading;
extern struct k_mutex data_mutex;

#define WEB_SERVER_PORT 80

static void handle_client(int client)
{
    struct sensor_reading data;
    char response[256];

    // Get the latest sensor data
    if (k_mutex_lock(&data_mutex, K_MSEC(500)) == 0) {
        data = latest_reading;
        k_mutex_unlock(&data_mutex);
    } else {
        // Could not get lock, send error or old data
        const char *error_response = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
        send(client, error_response, strlen(error_response), 0);
        close(client);
        return;
    }

    // Create the HTTP response
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n\r\n"
             "<html><head><title>Weather Station</title></head>"
             "<body><h1>Weather Station</h1>"
             "<p>Temperature: %d.%06d C</p>"
             "<p>Pressure: %d.%06d kPa</p>"
             "<p>Humidity: %d.%06d %%</p>"
             "</body></html>",
             data.temp.val1, data.temp.val2,
             data.press.val1, data.press.val2,
             data.humidity.val1, data.humidity.val2);

    send(client, response, strlen(response), 0);
    close(client);
}

void web_server_thread(void)
{
    int server_sock;
    struct sockaddr_in addr;

    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(WEB_SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);

    LOG_INF("Web server listening on port %d", WEB_SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_sock >= 0) {
            handle_client(client_sock);
        }
    }
}
```

## Part 4: Main Application and Thread Creation

### Step 1: Write the `main.c` file

`src/main.c`:
```c
#include <zephyr/kernel.h>
#include "sensor_manager.h"
#include "web_server.h"

/* Shared data and mutex */
struct sensor_reading latest_reading;
K_MUTEX_DEFINE(data_mutex);

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(web_server_tid, 2048, web_server_thread, NULL, NULL, NULL, 7, 0, 0);

void main(void)
{
    /* The threads are started automatically by K_THREAD_DEFINE */
}
```


## Part 5: Building and Testing

### Step 1: Build

```bash
west build -b rpi_4b
```

### Step 2: Prepare the SD Card

1.  Format an SD card with a FAT32 filesystem.
2.  Copy the Zephyr kernel to the SD card and rename it to `kernel8.img`:
    ```bash
    cp build/zephyr/zephyr.bin /path/to/your/sdcard/kernel8.img
    ```
3.  Create a file named `config.txt` on the SD card with the following content:
    ```
    arm_64bit=1
    kernel=kernel8.img
    ```

### Step 3: Test

1.  Insert the SD card into your Raspberry Pi 4B and power it on.
2.  Connect the Raspberry Pi to your network via Ethernet.
3.  **Note on IP Address:** The project is configured with a static IP address (`192.168.1.100`). You may need to change this in `prj.conf` to match your local network configuration.
4.  From a web browser on the same network, navigate to the IP address of your Raspberry Pi (e.g., `http://192.168.1.100`).
5.  You should see the weather station web page with the latest sensor data.

## Conclusion

This lab has guided you through the creation of a complete, multi-threaded embedded application on the Raspberry Pi 4B. You have successfully integrated a sensor, implemented inter-thread communication, and set up a simple web server. This project serves as a solid foundation that you can extend with more advanced features.
