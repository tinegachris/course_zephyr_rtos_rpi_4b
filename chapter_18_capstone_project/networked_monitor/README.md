# Capstone Project: Networked Weather Station

This project is a networked weather station that reads temperature, pressure, and humidity from a BME280 sensor and makes the data available via a web server.

## Functionality

- Reads sensor data from a BME280 sensor for temperature, pressure, and humidity.
- Creates two threads: one for reading sensor data and another for running a web server.
- Uses a mutex to protect shared sensor data.
- The web server listens on port 80 and serves an HTML page with the latest sensor readings.

## Hardware Requirements

- Raspberry Pi 4B
- BME280 sensor connected to the I2C bus

## How to Build and Run

1.  **Navigate to the project directory:**

    ```sh
    cd chapter_18_capstone_project/networked_monitor
    ```

2.  **Build the project:**

    ```sh
    west build -b rpi_4b
    ```

3.  **Flash the application to the Raspberry Pi 4B.**

4.  **Connect to the device over your network.** The device will have the static IP address `192.168.1.100`.

5.  **Open a web browser and navigate to `http://192.168.1.100`** to see the latest sensor readings.
