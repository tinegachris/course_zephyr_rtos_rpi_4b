# Chapter 18: Capstone Project - Networked Environmental Monitor

---
[Introduction](./README.md) | [Theory](./theory.md) | [Lab](./lab.md) | [Course Home](../index.md)

---

## Integrating Your Knowledge

This capstone project is the culmination of all the concepts you have mastered throughout this course. You will apply your knowledge of device drivers, threading, inter-thread communication, power management, and modular design to build a complete, real-world embedded application: a Networked Environmental Monitor.

## Project Overview

The goal of this project is to build a Raspberry Pi 4B-based weather station that collects environmental data and serves it over a web interface. This project is designed to be a comprehensive exercise that mirrors the development process of a professional embedded product.

### Core Features:

*   **Sensor Integration:** Read temperature, humidity, and barometric pressure from a BME280 sensor connected to the Raspberry Pi 4B's I2C bus.
*   **Networking:** Host a simple web server that displays the current sensor readings.
*   **Power Management:** Implement power-saving strategies suitable for a mains-powered device, such as CPU frequency scaling and disabling unused peripherals.
*   **Multi-Threaded Architecture:** Utilize a robust multi-threaded design to handle sensor readings, the web server, and system management concurrently.
*   **Interactive Shell:** Provide a command-line interface for configuration, diagnostics, and status monitoring.
*   **Modular Design:** Structure the application into logical, reusable modules for clean and maintainable code.
*   **Configurability:** Use Kconfig to allow for compile-time configuration of key parameters, such as the sensor polling interval.

### Learning Objectives:

By completing this capstone project, you will demonstrate your ability to:

*   **Apply Device Driver Architecture (Chapter 16):** Integrate and use a real-world sensor driver on a Linux-capable board.
*   **Design a Multi-Threaded Application (Chapter 7):** Create and manage multiple threads for concurrent tasks.
*   **Use Inter-Thread Communication (Chapter 12):** Implement message queues for safe and efficient data exchange between threads.
*   **Implement Power Management (Chapter 17):** Apply power management concepts to a mains-powered device.
*   **Work with a Networking Stack:** Gain experience with Zephyr's networking stack by implementing a simple web server.
*   **Build a Modular and Configurable Application (Chapters 14 & 15):** Use your knowledge of modules and Kconfig to create a professional and maintainable project structure.
*   **Integrate Multiple Subsystems:** Combine all the different parts of the Zephyr RTOS into a single, cohesive application.

### Hardware Requirements:

*   A Raspberry Pi 4B.
*   A BME280 sensor breakout board.
*   A breadboard and jumper wires.
*   An SD card for the Zephyr image.
*   An Ethernet cable for network connectivity.

This project will challenge you to think like an embedded systems architect, making design decisions and trade-offs to meet the requirements of a real-world product. Let's get started with the [theory section](./theory.md) to understand the architectural design of the Networked Environmental Monitor.

[Next: Architectural Design](./theory.md)
