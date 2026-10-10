# Smart Lock — Real-Time Embedded System with FreeRTOS

A real-time smart locking system developed with FreeRTOS and ESP-IDF, featuring PIN-based access control, servo actuation, emergency handling, event logging, and runtime performance monitoring.

The project focuses on concurrent task design, event-driven control, resource synchronization, and real-time performance analysis on a resource-constrained microcontroller.

---

## Overview

This project implements an embedded access-control system based on a finite state machine (FSM) and concurrent FreeRTOS tasks.

The system integrates a matrix keypad for PIN entry, a servo motor for lock actuation, an LCD for user feedback, an RTC for timekeeping, and an SD card for event logging.

An emergency input provides a dedicated event-handling path, while runtime monitoring allows task execution and system responsiveness to be analyzed.

Beyond implementing the locking functionality, the project explores the design of a multitasking embedded application, coordination between shared peripherals, task prioritization, and performance measurement.

---

## Key Features

- **PIN-based access control:** PIN entry, confirmation, deletion, and access validation.
- **Finite State Machine (FSM):** Centralized application logic and event-driven state transitions.
- **FreeRTOS multitasking:** Separation of input acquisition, control logic, actuation, display, timekeeping, logging, and monitoring.
- **Emergency handling:** GPIO interrupt signaling and dedicated panic-event processing.
- **Servo actuation:** Motor commands coordinated with the application state.
- **LCD interface:** System status and user feedback.
- **RTC integration:** Time acquisition through the I²C interface.
- **Event logging:** SD card communication through SPI.
- **Concurrency management:** Queues, mutexes, and semaphores for inter-task communication and resource protection.
- **Performance monitoring:** Analysis of response latency, task jitter, CPU utilization, stack usage, and queue occupancy.

---

## System Architecture

The application follows an event-driven architecture in which the FSM coordinates the main control logic and communicates with peripheral-related tasks.

```text
                  +-------------------+
                  |   Matrix Keypad   |
                  +---------+---------+
                            |
                            v
                  +-------------------+
                  |   Keyboard Tasks  |
                  +---------+---------+
                            |
                            v
                  +-------------------+
                  |    Event Queue    |
                  +---------+---------+
                            |
                            v
                  +-------------------+
                  | Finite State      |
                  | Machine (FSM)     |
                  +----+----+----+----+
                       |    |    |
              +--------+    |    +---------+
              v             v              v
          +-------+     +-------+      +---------+
          | Servo |     |  LCD  |      | SD Card |
          +-------+     +-------+      +---------+

     +-----------+             +-------------------+
     | RTC / I²C |------------>| Time and Display  |
     +-----------+             +-------------------+

     +----------------+        +-------------------+
     | Emergency GPIO |------->| Panic Task / FSM  |
     +----------------+        +-------------------+
